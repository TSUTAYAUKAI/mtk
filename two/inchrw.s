    .section .text
    .global inbyte
    .even
    .equ SYSCALL_NUM_GETSTRING, 1 /*システムコール定数定義*/

inbyte:
    link    %a6, #-4 /*バッファ確保*/

    movem.l %d2-%d3, -(%sp) /*レジスタの退避*/

retry_in:
    move.l  #SYSCALL_NUM_GETSTRING, %d0 /*システムコール*/
    move.l  #0, %d1
    lea     -4(%a6), %a0    
    move.l  %a0, %d2        
    move.l  #1, %d3
    trap    #0

    tst.l   %d0 /*0文字だったらやり直す*/
    beq     retry_in

    move.l  #0, %d0 /*戻り値の取得*/
    move.b  -4(%a6), %d0 

    movem.l (%sp)+, %d2-%d3　/*レジスタの復帰*/

    unlk    %a6 /*スタックの破棄*/
    
    rts
