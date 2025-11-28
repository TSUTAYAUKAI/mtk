.global inbyte

.equ SYSCALL_NUM_GETSTRING, 1

.text
.even

inbyte:
	| Use GETSTRING to read one byte from channel 0.
	movem.l %d2-%d3/%a0,-(%sp)	| preserve callee-saved registers we touch

inbyte_retry:
	move.l #SYSCALL_NUM_GETSTRING,%d0	| system call number
	moveq	#0,%d1			| channel 0
	lea	inbyte_buf,%a0		| 1-byte buffer
	move.l %a0,%d2			| destination buffer address
	moveq	#1,%d3			| request size = 1 byte
	trap	#0
	tst.l	%d0			| did we get a byte?
	beq	inbyte_retry		| retry until one byte received

	moveq	#0,%d0			| clear upper bits
	move.b	inbyte_buf,%d0		| load received byte

	movem.l (%sp)+,%d2-%d3/%a0	| restore registers
	rts

.section .bss
.even
inbyte_buf:
	.ds.b 1				| single byte input buffer
.even
