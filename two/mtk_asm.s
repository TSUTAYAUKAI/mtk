/* mtk_asm.s */

    .section .text
    .even
    
    /* 外部シンボルの定義 */
    .global first_task
    .global swtch
    .global P
    .global V
    .global pv_handler
    .global hard_clock
    .global init_timer

    /* C言語の変数・関数 */
    .extern curr_task
    .extern next_task
    .extern ready
    .extern task_tab
    .extern p_body
    .extern v_body
    .extern addq
    .extern sched

    /* 定数定義 */
    .equ TCB_SIZE, 20       /* TCB構造体のサイズ  */
    .equ TCB_SP_OFFSET, 4   /* stack_ptrのオフセット */
    .equ SYSCALL_NUM_SET_TIMER, 4

/* ==========================================================
   ユーザタスク起動サブルーチン
   first_task()
   C言語の begin_sch() から呼ばれる。ここからは戻らない。
   ========================================================== */
first_task:
    /* 現在のタスクのSSPを回復する */
    /* TCBのアドレス計算: task_tab + (curr_task * 20) */
    move.l  curr_task, %d0
    mulu.w  #TCB_SIZE, %d0
    lea     task_tab, %a0
    add.l   %d0, %a0        /* a0 = &task_tab[curr_task] */
    
    move.l  TCB_SP_OFFSET(%a0), %sp /* SSPを回復 */

    move.l  (%sp)+, %a0
    move.l  %a0, %usp       /* USP回復 */
    
    movem.l (%sp)+, %d0-%d7/%a0-%a6 /* レジスタ復帰 */

    /* ユーザタスクへ移行 */
    rte

/* ==========================================================
   タスクスイッチ関数
   swtch()
   sched()でnext_taskが決まった後に呼ばれる。
   ========================================================== */
swtch:
    move.w  %sr, -(%sp) /*戻り番地用にスタックに積む*/

    movem.l %d0-%d7/%a0-%a6, -(%sp) /* レジスタ保存 */
    move.l  %usp, %a0
    move.l  %a0, -(%sp)             /* USP保存 */

    /* 現在のタスクのSSPをTCBに保存 */
    move.l  curr_task, %d0
    mulu.w  #TCB_SIZE, %d0
    lea     task_tab, %a0
    add.l   %d0, %a0
    move.l  %sp, TCB_SP_OFFSET(%a0)
   
    move.l  next_task, curr_task　/* タスクの切り替え */

    /* 次のタスクのSSPをTCBから復帰 */
    move.l  curr_task, %d0
    mulu.w  #TCB_SIZE, %d0
    lea     task_tab, %a0
    add.l   %d0, %a0
    move.l  TCB_SP_OFFSET(%a0), %sp


    move.l  (%sp)+, %a0
    move.l  %a0, %usp               /* USP復帰 */
    movem.l (%sp)+, %d0-%d7/%a0-%a6 /* レジスタ復帰 */
 
    rte  /* 新しいタスク開始 */

/* ==========================================================
   システムコール入口 (P, V)
   C言語から呼ばれる: P(sem_id), V(sem_id)
   ========================================================== */
P:
    move.l  #0, %d0         /* D0 = 0 */
    move.l  4(%sp), %d1     /* D1 = 引数 sem_id */
    trap    #1
    rts

V:
    move.l  #1, %d0         /* D0 = 1  */
    move.l  4(%sp), %d1     /* D1 = 引数 sem_id */
    trap    #1
    rts

/* ==========================================================
   TRAP #1 割り込みハンドラ
   pv_handler
   P, Vシステムコール
   ========================================================== */
pv_handler:
    
    movem.l %d0-%d7/%a0-%a6, -(%sp)/* レジスタ退避 */

    move.w  %sr, -(%sp)     /* SR退避 */
    move.w  #0x2700, %sr    /* 割り込み禁止 */

    move.l  %d1, -(%sp)     /* 引数 sem_id をプッシュ */
    
    tst.l   %d0             /* D0が0ならP, 1ならVを呼び出す */
    bne     do_v
do_p:
    jsr     p_body
    bra     pv_end
do_v:
    jsr     v_body
    
pv_end:
    addq.l  #4, %sp         /* 引数除去 */

    move.w  (%sp)+, %sr /*割り込み禁止解除*/
    movem.l (%sp)+, %d0-%d7/%a0-%a6 /*レジスタ復帰*/

    rte

/* ==========================================================
   タイマ初期化
   init_timer()
   モニタのシステムコールを使ってタイマを設定する
   ========================================================== */
init_timer:
    move.l  #SYSCALL_NUM_SET_TIMER, %d0
    move.w  #50000, %d1         /* 割り込み間隔  */
    move.l  #hard_clock, %d2    /* 割り込みハンドラのアドレス */
    trap    #0
    rts

/* ==========================================================
   タイマ割り込みハンドラ
   hard_clock
   モニタから呼ばれるため RTS で戻る
   ========================================================== */
hard_clock:
    movem.l %d0-%d7/%a0-%a6, -(%sp) /*レジスタ退避*/

    /* curr_task を ready キューの末尾に追加 */
    /* addq(&ready, curr_task) */
    move.l  curr_task, -(%sp)   /* 第2引数: id */
    pea     ready               /* 第1引数: *head (&ready) */
    jsr     addq
    addq.l  #8, %sp             /* 引数除去 */

   
    jsr     sched  /*次のタスクを決める */

    jsr     swtch    /* タスク切り替え */

    movem.l (%sp)+, %d0-%d7/%a0-%a6/*レジスタ復帰*/
    
    rts
