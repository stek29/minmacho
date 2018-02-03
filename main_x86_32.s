	.section	__TEXT,__text
	.globl	start
start:
	mov $0x646E6172, %eax
	pushl %eax
	mov $0x46206948, %eax
	push %eax
	mov %esp, %edi #; preserve address of string
	push %eax #; align
	pushl $8 #; size
	push %edi #; address of string
	pushl $1 #; length
	xor %eax, %eax #; get 0
	push %eax #; align
	movb $4, %al #; write
	int $0x80 #; actuall syscall
	pushl $42 #; exit code
	pushl $1 #; align
	movb $1, %al
	int $0x80 #; exit

