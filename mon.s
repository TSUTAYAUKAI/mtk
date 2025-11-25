***************************************************************
** プログラム仕様
** ・UART1経由で受信した文字列をシステムコール(GETSTRING/PUTSTRING)
**   を通じて同じチャンネルへ即座にエコーバックする。
** ・周期タイマ割り込みでタスクTTを呼び出し、"******"を最大5回表示
**   した後はRESET_TIMERでタイマを停止する。
** ・送受信データはリングバッファを持つソフトウェアキューで管理し、
**   UART割り込みハンドラとシステムコール処理の間で排他制御を行う。
**
** 主要サブルーチン
** ・boot: スーパー・バイザ初期化、デバイス/割り込み設定、ユーザ起動。
** ・MAIN: ユーザモードへ移行後、タイマをセットしGET/PUTSTRINGで
**   入力文字列のエコーバックを繰り返すメインループ。
** ・TT: タイマ割り込みで呼び出され、"******"出力とタイマ停止判定を行う。
** ・check_send_interrupt/check_receive_interrupt: UART1送受信割り込み入口で、
**   送信完了時のキュー取り出しと受信データの取得を振り分ける。
** ・INTERPUT/INTERGET: UART送受信割り込み本体。キューからの送信データ供給と、
**   受信データのバッファ投入を行う。
** ・PUTSTRING/GETSTRING: ユーザモードから呼ばれるシステムコール処理で、
**   指定サイズの送受信処理をソフトキュー経由で実施する。
** ・systemcall_if: trap #0 入口。システムコール番号に応じて各処理へ分岐。
** ・Q_INIT/INIT_SINGLE_Q/INQ/OUTQ/PUT_BUF/GET_BUF: 2本のリングバッファキューの
**   初期化と入出力を担当し、割り込み禁止で排他制御しながらバッファ操作する。
***************************************************************

***************************************************************
**各種レジスタ定義
***************************************************************
***************
**レジスタ群の先頭
***************
.equ REGBASE,   0xFFF000          | DMAPを使用．
.equ IOBASE,    0x00d00000
***************
**割り込み関係のレジスタ
***************
.equ IVR,       REGBASE+0x300     |割り込みベクタレジスタ
.equ IMR,       REGBASE+0x304     |割り込みマスクレジスタ
.equ ISR,       REGBASE+0x30c     |割り込みステータスレジスタ
.equ IPR,       REGBASE+0x310     |割り込みペンディングレジスタ
***************
**タイマ関係のレジスタ
***************
.equ TCTL1,     REGBASE+0x600     |タイマ１コントロールレジスタ
.equ TPRER1,    REGBASE+0x602     |タイマ１プリスケーラレジスタ
.equ TCMP1,     REGBASE+0x604     |タイマ１コンペアレジスタ
.equ TCN1,      REGBASE+0x608     |タイマ１カウンタレジスタ
.equ TSTAT1,    REGBASE+0x60a     |タイマ１ステータスレジスタ
***************
** UART1（送受信）関係のレジスタ
***************
.equ USTCNT1,   REGBASE+0x900     | UART1ステータス/コントロールレジスタ
.equ UBAUD1,    REGBASE+0x902     | UART1ボーコントロールレジスタ
.equ URX1,      REGBASE+0x904     | UART1受信レジスタ
.equ UTX1,      REGBASE+0x906     | UART1送信レジスタ
***************
** LED
***************
.equ LED7,      IOBASE+0x000002f  |ボード搭載のLED用レジスタ
.equ LED6,      IOBASE+0x000002d  |使用法については付録A.4.3.1
.equ LED5,      IOBASE+0x000002b
.equ LED4,      IOBASE+0x0000029
.equ LED3,      IOBASE+0x000003f
.equ LED2,      IOBASE+0x000003d
.equ LED1,      IOBASE+0x000003b
.equ LED0,      IOBASE+0x0000039

*************************
**システムコール番号
*************************
.equ SYSCALL_NUM_GETSTRING,    1
.equ SYSCALL_NUM_PUTSTRING,    2
.equ SYSCALL_NUM_RESET_TIMER,  3
.equ SYSCALL_NUM_SET_TIMER,    4
	
***************************************************************
**スタック領域の確保
***************************************************************
.section .bss
.even
SYS_STK:
	.ds.b   0x4000  |システムスタック領域
	.even
SYS_STK_TOP:            |システムスタック領域の最後尾

task_p:
	.ds.l 1		|SET_TIMERで登録されるタスクポインタ

***************************************************************
**初期化 作成者:鵜飼, 平川, 松下 レビュー:平野, 札場
***************************************************************
.section .text
.even

.extern start
.global monitor_begin
monitor_begin:
	*スーパーバイザ&各種設定を行っている最中の割込禁止
	move.w #0x2700,%SR
	lea.l  SYS_STK_TOP, %SP | Set SSP


	****************
	**割り込みコントローラの初期化
	****************
	move.b #0x40, IVR       |ユーザ割り込みベクタ番号を
				| 0x40+levelに設定.
	move.l #0x00ffffff, IMR |全割り込みマスク|

	****************
	**送受信(UART1)関係の初期化(割り込みレベルは4に固定されている)
	****************
	move.w #0x0000, USTCNT1 |リセット
	move.w #0xe10c, USTCNT1 |送受信可能,パリティなし, 1 stop, 8 bit,
				|送受信割り込み禁止
	move.w #0x0038, UBAUD1  | baud rate = 230400 bps

	****************
	**タイマ関係の初期化(割り込みレベルは6に固定されている)
	*****************
	move.w #0x0004, TCTL1   | restart,割り込み不可,
				|システムクロックの1/16を単位として計時，
				|タイマ使用停止

	***************
	**キュー関係の初期化
	***************
	jsr Q_INIT



	***************
	**割り込みベクタの設定
	***************
	move.l #check_send_interrupt,       0x110 /* level 4, (64+4)*4 */
	move.l #check_timer_interrupt,0x118 /* level 6, (64+6)*4 */
	move.l #systemcall_if,        0x080 /* trap #0 */

	move.l #0x00ff3ff9, IMR       |送受信(UART1)とタイマのマスク解除|

	***************
	** crt0.sに戻る
	***************
	move.w #0x0000, %SR | USER MODE, LEVEL 0
	jmp start

	bra	MAIN


****************************************************************
*** プログラム領域 
****************************************************************
.section .text
.even
MAIN:
** 走行モードとレベルの設定 (「ユーザモード」への移行処理)
move.w #0x0000, %SR | USER MODE, LEVEL 0
lea.l USR_STK_TOP,%SP | user stack の設定
** システムコールによる RESET_TIMER の起動
move.l #SYSCALL_NUM_RESET_TIMER,%D0
trap #0
** システムコールによる SET_TIMER の起動
move.l #SYSCALL_NUM_SET_TIMER, %D0
move.w #50000, %D1
move.l #TT, %D2
trap #0
******************************
* sys_GETSTRING, sys_PUTSTRING のテスト
* ターミナルの入力をエコーバックする 
******************************
LOOP:
move.l #SYSCALL_NUM_GETSTRING, %D0
move.l #0, %D1 | ch = 0
move.l #BUF, %D2 | p = #BUF
move.l #256, %D3 | size = 256
trap #0
move.l %D0, %D3 | size = %D0 (length of given string)
move.l #SYSCALL_NUM_PUTSTRING, %D0
move.l #0, %D1 | ch = 0
move.l #BUF,%D2 | p = #BUF
trap #0
bra LOOP
******************************
* タイマのテスト
* ’******’ を表示し改行する． * ５回実行すると，RESET_TIMER をする． 
******************************
TT:
movem.l %D0-%D7/%A0-%A6,-(%SP)
cmpi.w #5,TTC | TTC カウンタで 5 回実行したかどうか数える
beq TTKILL | 5 回実行したら，タイマを止める
move.l #SYSCALL_NUM_PUTSTRING,%D0
move.l #0, %D1 | ch = 0
move.l #TMSG, %D2 | p = #TMSG
move.l #8, %D3 | size = 8
trap #0
addi.w #1,TTC | TTC カウンタを 1 つ増やして
bra TTEND | そのまま戻る
TTKILL:
move.l #SYSCALL_NUM_RESET_TIMER,%D0
trap #0
TTEND:
movem.l (%SP)+,%D0-%D7/%A0-%A6
rts
****************************************************************
*** 初期値のあるデータ領域 
****************************************************************
.section .data
TMSG:
.ascii "******\r\n" | \r: 行頭へ (キャリッジリターン)
.even | \n: 次の行へ (ラインフィード)
TTC:
.dc.w 0
.even
****************************************************************
*** 初期値の無いデータ領域 
****************************************************************
.section .bss
BUF:
.ds.b 256 | BUF[256]
.even
USR_STK:
.ds.b 0x4000 | ユーザスタック領域 .even
USR_STK_TOP: | ユーザスタック領域の最後尾

**********************************************************
**送受信割り込み用のHW割り込みインターフェース 作成者:鵜飼 レビュー:松下
**********************************************************
.section .text
.even
check_send_interrupt:					| UART1割り込みハンドラの入口
	movem.l	%d0-%d3/%a0-%a1,-(%SP) 	| 使用レジスタをスタックへ退避

	move.w 	UTX1, %d0			| 送信ステータスを取得
	andi.l	#0x00008000, %d0		| 送信完了フラグのみを抽出
	cmpi.l	#0x00008000, %d0		| 送信完了かを判定
	bne	check_receive_interrupt		| 送信完了でなければ受信処理へ

	moveq	#0, %d1			| チャネル0を指定
	jsr	INTERPUT			| 送信割り込み処理へ

check_receive_interrupt:				| 受信割り込みのチェック
	move.w URX1, %d3			| 受信ステータスとデータを取得
	move.b %d3, %d2			| 受信データ本体を抽出
	andi.l #0x00002000, %d3		| 受信完了フラグのみを抽出
	cmpi.l #0x00002000, %d3		| 受信完了かを判定
	bne	finish_check_interrupt		| 受信完了でなければ終了

	moveq	#0, %d1			| チャネル0を指定
	jsr	INTERGET			| 受信割り込み処理へ

finish_check_interrupt:				| 割り込み処理終了
	movem.l	(%SP)+, %d0-%d3/%a0-%a1	| 退避していたレジスタを復帰
	rte 					| 割り込みから復帰

**********************************************************
**INTERPUT 作成者:札場 レビュー:平野
**********************************************************
INTERPUT:					| 送信割り込み処理本体
	move.w %SR,-(%sp)			| ステータスレジスタを退避
	movem.l %d0-%d1,-(%sp)			| 使用するデータレジスタを退避
	move.w #0x2700,%SR			| 割り込みを禁止

	cmpi.l #0,%d1				| サポートしているチャネルか判定
	bne END_INTERPUT			| 0以外なら何もせず終了

	moveq	#1,%d0				| 送信用キュー番号1を指定
	jsr OUTQ				| キューから1バイト取得
	cmpi.l #0,%d0				| 取得成功か判定
	beq FAIL_INTERPUT			| 失敗ならUART制御レジスタをリセット

	andi.l #0x000000ff,%d1			| 送信データを1バイトに制限
	addi.w #0x0800,%d1			| 送信フラグを付与
	move.w %d1,UTX1				| UARTへデータ書き込み
	bra END_INTERPUT			| 正常終了へ

FAIL_INTERPUT:					| キューが空だった場合
	move.w #0xE108,USTCNT1			| 送信割り込みを停止

END_INTERPUT:					| 後始末
	movem.l (%sp)+,%d0-%d1			| データレジスタを復帰
	move.w (%sp)+,%SR			| ステータスレジスタを復帰
	rts					| 送信割り込み処理終了

**********************************************************
**PUTSTRING 作成者:札場 レビュー:平野
**********************************************************
PUTSTRING:					| 文字列送信用システムコール本体
	movem.l %d1-%d4/%a0-%a1,-(%SP)	| 使用レジスタを退避
	cmpi.l #0,%d1				| チャネル番号を確認
	bne END_PUTSTRING			| サポート外なら何もせず戻る

	moveq	#0,%d4				| 送信済みバイト数を初期化
	movea.l %d2,%a1			| 送信元アドレスをセット

	cmpi.l #0,%d3				| 送信サイズが0か確認
	beq	SEND_SZ				| 0なら即座に戻り値だけ返す

loop_put_string:					| 送信キューへ書き込むループ
	cmp.l %d3,%d4				| 送信済みが目標サイズに達したか
	beq ALLOW_SEND				| 全て書けたら送信許可へ

	moveq #1,%d0				| 送信用キュー番号1
	move.b (%a1),%d1			| バッファから1バイト読み出し
	jsr INQ					| キューへ格納
	cmpi.l #0,%d0				| 書き込み成功したか確認
	beq ALLOW_SEND				| 失敗したらUART送信を開始させる

	addq.l #1,%d4				| カウンタをインクリメント
	adda.l #1,%a1				| 次の文字へポインタ更新
	bra loop_put_string			| 引き続きキューへ投入

ALLOW_SEND:					| UART送信を許可
	move.w #0xE10C,USTCNT1			| 送信割り込みを有効化

SEND_SZ:					| 戻り値準備
	move.l %d4,%d0				| 実際にキューへ送れたサイズを返す

END_PUTSTRING:
	movem.l (%SP)+,%d1-%d4/%a0-%a1	| レジスタ復帰
	rts					| システムコール復帰

**********************************************************
**GETSTRING 作成者:平野 レビュー:札場
**********************************************************
GETSTRING:
	/* 入力：チャネル d1、コピー先アドレス d2、個数 d3 */
	/* 出力：取り出したデータ個数 d0*/

	movem.l	%d4-%d7/%a0-%a6, -(%sp)	| 作業レジスタとアドレスレジスタを退避
	cmpi.l	#0, %d1			| 対応チャネルか確認
	bne	finish_get_string		| 0以外なら何もせず終了

	moveq	#0, %d4			| 受信済みサイズを初期化
	movea.l	%d2, %a1			| コピー先ポインタを設定
	
loop_get_string:					| 受信データ取り出しループ
	cmp.l	%d3,%d4			| 指定サイズに達したか確認
	beq	finish_get_string		| 目標まで受信済みなら終了	

	moveq	#0, %d0			| 受信用キュー番号0
	jsr	OUTQ 				| キューから1バイト取り出す

	cmpi.l	#0, %d0			| 取り出し成功したか判定
	beq	finish_get_string		| 失敗ならそのまま終了	

	move.b	%d1, (%a1)+			| 取得したデータをバッファへ格納
	
	addq.l	#1, %d4			| 受信済みサイズを更新
	bra	loop_get_string		| 必要な数だけ繰り返す
	
finish_get_string:				| 受信処理終了
	move.l	%d4, %d0 			| 実際に受け取れたサイズを返す
	movem.l	(%sp)+, %d4-%d7/%a0-%a6	| 退避していたレジスタを復帰
	rts					| システムコール復帰

**********************************************************
**INTERGET 作成者:平野 レビュー:札場
**********************************************************
INTERGET:					| 受信割り込み処理本体
	movem.l	%d0-%d2/%a0-%a1, -(%sp)	| 使用レジスタを退避

	cmpi.l	#0, %d1			| チャネルが0か確認
	bne	finish_inter_get		| 0以外なら処理不要
	moveq	#0, %d0			| 受信用キュー番号0
	move.b	%d2, %d1			| 受信データをd1へ
	jsr	INQ				| キューへデータを格納

finish_inter_get:
	movem.l	(%sp)+, %d0-%d2/%a0-%a1	| レジスタを復帰	
	rts					| 受信割り込み処理終了

********************************************************************
**タイマ割り込みに関するハードウェア割り込みインターフェース 作成者:平川, 鵜飼 レビュー:松下
********************************************************************
check_timer_interrupt:				| タイマ割り込みハンドラ入口
	movem.l %d0/%a0, -(%SP)		| 使用レジスタを退避

	move.w TSTAT1, %d0			| タイマステータスを読み出し
	andi.l #0x00000001, %d0		| 割り込み発生ビットのみ抽出
	beq finish_timer_interrupt		| 発生していなければ終了

	move.w #0x0000, TSTAT1		| 割り込みフラグをクリア
	jsr CALL_RP				| 登録タスクをコール

finish_timer_interrupt:				| タイマ割り込みの終了処理
	movem.l (%SP)+, %d0/%a0		| レジスタを復帰
	rte					| 割り込みから復帰

**********************************************************
**タイマ制御ルーチン 作成者:松下 レビュー:鵜飼, 平川
**********************************************************
RESET_TIMER:					| タイマ停止とリセット
        move.w #0x0004, TCTL1		| カウンタ停止＆リスタート設定
	rts					| 呼び出し元へ戻る

SET_TIMER:					| タイマ設定システムコール
        move.l %d2, task_p			| コールバックアドレスを保存
	move.w #206, TPRER1			| プリスケーラ設定
	move.w %d1, TCMP1			| コンペア値を設定
	move.w #0x0015, TCTL1		| タイマをスタート
	rts					| 呼び出し元へ戻る

CALL_RP:					| 登録タスク呼び出し
	movem.l %a0, -(%sp)		| アドレスレジスタを退避
	movea.l task_p, %a0			| 登録されているタスクをロード
	jsr    (%a0)				| タスクを実行
	movem.l (%sp)+, %a0		| レジスタを復帰
	rts					| 呼び出し元へ戻る

*********************************************************
**システムコールインターフェース 作成者:平川 レビュー:松下
*********************************************************
systemcall_if:					| trap #0 によるシステムコール入口
	movem.l %D1-%D7/%A0-%A6, -(%sp)	| 破壊する汎用レジスタを保存

	cmpi.l #SYSCALL_NUM_GETSTRING, %D0	| システムコール番号判定
	beq CALL_GETSTRING
	cmpi.l #SYSCALL_NUM_PUTSTRING, %D0
	beq CALL_PUTSTRING
	cmpi.l #SYSCALL_NUM_RESET_TIMER, %D0
	beq CALL_RESET_TIMER
	cmpi.l #SYSCALL_NUM_SET_TIMER, %D0
	beq CALL_SET_TIMER
	bra sys_EXIT				| 未定義番号はそのまま戻る

CALL_GETSTRING:				| GETSTRING呼び出し
	jsr GETSTRING
	bra sys_EXIT

CALL_PUTSTRING:				| PUTSTRING呼び出し
	jsr PUTSTRING
	bra sys_EXIT

CALL_RESET_TIMER:				| RESET_TIMER呼び出し
	jsr RESET_TIMER
	bra sys_EXIT

CALL_SET_TIMER:				| SET_TIMER呼び出し
	jsr SET_TIMER
	bra sys_EXIT

sys_EXIT:					| システムコール出口
	movem.l (%sp)+, %D1-%D7/%A0-%A6	| レジスタを復帰
	rte					| トラップから復帰
	
*********************************************************
**キュー 作成者:平野, 札場 レビュー:鵜飼, 平川
*********************************************************
Q_INIT:	/*構造体とバッファの初期化サブルーチン*/
	movem.l %d0/%a0-%a1, -(%a7) /*レジスタの退避*/

	lea.l   QST_TABLE, %a1          /* QST[0] のアドレス*/
	lea.l   Q_0, %a0         /* BUFFER_0 のアドレス*/
	jsr     INIT_SINGLE_Q           /*1キュー分を初期化*/

	lea.l   QST_TABLE+(QST_SIZE*1), %a1 /*QST[1]の初期化 */
	lea.l   Q_1, %a0
	jsr     INIT_SINGLE_Q
    
	movem.l (%a7)+, %d0/%a0-%a1 /*レジスタの復帰*/
	rts

	
INIT_SINGLE_Q:
	move.l  %a0, in(%a1)     /*ポインタの初期化*/
	move.l  %a0, out(%a1)     
	move.l  %a0, top(%a1)    /*topをa0へ*/
    
	add.l   #B_SIZE-1, %a0
	move.l  %a0, bottom(%a1)      /*bottomをa0へ*/

	move.w  #0,s(%a1) /*sを0へ*/
     
	rts

INQ:/*キュー#noにp番地のデータを入れるサブルーチン*/
	move.w %SR,-(%a7)				| ステータスレジスタを退避
	movem.l %d2/%a0-%a2, -(%a7) /*レジスタの退避*/
	move.w #0x2700,%SR				| 割り込み禁止で排他制御
	lea.l   QST_TABLE, %a1 /*キューの選択*/
	move.w  #QST_SIZE, %d2			| 構造体サイズをロード
	mulu.w  %d0, %d2				| キュー番号ぶんオフセット計算
	add.l   %d2, %a1				| 対象キューの管理構造体を指す
	move.b  %d1, %d0				| データを下位8bitへ
	jsr     PUT_BUF				| バッファ書き込み処理へ
	movem.l (%a7)+, %d2/%a0-%a2 /*レジスタの復帰*/
	move.w (%a7)+,%SR				| ステータスレジスタを復帰
	rts						| 呼び出し元へ戻る

PUT_BUF: /*キューへのデータ書き込みをするサブルーチン*/
	movem.l %d1/%a0/%a2, -(%a7) /*レジスタの退避*/

	cmp.w   #0x0100, s(%a1) /*空き容量の確認*/
	beq	PUT_FAIL

	move.l  in(%a1), %a0 /*データの書き込み*/
	move.b  %d0, (%a0)
	move.l  bottom(%a1), %a2		| バッファ末尾アドレスを取得
	cmp.l   %a2, %a0 /*キューの端か*/
	bne     PUT_CAN
	move.l  top(%a1), %a0			| 末尾に到達したので先頭へ折り返し
	bra	PUT_UPDATE_PTR			| ポインタ更新処理へ
PUT_CAN:
	addq.l  #1, %a0				| 次のアドレスへ進める
PUT_UPDATE_PTR:
	move.l  %a0, in(%a1)			| inポインタを更新
PUT_SET_GET_FLG:
	addq.w  #1, s(%a1) /*読み出し許可*/
	move.w  #1, %d0
	bra     PUT_DONE
PUT_FAIL:
	move.w  #0, %d0
PUT_DONE:
	movem.l (%a7)+, %d1/%a0/%a2 /*レジスタの復帰*/
	rts

OUTQ: /*キュー#noからデータを一つ取り出し，p番地にcopyするサブルーチン*/
	move.w %SR,-(%a7)				| ステータスレジスタを退避
	movem.l %d2/%a0-%a2, -(%a7) /*レジスタの退避*/
	move.w #0x2700,%SR				| 割り込み禁止で排他制御
	lea.l   QST_TABLE, %a1 /*キューの選択*/
	move.w  #QST_SIZE, %d2			| 構造体のサイズをロード
	mulu.w  %d0, %d2				| キュー番号分だけ先へ進める
	add.l   %d2, %a1				| 対象キュー構造体へ
	jsr     GET_BUF				| データ取り出し処理へ
	
	movem.l (%a7)+, %d2/%a0-%a2 /*レジスタの復帰*/
	move.w (%a7)+,%SR				| ステータスレジスタ復帰
	rts						| 呼び出し元へ戻る

GET_BUF: /*キューからのデータ読み出しをするサブルーチン*/
	movem.l %a0/%a2, -(%a7) /*レジスタの退避*/
	cmp.w   #0x00, s(%a1) /*容量の確認*/
	beq     GET_FAIL
	move.l  out(%a1), %a0 /*データの読み出し*/
	move.b  (%a0), %d1
	
	move.l  bottom(%a1), %a2 /*ポインタの更新*/
	cmp.l   %a2, %a0 /*キューの端か*/
	bne     GET_CAN
	move.l  top(%a1), %a0			| 末尾に達したので先頭へ戻す
	bra     GET_UPDATE_PTR			| ポインタ更新処理へ
GET_CAN:
	addq.l  #1, %a0				| 次のアドレスを指す	
GET_UPDATE_PTR:
	move.l  %a0, out(%a1)			| outポインタを更新
GET_SET_PUT_FLG:
	subq.w  #1, s(%a1) /*書き込み許可*/
	move.w  #1, %d0				| 読み出し成功を知らせる
	bra     GET_DONE
GET_FAIL:
	move.w  #0, %d0				| 読み出し失敗を通知
    
GET_DONE:
	movem.l (%a7)+, %a0/%a2 /*レジスタの復帰*/
	rts					| 呼び出し元へ戻る
	

	
.section .data
	.equ B_SIZE,256

	.equ    in, 0    /*次に書き込むアドレス*/
	.equ    out, 4    /*次に読み出すアドレス*/
	.equ    top, 8   /*バッファの先頭アドレス*/
	.equ    bottom, 12    /*バッファの末尾アドレス*/
	.equ    s, 16  

	.equ    QST_SIZE, 20   /* キュー管理構造体1つあたりのサイズ */
QST_TABLE:
    .ds.b   QST_SIZE * 2 /*構造体2つ分のメモリ確保*/

/* 各キューのバッファを2つ確保 */
Q_0:
	.ds.b   B_SIZE
Q_1:
	.ds.b   B_SIZE
	
	.end	
