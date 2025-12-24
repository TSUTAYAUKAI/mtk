/* mtk_asm.s */
/* マルチタスクカーネル (アセンブリ言語部分) */

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

    /* 外部参照 (C言語の変数・関数) */
    .extern curr_task
    .extern next_task
    .extern ready
    .extern task_tab
    .extern p_body
    .extern v_body
    .extern addq
    .extern sched

    /* 定数定義 */
    .equ TCB_SIZE, 20       /* TCB構造体のサイズ (5要素×4バイト) */
    .equ TCB_SP_OFFSET, 4   /* stack_ptrのオフセット (2番目の要素) */
    .equ SYSCALL_NUM_SET_TIMER, 4

/* ==========================================================
   ユーザタスク起動サブルーチン
   first_task()
   C言語の begin_sch() から呼ばれる。ここからは戻らない。
   ========================================================== */
first_task:
    /* 1. 現在のタスク(curr_task)のSSPを回復する */
    /* TCBのアドレス計算: task_tab + (curr_task * 20) */
    move.l  curr_task, %d0
    mulu.w  #TCB_SIZE, %d0
    lea     task_tab, %a0
    add.l   %d0, %a0        /* a0 = &task_tab[curr_task] */
    
    move.l  TCB_SP_OFFSET(%a0), %sp /* SSPを回復 */

    /* 2. USP, レジスタの回復 (図2.8のスタック構造に対応) */
    move.l  (%sp)+, %a0
    move.l  %a0, %usp       /* USP回復 */
    
    movem.l (%sp)+, %d0-%d7/%a0-%a6 /* レジスタ回復 */

    /* 3. ユーザタスクへ移行 */
    rte

/* ==========================================================
   タスクスイッチ関数
   swtch()
   sched()でnext_taskが決まった後に呼ばれる。
   ========================================================== */
swtch:
    /* 1. 戻り番地用にSRをスタックに積む (RTEで復帰するため) */
    move.w  %sr, -(%sp)

    /* 2. 現在のタスクのコンテキスト保存 */
    movem.l %d0-%d7/%a0-%a6, -(%sp) /* レジスタ保存 */
    move.l  %usp, %a0
    move.l  %a0, -(%sp)             /* USP保存 */

    /* 3. 現在のタスクのSSPをTCBに保存 */
    move.l  curr_task, %d0
    mulu.w  #TCB_SIZE, %d0
    lea     task_tab, %a0
    add.l   %d0, %a0
    move.l  %sp, TCB_SP_OFFSET(%a0)

    /* 4. タスクの切り替え */
    move.l  next_task, curr_task

    /* 5. 次のタスクのSSPをTCBから復帰 */
    move.l  curr_task, %d0
    mulu.w  #TCB_SIZE, %d0
    lea     task_tab, %a0
    add.l   %d0, %a0
    move.l  TCB_SP_OFFSET(%a0), %sp

    /* 6. 次のタスクのコンテキスト復帰 */
    move.l  (%sp)+, %a0
    move.l  %a0, %usp               /* USP復帰 */
    movem.l (%sp)+, %d0-%d7/%a0-%a6 /* レジスタ復帰 */

    /* 7. 新しいタスクへ (RTE) */
    rte

/* ==========================================================
   システムコール入口 (P, V)
   C言語から呼ばれる: P(sem_id), V(sem_id)
   ========================================================== */
P:
    move.l  #0, %d0         /* D0 = 0 (P操作を示す) */
    move.l  4(%sp), %d1     /* D1 = 引数 sem_id */
    trap    #1
    rts

V:
    move.l  #1, %d0         /* D0 = 1 (V操作を示す) */
    move.l  4(%sp), %d1     /* D1 = 引数 sem_id */
    trap    #1
    rts

/* ==========================================================
   TRAP #1 割り込みハンドラ
   pv_handler
   P, Vシステムコールの実体処理
   ========================================================== */
pv_handler:
    /* 1. レジスタ退避 */
    movem.l %d0-%d7/%a0-%a6, -(%sp)

    /* 2. 割り込み禁止 (SR操作) */
    move.w  %sr, -(%sp)     /* SR退避 */
    move.w  #0x2700, %sr    /* 割り込み禁止 (Level 7) */

    /* 3. C関数の呼び出し (p_body or v_body) */
    move.l  %d1, -(%sp)     /* 引数 sem_id をプッシュ */
    
    tst.l   %d0             /* D0が0ならP, 1ならV */
    bne     do_v
do_p:
    jsr     p_body
    bra     pv_end
do_v:
    jsr     v_body
    
pv_end:
    addq.l  #4, %sp         /* 引数除去 */

    /* 4. 割り込み禁止解除 & レジスタ復帰 */
    move.w  (%sp)+, %sr
    movem.l (%sp)+, %d0-%d7/%a0-%a6

    /* 5. 復帰 */
    rte

/* ==========================================================
   タイマ初期化
   init_timer()
   モニタのシステムコールを使ってタイマを設定する
   ========================================================== */
init_timer:
    move.l  #SYSCALL_NUM_SET_TIMER, %d0
    move.w  #1000, %d1         /* 割り込み間隔 (適当な値) 。個人課題で変更した。*/
    move.l  #hard_clock, %d2    /* ハンドラのアドレス */
    trap    #0
    rts

/* ==========================================================
   タイマ割り込みハンドラ
   hard_clock
   モニタから呼ばれるため RTS で戻る
   ========================================================== */
hard_clock:
    /* 1. レジスタ退避 (重要: 全レジスタ保存) */
    movem.l %d0-%d7/%a0-%a6, -(%sp)

    /* 2. curr_task を ready キューの末尾に追加 (ラウンドロビン) */
    /* addq(&ready, curr_task) */
    move.l  curr_task, -(%sp)   /* 第2引数: id */
    pea     ready               /* 第1引数: *head (&ready) */
    jsr     addq
    addq.l  #8, %sp             /* 引数除去 */

    /* 3. 次のタスクを決める */
    jsr     sched

    /* 4. タスク切り替え */
    jsr     swtch

    /* 5. レジスタ復帰 */
    movem.l (%sp)+, %d0-%d7/%a0-%a6
    
    rts
