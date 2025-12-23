/* mtk_c.h */
#ifndef _MTK_C_H_
#define _MTK_C_H_

/* 定数定義 */
#define NULLTASKID  0       /* キューの終端 */
#define NUMTASK     5       /* 最大タスク数 */
#define STKSIZE     1024    /* スタックサイズ*/
#define NUMSEMAPHORE 2      /* セマフォの数 */

/* 型定義 */
typedef int TASK_ID_TYPE;   /* タスクID */

/* セマフォ構造体 */
typedef struct {
    int count;              /* カウンタ */
    int nst;                /* 予約 (WaitPなどで使用) */
    TASK_ID_TYPE task_list; /* 待ち行列の先頭タスクID */
} SEMAPHORE_TYPE;

/* タスクコントロールブロック (TCB) */
typedef struct {
    void (*task_addr)();    /* タスクの開始アドレス */
    void *stack_ptr;        /* 現在のスタックポインタ(SSP) */
    int priority;           /* 優先度 */
    int status;             /* 状態 */
    TASK_ID_TYPE next;      /* 次のタスクID (キュー用) */
} TCB_TYPE;

/* スタック構造体  */
typedef struct {
    char ustack[STKSIZE];   /* ユーザスタック */
    char sstack[STKSIZE];   /* スーパーバイザスタック */
} STACK_TYPE;

/* グローバル変数の外部参照宣言 (mtk_c.c以外から参照する) */
#ifndef _KERNEL_MAIN_
extern TCB_TYPE task_tab[NUMTASK + 1];
extern STACK_TYPE stacks[NUMTASK];
extern SEMAPHORE_TYPE semaphore[NUMSEMAPHORE];
extern TASK_ID_TYPE ready;
extern TASK_ID_TYPE curr_task;
extern TASK_ID_TYPE new_task;
extern TASK_ID_TYPE next_task;
#endif

/* 関数プロトタイプ宣言 */
/* C言語関数 */
void init_kernel(void);
void set_task(void (*func)());
void begin_sch(void);
void addq(TASK_ID_TYPE *head, TASK_ID_TYPE id);
TASK_ID_TYPE removeq(TASK_ID_TYPE *head);
void sched(void);
void p_body(int sem_id);
void v_body(int sem_id);
void sleep(int ch);
void wakeup(int ch);
void *init_stack(TASK_ID_TYPE id);

/* アセンブリ関数 */
void first_task(void);
void swtch(void);
void P(int sem_id);
void V(int sem_id);
void init_timer(void);

#endif
