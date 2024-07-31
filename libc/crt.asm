_start:	mov %rsp, %rdi
	mov 8(%rsp), %rsi
	and $0xfffffffffffffff0, %rsp
	call main
	.global _start
