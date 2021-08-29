/**
*			Copyright (C) 2008-2015 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
* @file jmp.S
* @brief setjmp / longjmp variant which saves all registers + setupjmp which creates a new machine context
* @author Alessandro Pellegrini, Emiliano Silvestri
* @date December, 2015
*/

.file "jmp.S"
.text

# Stack organization of this function after the first three push:
#
#    _______________
#    |     R11     |
#    |-------------|
#    |     RAX     |
#    |-------------| <-- Saved RSP points here
#    |    FLAGS    |
#    |-------------|
#    |  Ret. Addr. |
#    |-------------|
#    |   Old RDI   | <-- This is pushed by wrapping macro
#    |-------------|
#
# Arguments to this function:
#  - RDI: pointer to the CPU context where to store the current (before call) context

.align 4
.globl _set_jmp
.type _set_jmp, @function
_set_jmp:
	pushfq					# save flags
	pushq %rax 				# save rax, it will point to the context
	pushq %r11				# save r11, it will be used as the source
	movq %rdi, %rax 		# rax points to the context
	movq 8(%rsp), %r11 		# r11 keeps the 'old' rax
	movq %r11, (%rax)		# rax is the first field of the context
	movq %rdx, 8(%rax)
	movq %rcx, 16(%rax)
	movq %rbx, 24(%rax)
	movq %rsp, 32(%rax)
	addq $16, 32(%rax)		# saved rsp must point one quadword above the old return address
	movq %rbp, 40(%rax)
	movq %rsi, 48(%rax)
	movq 32(%rsp), %r11		# old 'rdi' was pushed by the surrounding macro
	movq %r11, 56(%rax)
	movq %r8, 64(%rax)
	movq %r9, 72(%rax)
	movq %r10, 80(%rax)
	movq (%rsp), %r11 		# r11 keeps the 'old' r11
	movq %r11, 88(%rax)		# r11 is the 12-th field of the context
	movq %r12, 96(%rax)
	movq %r13, 104(%rax)
	movq %r14, 112(%rax)
	movq %r15, 120(%rax)
	movq 16(%rsp), %rdx
	movq %rdx, 136(%rax)	# (%rsp) is flags

	movq 24(%rsp), %r11		# Save the original return address
	movq %r11, 128(%rax)

	# Now save other registers. fxsave wants memory aligned to 16 byte.
	# The context structure is aligned to 16 bytes. We have 18 8-byte
	# registers, so the next address is exactly the 'others' buffer.
	fxsave 144(%rax)

	addq $24, %rsp
	xorq %rax, %rax			# return 0 because the context is being created
	ret

.size   _set_jmp, .-_set_jmp


# This long_jmp version does not restore RAX. In fact, the common usage is to
# call longjmp(context, val); and RAX is set to val. This function respects this,
# so that if RAX should be restored as well, it is possible to call
# long_jmp(context, context->rax);
#
# Arguments to this function:
#  - RDI: pointer to the CPU context to restore
#  - RSI: return value of this function call
#
# The return address at the end of the function does not return control to the
# caller, rather to the instruction immediately after the set_jmp call.
#
# The organization of this long_jmp implementation works as well if the stack of
# the caller and the stack of the destination of the long jump are different.

.align 4
.globl _long_jmp
.type _long_jmp, @function
_long_jmp:
	movq %rdi, %rax			# rax points to the context

	movq 128(%rax), %r10	# This is the old return address
	movq 32(%rax), %r11		# r11 is the old rsp
	movq %r10, 8(%r11)		# restore the old return address

	movq %rsi, (%r11)		# Put on the old stack the desired return value

	movq 8(%rax), %rdx		# rdx is the second field of the context
	movq 16(%rax), %rcx
	movq 24(%rax), %rbx
	movq 32(%rax), %rsp
	movq 40(%rax), %rbp
	movq 48(%rax), %rsi
	movq 56(%rax), %rdi
	movq 64(%rax), %r8
	movq 72(%rax), %r9
	movq 80(%rax), %r10		# Finish to restore GP registers
	movq 88(%rax), %r11
	movq 96(%rax), %r12
	movq 104(%rax), %r13
	movq 112(%rax), %r14
	movq 120(%rax), %r15
	pushq 136(%rax)			# this is flags
	popfq

	# Restore other registers
	fxrstor 144(%rax)

	movq 32(%rax), %rsp		# (possibly) change stack
	popq %rax				# Set the desired return value
	ret						# do the long jump

.size   _long_jmp, .-_long_jmp


# This function is intended to replace the old fashion context creation
# by entirely cloning both the stack frame of the current function at the
# bottom of the alternate stack memory area and the current machine context
# where the BP and SP values come to be updated with the new values.
#
#  _OLD_SP____\ |_______________|
#             / |______RET______|
#               |__DISPLACEMENT_|
#               |_____NEW_SP____|
#               |_____NEW_BP____|
#               |_____OLD_SP____|  |\ CLONED   |_______________| /____NEW_SP_
#               |_____OLD_BP____|  | \         |______RET______| \
#               |___STACK_SIZE__|   \ \        |__DISPLACEMENT_|
#               |_____STACK_____|    \ \ |\    |_____NEW_SP____|
#               |____CREATED____|     \ \| \   |_____NEW_BP____|
#  _OLD_BP____\ |____CREATOR____|      \    \  |_____OLD_SP____|
#             / |_____FLAGS_____|       \   /  |_____OLD_BP____|
#               |______RBP______|        | /   |___STACK_SIZE__|
#               |______RAX______|        |/    |_____STACK_____|
#               |___Ret._Addr.__|              |____CREATED____|
#               |               |              |____CREATOR____| /____NEW_BP_
#                                                                \
#
# When execution flow associated to the new context will take control it
# will start working by accessing the cloned data, while the control flow
# associated to the old context will continue along its original path.

.align 4
.globl _setup_jmp
.type _setup_jmp, @function
_setup_jmp:

	pushq	%rax
	pushq	%rbp

	lahf
	seto	%al
	pushq	%rax

	movq	%rsp, %rbp
	subq	$0x50, %rsp
	movq	%rdi, -0x8(%rbp)	# creator
	movq	%rsi, -0x10(%rbp)	# created
	movq	%rdx, -0x18(%rbp)	# stack
	movq	%rcx, -0x20(%rbp)	# stack_size
	movq	$0x0, -0x28(%rbp)	# old_BP
	movq	$0x0, -0x30(%rbp)	# old_SP
	movq	$0x0, -0x38(%rbp)	# new_BP
	movq	$0x0, -0x40(%rbp)	# new_SP
	movq	$0x0, -0x48(%rbp)	# displacement
	movq	$0x0, -0x50(%rbp)	# ret

	xorq	%rax, %rax
	movq	-0x8(%rbp), %rdi

	pushq	%rdi
	callq	_set_jmp
	addq	$0x8, %rsp

	movq	%rax, -0x50(%rbp)	# ret
	cmpq	$0x0, %rax
	jne		.L1

	movq	$0x290, %rcx		# context_size (3rd arg)
	movq	-0x8(%rbp), %rsi	# creator (2nd arg)
	movq	-0x10(%rbp), %rdi	# created (1st arg)
	movq	0x28(%rsi), %rax	# old_BP
	movq	%rax, -0x28(%rbp)	# old_BP
	movq	0x20(%rsi), %rax	# old_SP
	movq	%rax, -0x30(%rbp)	# old_SP

	cld
	rep
	movsb

	movq	-0x28(%rbp), %rcx	# old_BP
	movq	-0x30(%rbp), %rsi	# old_SP (2nd arg)
	subq	%rsi, %rcx			# displacement (3rd arg)
	movq	-0x18(%rbp), %rdi	# stack
	movq	-0x20(%rbp), %rax	# stack_size
	addq	%rax, %rdi			# new_BP
	xorq	%rax, %rax
	movq	$0x3f, %rax
	notq	%rax
	andq	%rax, %rdi			# new_BP = new_BP & ~0x3F
	movq	%rdi, -0x38(%rbp)	# new_BP
	subq	%rcx, %rdi			# new_SP (1st arg)
	movq	%rdi, -0x40(%rbp)	# new_SP

	cld
	rep
	movsb

	movq	$0x1ae7, %rsi		# MAGIC_NUMBER (2nd arg)
	movq	-0x10(%rbp), %rdi	# created (1st arg)
	movq	-0x38(%rbp), %rax	# new_BP
	movq	%rax, 0x28(%rdi)	# created->rbp = new_BP
	movq	-0x40(%rbp), %rax	# new_SP
	movq	%rax, 0x20(%rdi)	# created->rsp = new_SP

	callq	_long_jmp

	nop

.L1:

	cmpq	$0x1ae7, -0x50(%rbp)
	jne		.L2

	movq	-0x8(%rbp), %rdi	# creator (1st arg)
	movq	-0x10(%rbp), %rsi	# created (2nd arg)

	callq	context_create_boot

	nop

.L2:

	movq	-0x20(%rbp), %rcx
	movq	-0x18(%rbp), %rdx
	movq	-0x10(%rbp), %rsi
	movq	-0x8(%rbp), %rdi
	movq	%rbp, %rsp

	popq	%rax
	addb	$0x7f, %al
	sahf

	popq	%rbp
	popq	%rax

	ret

.size   _setup_jmp, .-_setup_jmp