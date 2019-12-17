#ifndef GOMP_CONTEXT_H
#define GOMP_CONTEXT_H 1

/* Preamble for the *setjmp* function that resolves
   caller-save/non-callee-save mismatch. */
#define set_jmp(env)		({\
								int _set_ret; \
								__asm__ __volatile__ ("pushq %rdi"); \
								_set_ret = _set_jmp(env); \
								__asm__ __volatile__ ("add $8, %rsp"); \
								_set_ret; \
							 })

/* Preamble for the *longjmp* function with the only
   purpose of being compliant to set_jmp. */
#define long_jmp(env, val)	_long_jmp(env, val)

/* Save machine context for userspace context switch. */
#define	context_save(context) set_jmp(context)

/* Restore machine context for userspace context switch. */
#define	context_restore(context) long_jmp(context, 1)

/* Swicth machine context for userspace context switch. */
#define	context_switch(context_old, context_new) \
			if (set_jmp(context_old) == 0) \
				long_jmp(context_new, 1)

/* Setup machine context for userspace context switch. */
#define gomp_context_create(creator, created, stack, stack_size) \
      _setup_jmp(creator, created, stack, stack_size)


/* This structure maintains content of all the CPU registers.  */

struct gomp_task_context {
  /* This is the space for general purpose registers. */
  unsigned long long rax;
  unsigned long long rdx;
  unsigned long long rcx;
  unsigned long long rbx;
  unsigned long long rsp;
  unsigned long long rbp;
  unsigned long long rsi;
  unsigned long long rdi;
  unsigned long long r8;
  unsigned long long r9;
  unsigned long long r10;
  unsigned long long r11;
  unsigned long long r12;
  unsigned long long r13;
  unsigned long long r14;
  unsigned long long r15;
  unsigned long long rip;
  unsigned long long flags;
  /* Space for other registers. */
  unsigned char others[512] __attribute__((aligned(16)));
};


/* jmp.S */

extern long long _set_jmp(struct gomp_task_context*);
extern void _long_jmp(struct gomp_task_context*, long long) __attribute__ ((__noreturn__));
extern void _setup_jmp(struct gomp_task_context *, struct gomp_task_context *, void *, size_t);

/* context.c */

extern void context_create_boot(struct gomp_task_context *, struct gomp_task_context *) __attribute__ ((noreturn));

#endif /* GOMP_CONTEXT_H */