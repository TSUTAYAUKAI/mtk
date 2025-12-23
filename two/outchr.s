/* outchr.s : 中難度Option (link/unlk版) */

    .section .text
    .global outbyte
    .even
    
    /* 定数定義 */
    .equ SYSCALL_NUM_PUTSTRING, 2

outbyte:
    /* 1. スタックフレームの作成 (バッファ確保) */
    /* 旧FP保存 -> FP更新 -> スタック4バイト確保(-4) */
    link    %a6, #-4

    /* 2. レジスタの退避 */
    movem.l %d2-%d3, -(%sp)

    /* 3. 引数の取得 (ここが便利！) */
    /* linkを使った場合、引数は必ず 8(%a6) 以降にあります */
    /* 0(%a6) = 旧フレームポインタ
       4(%a6) = 戻り番地
       8(%a6) = 第1引数 (char c)
    */
    move.l  8(%a6), %d0     /* 引数cをレジスタに取り出す */
    
    /* 4. ローカル変数(バッファ)への書き込み */
    /* 確保した -4(%a6) の領域にデータを置く */
    move.b  %d0, -4(%a6)

retry_out:
    move.l  #SYSCALL_NUM_PUTSTRING, %d0
    move.l  #0, %d1

    /* 5. バッファアドレスの指定 */
    /* フレームポインタ基準で -4 の位置のアドレスを計算 */
    lea     -4(%a6), %a0
    move.l  %a0, %d2

    move.l  #1, %d3
    trap    #0

    /* 6. 戻り値チェック (0文字ならリトライ) */
    tst.l   %d0
    beq     retry_out

    /* 7. レジスタの復帰 */
    movem.l (%sp)+, %d2-%d3

    /* 8. スタックフレームの破棄 */
    unlk    %a6

    rts
