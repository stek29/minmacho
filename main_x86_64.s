	.section	__TEXT,__text
	.globl	start

start:
	movabsq    $0x646e617246206948, %rax
	pushq      %rax
	movq       %rsp, %rsi
	movl       $0x1, %edi
	movl       $0x8, %edx
	movl       $0x2000004, %eax
	syscall
	movl       $0x2000001, %eax
	movl       $42, %edi
	syscall
