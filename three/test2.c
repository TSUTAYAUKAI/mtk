/* test2.c */
/* マルチタスク動作確認用プログラム */

#include <stdio.h>
#include "mtk_c.h"

/* 動作確認用タスク1 */
void task1() {
    volatile int i;
    P(1);
    while (1) {
    
        /* "1" を表示し続ける */
        printf("1");
	fflush(stdout);
        
        /* 表示が速すぎると見にくいので少しウェイトを入れる */
        for (i = 0; i < 10000; i++);
        V(2);
        P(1);
    }
}

/* 動作確認用タスク2 */
void task2() {
    volatile int i;
    V(1);
    P(2);
    while (1) {
        /* "2" を表示し続ける */
        printf("2");
	fflush(stdout);
        
        /* 表示が速すぎると見にくいので少しウェイトを入れる */
        for (i = 0; i < 10000; i++);
        V(1);
        P(2);
    }
}


/* ユーザ定義 exit() (テーマ1からの引き継ぎ) */
void exit(int status) {
    /* ここに来ることは想定していないが、暴走防止のため無限ループ */
    while(1);
}

int main() {
    /* 1. カーネルの初期化 */
    /* TCB、セマフォ、割り込みベクタ(TRAP #1)の設定 */
    init_kernel();
    
        semaphore[1].count = 0; /* 初期値は用途によるが一旦0 */
        semaphore[1].task_list = NULLTASKID;
        
        semaphore[2].count = 0; /* 初期値は用途によるが一旦0 */
        semaphore[2].task_list = NULLTASKID;

    /* 2. タスクの登録 */
    /* ユーザ関数をタスクとして登録する */
    set_task(task1);
    set_task(task2);

    /* 3. マルチタスクの開始 */
    /* ここでタイマが起動し、最初のタスクへジャンプする */
    /* これ以降、main関数には戻ってこない */
    begin_sch();

    return 0;
}
