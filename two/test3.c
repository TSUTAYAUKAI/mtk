/* test3.c */
/* テーマ3: 2ポート同時出力の動作確認 (資料準拠版) */

#include <stdio.h>
#include "mtk_c.h"

/* --- 資料 3.1.2節で指定された大域変数 --- */
FILE *com0in;   /* UART1 (Port 0) 入力用 */
FILE *com0out;  /* UART1 (Port 0) 出力用 */
FILE *com1in;   /* UART2 (Port 1) 入力用 */
FILE *com1out;  /* UART2 (Port 1) 出力用 */


void task1() { /* Port 0 (UART1) Task */
    volatile int i;
    while(1) {
        if(com0out) {
            fprintf(com0out, "Output from Port 0\n");
            fflush(com0out);
        }
        for(i=0; i<30000; i++);
    }
}

void task2() { /* Port 1 (UART2) Task */
    volatile int i;
    while(1) {
        if(com1out) {
            fprintf(com1out, "Output from Port 1\n");
            fflush(com1out);
        }
        for(i=0; i<30000; i++);
    }
}
/* ユーザ定義 exit() (暴走防止) */
void exit(int status) {
    while(1);
}

int main() {
    /* 1. カーネルの初期化 */
    init_kernel();
    
    /* ポート0 (UART1) のストリーム確保 */
    /* fd=3 を使用する例 */
    com0in  = fdopen(3, "r");
    com0out = fdopen(3, "w");

    /* ポート1 (UART2) のストリーム確保 */
    /* fd=4 を使用する例 */
    com1in  = fdopen(4, "r");
    com1out = fdopen(4, "w");

    /* エラーチェック (実装不備でNULLになる場合の確認) */
    if (com1out == NULL) {
        printf("Error: fdopen failed for Port 1. Check fcntl() implementation.\n");
    }

    /* 3. タスクの登録 */
    set_task(task1);
    set_task(task2);

    /* 4. マルチタスクの開始 */
    begin_sch();

    return 0;
}
