	.section	__TEXT,__text
	.globl	_main
_main:
	movk x0, #0x6948, LSL#00
	movk x0, #0x4620, LSL#16
	movk x0, #0x6172, LSL#32
	movk x0, #0x646e, LSL#48
	str x0, [sp, #-(8*2)]!
	mov x0, #1  ; fd
	mov x1, sp;
	mov x2, #8  ; strlen
	mov x16, #4 ; syscall write
	svc #0x80
	mov x0, #42 ; exit code
	mov x16, #1 ; syscall exit
	svc #0x80

