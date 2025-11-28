.global outbyte

.equ SYSCALL_NUM_PUTSTRING, 2

.text
.even

outbyte:
	move.l 4(%sp),%d0		| fetch argument (pushed as long)
	andi.l #0xff,%d0		| keep lower 8 bits

	movem.l %d2-%d3/%a0,-(%sp)	| preserve callee-saved registers
	lea	outbyte_buf,%a0
	move.b %d0,(%a0)		| store byte into buffer

outbyte_retry:
	move.l #SYSCALL_NUM_PUTSTRING,%d0 | system call number
	moveq	#0,%d1			| channel 0
	move.l %a0,%d2			| source address
	moveq	#1,%d3			| send 1 byte
	trap	#0
	cmpi.l #1,%d0			| did we queue the byte?
	bne	outbyte_retry		| retry until success

	movem.l (%sp)+,%d2-%d3/%a0	| restore registers
	rts

.section .bss
.even
outbyte_buf:
	.ds.b 1				| single byte output buffer
.even
