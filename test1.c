#include <stdio.h>

/* 文字を入力したら表示される */

int main(void){
  char c;
  while(1){
    scanf("%c", &c);
    printf("%c", c);
  }
  return 0;
}
