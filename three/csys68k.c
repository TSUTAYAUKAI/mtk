/* csys68k.c (Theme 3 Modified) */
#include <stdarg.h>
/* fcntl用定数 (ヘッダがない場合用) */
#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef O_RDWR
#define O_RDWR 2
#endif

/* * 外部アセンブリ関数の定義変更 
 * 引数にポート番号 (int port) を追加しました。
 * port=0: UART1, port=1: UART2
 */
extern void outbyte(int port, unsigned char c);
extern char inbyte(int port);

/* * fcntl: fdopenのために実装 [cite: 6273]
 * cmd=F_GETFL の場合に O_RDWR (読み書き可) を返すことで、
 * fdopen が成功するようにします。
 */
int fcntl(int fd, int cmd, ...)
{
  if (cmd == F_GETFL) {
    return O_RDWR; 
  }
  return 0;
}

/* ヘルパー関数: fd から ポート番号への変換 [cite: 6283] */
static int get_port_from_fd(int fd) {
    if (fd >= 0 && fd <= 3) {
        return 0; /* fd 0-3 -> Port 0 (UART1) */
    } else if (fd == 4) {
        return 1; /* fd 4   -> Port 1 (UART2) */
    }
    return -1;    /* Error */
}

int read(int fd, char *buf, int nbytes)
{
  char c;
  int  i;
  int port;

  /* fdからポート番号を決定 */
  port = get_port_from_fd(fd);
  if (port == -1) {
      return -1; /* エラー: 無効なファイルディスクリプタ */
  }

  for (i = 0; i < nbytes; i++) {
    /* 指定されたポートから1文字読む */
    c = inbyte(port);

    /* --- エコーバック処理 --- */
    /* 入力と同じポートに出力(outbyte)する */

    if (c == '\r' || c == '\n'){ /* CR -> CRLF */
      outbyte(port, '\r');
      outbyte(port, '\n');
      *(buf + i) = '\n';

    /* } else if (c == '\x8'){ */     /* backspace \x8 */
    } else if (c == '\x7f'){      /* backspace \x8 -> \x7f (by terminal config.) */
      if (i > 0){
        outbyte(port, '\x8'); /* bs  */
        outbyte(port, ' ');   /* spc */
        outbyte(port, '\x8'); /* bs  */
        i--;
      }
      i--;
      continue;

    } else {
      outbyte(port, c);
      *(buf + i) = c;
    }

    if (*(buf + i) == '\n'){
      return (i + 1);
    }
  }
  return (i);
}

int write (int fd, char *buf, int nbytes)
{
  int i, j;
  int port;

  /* fdからポート番号を決定 */
  port = get_port_from_fd(fd);
  if (port == -1) {
      return -1; /* エラー */
  }

  for (i = 0; i < nbytes; i++) {
    if (*(buf + i) == '\n') {
      outbyte (port, '\r');          /* LF -> CRLF */
    }
    /* 指定されたポートへ1文字出力 */
    outbyte (port, *(buf + i));

    /* ビジーウェイト (元のコードを維持) */
    for (j = 0; j < 300; j++);
  }
  return (nbytes);
}
