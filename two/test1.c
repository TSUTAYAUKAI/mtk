/* test1.c */
#include <stdio.h>

/* プロトタイプ宣言 (csys68k.c等に含まれる標準入出力関数) */
/* 実際にはstdio.hがそれらを解決しますが、明示的な確認用 */

/* ユーザ定義 exit() : PDF 1.4.6節 低難度Option */
/* main関数を抜けた後の暴走を防ぐための無限ループ措置 */
void exit(int status) {
    /* LED0 (0x00d00039) に 'H' (Halt) のようなステータスを表示しても良い */
    /* ここでは単純に無限ループで停止させる */
    volatile char *led = (char *)0x00d00039;
    *led = 'E'; /* Exit called */
    
    while(1);
}

int main(void) {
    char buffer[128];
    int i = 0;

    /* 起動メッセージ */
    printf("=== Theme 1 Test Program ===\n");
    printf("Please input text and press Enter.\n");

    /* エコーバックループ */
    while(1) {
        printf("INPUT> ");
        
        /* 文字列の入力 (inbyte -> read -> scanf の経路を確認) */
        scanf("%s", buffer);

        /* 結果の表示 (outbyte -> write -> printf の経路を確認) */
        printf("You typed: %s\n", buffer);
        
        /* ループ回数の表示テスト */
        i++;
        printf("Loop count: %d\n", i);
    }

    return 0;
}
