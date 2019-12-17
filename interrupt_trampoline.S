.file	"interrupt_trampoline.S"
		.text

.globl	gomp_interrupt_trampoline
		.type	gomp_interrupt_trampoline, @function

gomp_interrupt_trampoline:

	# save RAX
	pushq		%rax

	# save FLAGS
	lahf
	seto		%al
	pushq		%rax

	# save RBX
	pushq		%rbx

	# %rax <-- &gomp_global_sfo
	leaq		gomp_global_sfo(%rip), %rax
	# %rbx <-- &gomp_tls_data
	movq		$0x0, %rbx
	movq		%gs:(%rbx), %rbx
	
	# save RCX
	pushq		%rcx

	# %rcx <- (&gomp_global_sfo)->in_thread_team_offset
	movq		(%rax), %rcx
	# %rcx <- (&gomp_tls_data)->ts.team (it's a pointer)
	movq		(%rbx, %rcx, 1), %rcx
	# ((&gomp_tls_data)->ts.team == NULL) ?
	testq		%rcx, %rcx
	je			.L5

	# %rcx <- (&gomp_global_sfo)->in_thread_preemptable_offset
	movq		8(%rax), %rcx
	# %rcx <- (&gomp_tls_data)->non_preemptable
	movq		(%rbx, %rcx, 1), %rcx
	# ((&gomp_tls_data)->non_preemptable > 0) ?
	testq		%rcx, %rcx
	jne			.L5

	# %rcx <- (&gomp_global_sfo)->in_thread_task_offset
	movq		16(%rax), %rcx
	# %rcx <- (&gomp_tls_data)->task (it's a pointer)
	movq		(%rbx, %rcx, 1), %rcx
	# ((&gomp_tls_data)->task == NULL) ?
	testq		%rcx, %rcx
	je			.L5

	# save RSI
	pushq		%rsi

	# %rsi <- (&gomp_global_sfo)->in_task_kind_offset
	movq		24(%rax), %rsi
	# %esi <- (&gomp_tls_data)->task->kind
	movl		(%rcx, %rsi, 1), %esi
	# (&gomp_tls_data)->task->kind == GOMP_TASK_IMPLICIT ?
	testl		%esi, %esi
	je			.L4

	# save RDI
	pushq		%rdi

	# %rdi <- (&gomp_global_sfo)->in_task_state_offset
	movq		32(%rax), %rdi
	# %rsi <- (&gomp_tls_data)->task->state (it's a pointer)
	movq		(%rcx, %rdi, 1), %rsi
	# ((&gomp_tls_data)->task->state == NULL) ?
	testq		%rsi, %rsi
	je			.L3

	# %rdi <- (&gomp_global_sfo)->in_state_context_offset
	movq		40(%rax), %rdi
	# %rdi <- (&gomp_tls_data)->task->state->context
	leaq		(%rsi, %rdi, 1), %rdi

	call		_set_jmp

	# _set_jmp return 0: we have just saved current context for the first time,
	#                    so we give control to the scheduler in order to check
	#                    whether higher priority tasks are available.
	testl		%eax, %eax
	je			.L1

	# _set_jmp return 1: this context is resumed by a possible different thread
	#                    in a second time, and we need therefore to perform some
	#                    activities that depend on the task state at the time it
	#                    has been suspended.
	cmpl		$0x1, %eax
	je			.L2

	# _set_jmp return 6887: all tasks related to the scheduling activities have
	#                       been performed, and we can safetly leave this routine.
	jmp			.L3

.L1:
	call		gomp_interrupt_task_scheduling_pre

	# Thid branch is executed if and only if "gomp_interrupt_task_scheduling_pre"
	# function invokation does ot lead to context-switch current task for another.
	# In this case we don't need to execute "gomp_interrupt_task_scheduling_post"
	# cause this context has not been resumed by anyone, and we can safetely
	# resume the interrupted flow.

	# %rax <-- &gomp_global_sfo
	leaq		gomp_global_sfo(%rip), %rax
	# %rbx <-- &gomp_tls_data
	movq		$0x0, %rbx
	movq		%gs:(%rbx), %rbx

	# %rdi <- (&gomp_global_sfo)->in_thread_task_offset
	movq		16(%rax), %rdi
	# %rsi <- (&gomp_tls_data)->task (it's a pointer)
	movq		(%rbx, %rdi, 1), %rsi

	# %rdi <- (&gomp_global_sfo)->in_task_state_offset
	movq		32(%rax), %rdi
	# %rsi <- (&gomp_tls_data)->task->state (it's a pointer)
	movq		(%rsi, %rdi, 1), %rsi

	# %rdi <- (&gomp_global_sfo)->in_state_context_offset
	movq		40(%rax), %rdi
	# %rdi <- (&gomp_tls_data)->task->state->context
	leaq		(%rsi, %rdi, 1), %rdi

	# %rsi <-- 6887
	movq		$0x1ae7, %rsi

	call		_long_jmp

.L2:
	call		gomp_interrupt_task_scheduling_post

	# Thid branch is executed when "gomp_interrupt_task_scheduling_pre" function
	# invokation has previously context-switched off this task, while it is
	# currently resumed along the execution path of the same thread or another
	# one, and we need to perform some finalization operations before resuming
	# the interrupted flow.

	# %rax <-- &gomp_global_sfo
	leaq		gomp_global_sfo(%rip), %rax
	# %rbx <-- &gomp_tls_data
	movq		$0x0, %rbx
	movq		%gs:(%rbx), %rbx

	# %rdi <- (&gomp_global_sfo)->in_thread_task_offset
	movq		16(%rax), %rdi
	# %rsi <- (&gomp_tls_data)->task (it's a pointer)
	movq		(%rbx, %rdi, 1), %rsi

	# %rdi <- (&gomp_global_sfo)->in_task_state_offset
	movq		32(%rax), %rdi
	# %rsi <- (&gomp_tls_data)->task->state (it's a pointer)
	movq		(%rsi, %rdi, 1), %rsi

	# %rdi <- (&gomp_global_sfo)->in_state_context_offset
	movq		40(%rax), %rdi
	# %rdi <- (&gomp_tls_data)->task->state->context
	leaq		(%rsi, %rdi, 1), %rdi

	# %rsi <-- 6887
	movq		$0x1ae7, %rsi

	call		_long_jmp

.L3:
	# restore RDI
	popq		%rdi

.L4:
	# restore RSI
	popq		%rsi

.L5:
	# restore RCX
	popq		%rcx

	# restore RBX
	popq		%rbx

	# restore FLAGS
	popq		%rax
	addb		$0x7f, %al
	sahf

	# restore RAX
	popq		%rax

	retq

.size	gomp_interrupt_trampoline, .-gomp_interrupt_trampoline