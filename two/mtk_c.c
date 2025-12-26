/* mtk_c.c */
/* マルチタスクカーネル (C言語部分) */

#define _KERNEL_MAIN_   /* 実体を宣言するために定義 */
#include "mtk_c.h"

/* ==========================================================
   グローバル変数の実体定義
   ========================================================== */
TCB_TYPE task_tab[NUMTASK + 1];     /* TCB (ID=1〜5を使用) */
STACK_TYPE stacks[NUMTASK];         /* スタック (添字0〜4を使用) */
SEMAPHORE_TYPE semaphore[NUMSEMAPHORE]; /* セマフォ */

TASK_ID_TYPE ready;      /* 実行待ち行列の先頭ID */
TASK_ID_TYPE curr_task;  /* 現在実行中のタスクID */
TASK_ID_TYPE next_task;  /* 次に実行するタスクID (schedで決定) */


/* 外部関数のプロトタイプ宣言 (アセンブラ関数) */
extern void pv_handler(); /* mtk_asm.s にある割り込みハンドラ */

/* ==========================================================
   キュー操作関数
   ========================================================== */

/* addq: キューの最後尾にタスクを追加 [cite: 830] */
void addq(TASK_ID_TYPE *head, TASK_ID_TYPE id) {
    TASK_ID_TYPE curr;

    /* キューが空の場合 */
    if (*head == NULLTASKID) {
        *head = id;
    } 
    else {
        /* 末尾を探す */
        curr = *head;
        while (task_tab[curr].next != NULLTASKID) {
            curr = task_tab[curr].next;
        }
        /* 末尾に繋ぐ */
        task_tab[curr].next = id;
    }
    
    /* 新しい末尾のnextは必ずNULL */
    task_tab[id].next = NULLTASKID;
}

/* removeq: キューの先頭からタスクを取り出す [cite: 832] */
TASK_ID_TYPE removeq(TASK_ID_TYPE *head) {
    TASK_ID_TYPE id;

    if (*head == NULLTASKID) {
        return NULLTASKID;
    }

    /* 先頭を取り出す */
    id = *head;
    /* 先頭位置を次にずらす */
    *head = task_tab[id].next;
    
    /* 取り出したタスクのnextはクリアしておく */
    task_tab[id].next = NULLTASKID;

    return id;
}

/* ==========================================================
   スケジューリング関数
   ========================================================== */

/* sched: 次に実行するタスクを決定する [cite: 843] */
void sched(void) {
    /* readyキューから1つ取り出す */
    next_task = removeq(&ready);

    /* 実行できるタスクがなければ無限ループ (アイドル状態) */
    if (next_task == NULLTASKID) {
        while(1);
    }
}

/* ==========================================================
   セマフォ操作 (P/V)
   ========================================================== */

/* sleep: 現在のタスクを待ち行列に入れ、タスク切り替えを行う [cite: 846] */
void sleep(int ch) {
    /* 指定されたセマフォの待ち行列に追加 */
    addq(&semaphore[ch].task_list, curr_task);

    /* 次のタスクを決める */
    sched();

    /* コンテキストスイッチ (アセンブラ関数) */
    swtch();
}

/* wakeup: 待ち行列からタスクを起こしてreadyキューに戻す [cite: 850] */
void wakeup(int ch) {
    TASK_ID_TYPE id;

    /* セマフォの待ち行列から1つ取り出す */
    id = removeq(&semaphore[ch].task_list);

    /* タスクがあればreadyキューへ移動 (実行可能状態へ) */
    if (id != NULLTASKID) {
        addq(&ready, id);
    }
}

/* p_body: P操作 (count--) [cite: 834] */
void p_body(int sem_id) {
    semaphore[sem_id].count--;
    if (semaphore[sem_id].count < 0) {
        /* 資源がないので寝る */
        sleep(sem_id);
    }
}

/* v_body: V操作 (count++) [cite: 839] */
void v_body(int sem_id) {
    semaphore[sem_id].count++;
    if (semaphore[sem_id].count <= 0) {
        /* 待っているタスクがいれば起こす */
        wakeup(sem_id);
    }
}

/* ==========================================================
   初期化関連
   ========================================================== */

/* init_stack: タスクのスタックフレーム初期化 [cite: 803] */
/* 図2.8に基づき、RTEでタスクが開始できるようにダミーのコンテキストを作る */
void *init_stack(TASK_ID_TYPE id) {
    /* スタック配列は0始まり、IDは1始まりなので id-1 を使う */
    /* スタックの底(高位アドレス)から積んでいくためにポインタを設定 */
    /* STKSIZEの最後尾のアドレスを取得 */
    unsigned char *sp = (unsigned char *)&stacks[id - 1].sstack[STKSIZE];
    unsigned long *lsp;
    unsigned short *wsp;

    /* 1. Initial PC (4bytes) */
    sp -= 4;
    lsp = (unsigned long *)sp;
    *lsp = (unsigned long)task_tab[id].task_addr;

    /* 2. Initial SR (2bytes) -> 0x0000 (ユーザモード/割込許可) */
    sp -= 2;
    wsp = (unsigned short *)sp;
    *wsp = 0x0000;

    /* 3. Registers D0-D7, A0-A6 (15 * 4 = 60 bytes) */
    /* 初期値は不定でよいのでポインタだけずらす */
    sp -= 60;

    /* 4. Initial USP (4bytes) */
    /* ユーザスタックの底のアドレスを計算して入れる */
    sp -= 4;
    lsp = (unsigned long *)sp;
    *lsp = (unsigned long)&stacks[id - 1].ustack[STKSIZE];

    /* 作成したSSPのトップアドレスを返す */
    return (void *)sp;
}

/* set_task: ユーザタスクの登録 [cite: 792] */
void set_task(void (*func)()) {
    TASK_ID_TYPE id;
    int i;

    /* 空いているTCBを探す (ID=1から) */
    id = NULLTASKID;
    for (i = 1; i <= NUMTASK; i++) {
        if (task_tab[i].task_addr == 0) { /* task_addrが0なら未使用とみなす */
            id = i;
            break;
        }
    }

    if (id == NULLTASKID) {
        return; /* 空きなし */
    }

    /* TCB設定 */
    task_tab[id].task_addr = func;
    task_tab[id].status = 1; /* 使用中 */
    task_tab[id].next = NULLTASKID;

    /* スタック初期化とSSPの保存 */
    task_tab[id].stack_ptr = init_stack(id);

    /* readyキューへ登録 */
    addq(&ready, id);
}

/* init_kernel: カーネルの初期化 [cite: 785] */
void init_kernel(void) {
    int i;

    /* 変数初期化 */
    ready = NULLTASKID;
    curr_task = NULLTASKID;
    next_task = NULLTASKID;

    /* TCB初期化 */
    for (i = 0; i <= NUMTASK; i++) {
        task_tab[i].task_addr = 0;
        task_tab[i].next = NULLTASKID;
    }

    /* セマフォ初期化 */
    for (i = 0; i < NUMSEMAPHORE; i++) {
        semaphore[i].count = 1; /* 初期値は用途によるが一旦0 */
        semaphore[i].task_list = NULLTASKID;
    }

    /* TRAP #1 (Vector 33) に pv_handler を登録 */
    /* Vector address = 33 * 4 = 132 = 0x84 */
    *(void (**)())0x84 = pv_handler;
}

/* begin_sch: マルチタスク開始 [cite: 808] */
void begin_sch(void) {
    /* 最初のタスクを決める */
    curr_task = removeq(&ready);

    /* タイマの初期化 (mtk_asm.s) */
    init_timer();

    /* 最初のタスクへ飛ぶ (mtk_asm.s) */
    /* ここからは戻ってこない */
    first_task();
}
