    .section .text
    .global outbyte
    .even
    

    .equ SYSCALL_NUM_PUTSTRING, 2     /* システムコール定数定義 */

outbyte:
    
    link    %a6, #-4 /* バッファ確保*/
   
    movem.l %d2-%d3, -(%sp) /* レジスタの退避 */

    move.l  8(%a6), %d0     /* 引数cを取り出す */
    
    move.b  %d0, -4(%a6) /*バッファに書き込み*/

retry_out:
    move.l  #SYSCALL_NUM_PUTSTRING, %d0 /*システムコール*/
    move.l  #0, %d1
    lea     -4(%a6), %a0
    move.l  %a0, %d2
    move.l  #1, %d3
    trap    #0

    tst.l   %d0 /*0文字ならリトライ*/
    beq     retry_out

    movem.l (%sp)+, %d2-%d3/* 7. レジスタの復帰 */

    unlk    %a6 /*スタックフレームの破棄*/

    rts
