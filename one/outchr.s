.global outbyte

.equ SYSCALL_NUM_PUTSTRING, 2

.text
.even

outbyte:
	move.l 4(%sp),%d0		| スタックからデータを取り出す
	andi.l #0xff,%d0		| ８ビットに
	movem.l %d2-%d3/%a0,-(%sp)
	lea	outbyte_buf,%a0
	move.b %d0,(%a0)		| バッファにいれる

outbyte_loop:
	move.l #SYSCALL_NUM_PUTSTRING,%d0
	moveq	#0,%d1			| チャネル0を指定
	move.l %a0,%d2 
	moveq	#1,%d3			| １バイト送る
	trap	#0
	cmpi.l #1,%d0			| 判定
	bne	outbyte_loop
	movem.l (%sp)+,%d2-%d3/%a0
	rts

.section .bss
.even
outbyte_buf:
	.ds.b 1				| １バイト分のバッファ確保 
.even
