.global inbyte

.equ SYSCALL_NUM_GETSTRING, 1

.text
.even

inbyte:
	movem.l %d2-%d3/%a0,-(%sp)
inbyte_loop:
	move.l #SYSCALL_NUM_GETSTRING,%d0	| GETSTRING指定
	moveq	#0,%d1			| チャネル0を指定
	lea	inbyte_buf,%a0	
	move.l %a0,%d2			| バッファのアドレスを格納
	moveq	#1,%d3
	trap	#0
	tst.l	%d0
	beq	inbyte_loop		| １バイト受け取るまで続ける

	moveq	#0,%d0
	move.b	inbyte_buf,%d0		| 返り値を格納

	movem.l (%sp)+,%d2-%d3/%a0
	rts

.section .bss
.even
inbyte_buf:
	.ds.b 1				| １バイト分
.even
