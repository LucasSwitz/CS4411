	.section	.text
	.globl	ctx_switch, ctx_start, ctx_entry

ctx_switch:
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	%esp, (%eax)
	movl	%edx, %esp
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret

ctx_start:
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	%esp, (%eax)
	movl	%edx, %esp
	callq	ctx_entry
