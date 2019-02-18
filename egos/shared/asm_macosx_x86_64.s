	.section	__TEXT,__text,regular,pure_instructions
	.globl		_ctx_switch, _ctx_start, _ctx_entry

_ctx_switch:
    pushq	%rbp
//	movq	%rsp, %rbp
	pushq	%rbx
    pushq	%r15
    pushq	%r14
    pushq	%r13
    pushq	%r12
    pushq	%r11
    pushq	%r10
    pushq	%r9
    pushq	%r8
    movq	%rsp, (%rdi)
    movq	%rsi, %rsp
    popq	%r8
    popq	%r9
    popq	%r10
    popq	%r11
    popq	%r12
    popq	%r13
    popq	%r14
    popq	%r15
	popq	%rbx
    popq	%rbp
    retq

_ctx_start:
    pushq	%rbp
//	movq	%rsp, %rbp
	pushq	%rbx
    pushq	%r15
    pushq	%r14
    pushq	%r13
    pushq	%r12
    pushq	%r11
    pushq	%r10
    pushq	%r9
    pushq	%r8
    movq	%rsp, (%rdi)
    movq	%rsi, %rsp
	callq	_ctx_entry
