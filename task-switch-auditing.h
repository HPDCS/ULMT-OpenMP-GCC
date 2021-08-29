#if _LIBGOMP_TASK_SWITCH_AUDITING_

#ifndef RDTSC
#define RDTSC() ({ \
  unsigned int cycles_low; \
  unsigned int cycles_high; \
  asm volatile ( \
    "RDTSC\n\t" \
    "mov %%edx, %0\n\t" \
    "mov %%eax, %1\n\t" \
    : \
    "=r" (cycles_high), "=r" (cycles_low) \
    : \
    : \
    "%rax", "%rdx" \
  ); \
  (((uint64_t) cycles_high << 32) | cycles_low); \
})
#endif

extern struct gomp_task_icv gomp_global_icv;

#if _LIBGOMP_IN_FLOW_TASK_SWITCH_AUDITING_
struct gomp_inflow_task_switch_audit
{
	unsigned int number_of_invocations;
  unsigned int number_of_implicit_suspension_for_tied_resume;
  unsigned int number_of_implicit_suspension_for_suspended_tied_resume;
	unsigned int number_of_implicit_suspension_for_untied_resume;
  unsigned int number_of_implicit_suspension_for_suspended_untied_resume;
  unsigned int number_of_untied_suspension_for_tied_resume;
  unsigned int number_of_untied_suspension_for_suspended_tied_resume;
  unsigned int number_of_untied_suspension_for_untied_resume;
  unsigned int number_of_untied_suspension_for_suspended_untied_resume;
  unsigned int number_of_untied_suspension_for_implicit_resume;
  unsigned int number_of_tied_suspension_for_tied_resume;
  unsigned int number_of_tied_suspension_for_suspended_tied_resume;
  unsigned int number_of_tied_suspension_for_untied_resume;
  unsigned int number_of_tied_suspension_for_suspended_untied_resume;
  unsigned int number_of_tied_suspension_for_implicit_resume;
};
#endif

struct gomp_critical_task_switch_audit
{
  unsigned int number_of_invocations;
  unsigned int number_of_untied_suspension_for_tied_resume;
  unsigned int number_of_untied_suspension_for_suspended_tied_resume;
  unsigned int number_of_untied_suspension_for_untied_resume;
  unsigned int number_of_untied_suspension_for_suspended_untied_resume;
  unsigned int number_of_tied_suspension_for_tied_resume;
  unsigned int number_of_tied_suspension_for_suspended_tied_resume;
  unsigned int number_of_tied_suspension_for_untied_resume;
  unsigned int number_of_tied_suspension_for_suspended_untied_resume;
};

struct gomp_blocked_task_switch_audit
{
  unsigned int number_of_invocations;
  unsigned int number_of_untied_suspension_for_tied_resume;
  unsigned int number_of_untied_suspension_for_suspended_tied_resume;
  unsigned int number_of_untied_suspension_for_untied_resume;
  unsigned int number_of_untied_suspension_for_suspended_untied_resume;
  unsigned int number_of_tied_suspension_for_tied_resume;
  unsigned int number_of_tied_suspension_for_suspended_tied_resume;
  unsigned int number_of_tied_suspension_for_untied_resume;
  unsigned int number_of_tied_suspension_for_suspended_untied_resume;
};

struct gomp_interrupt_task_switch_audit
{
  unsigned int number_of_invocations;
	unsigned int number_of_untied_suspension_for_tied_resume;
	unsigned int number_of_untied_suspension_for_suspended_tied_resume;
  unsigned int number_of_untied_suspension_for_untied_resume;
  unsigned int number_of_untied_suspension_for_suspended_untied_resume;
	unsigned int number_of_tied_suspension_for_tied_resume;
  unsigned int number_of_tied_suspension_for_suspended_tied_resume;
	unsigned int number_of_tied_suspension_for_untied_resume;
  unsigned int number_of_tied_suspension_for_suspended_untied_resume;
  /* Such statistics are populated only if gomp_ipi_var is enabled. */
  unsigned int number_of_ipi_syscall;
  unsigned int number_of_ipi_sent;
};

struct gomp_voluntary_task_switch_audit
{
  unsigned int number_of_invocations;
  unsigned int number_of_untied_suspension_for_tied_resume;
  unsigned int number_of_untied_suspension_for_suspended_tied_resume;
  unsigned int number_of_untied_suspension_for_untied_resume;
  unsigned int number_of_untied_suspension_for_suspended_untied_resume;
  unsigned int number_of_tied_suspension_for_tied_resume;
  unsigned int number_of_tied_suspension_for_suspended_tied_resume;
  unsigned int number_of_tied_suspension_for_untied_resume;
  unsigned int number_of_tied_suspension_for_suspended_untied_resume;
};

struct gomp_undeferred_task_switch_audit
{
  unsigned int number_of_invocations;
  unsigned int number_of_untied_suspension_for_tied_resume;
  unsigned int number_of_untied_suspension_for_suspended_tied_resume;
  unsigned int number_of_untied_suspension_for_untied_resume;
  unsigned int number_of_untied_suspension_for_suspended_untied_resume;
  unsigned int number_of_tied_suspension_for_tied_resume;
  unsigned int number_of_tied_suspension_for_suspended_tied_resume;
  unsigned int number_of_tied_suspension_for_untied_resume;
  unsigned int number_of_tied_suspension_for_suspended_untied_resume;
};

struct gomp_task_switch_audit
{
#if _LIBGOMP_IN_FLOW_TASK_SWITCH_AUDITING_
	struct gomp_inflow_task_switch_audit inflow_task_switch;
#endif
  struct gomp_critical_task_switch_audit critical_task_switch;
  struct gomp_blocked_task_switch_audit blocked_task_switch;
	struct gomp_interrupt_task_switch_audit interrupt_task_switch;
  struct gomp_voluntary_task_switch_audit voluntary_task_switch;
  struct gomp_undeferred_task_switch_audit undeferred_task_switch;
};

static inline __attribute__((always_inline)) void
gomp_init_task_switch_audit (struct gomp_task_switch_audit **audit)
{
  if ((*audit) == NULL)
    (*audit) = gomp_malloc (sizeof(struct gomp_task_switch_audit));
  memset((*audit), 0, sizeof(struct gomp_task_switch_audit));
}

#if _LIBGOMP_IN_FLOW_TASK_SWITCH_AUDITING_
static inline __attribute__((always_inline)) void
gomp_save_inflow_task_switch (struct gomp_task_switch_audit *audit,
          struct gomp_task *prev_task, struct gomp_task *next_task)
{
  audit->inflow_task_switch.number_of_invocations += 1;

  if (prev_task->kind == GOMP_TASK_IMPLICIT)
  {
    if (next_task->type == GOMP_TASK_TYPE_TIED)
    {
      if (next_task->suspending_thread != NULL)
        audit->inflow_task_switch.number_of_implicit_suspension_for_suspended_tied_resume += 1;
      else
        audit->inflow_task_switch.number_of_implicit_suspension_for_tied_resume += 1;
    }
    else
    {
      if (next_task->suspending_thread != NULL)
        audit->inflow_task_switch.number_of_implicit_suspension_for_suspended_untied_resume += 1;
      else
        audit->inflow_task_switch.number_of_implicit_suspension_for_untied_resume += 1;
    }
  }
  else if (prev_task->type == GOMP_TASK_TYPE_TIED)
  {
    if (next_task->kind == GOMP_TASK_IMPLICIT)
    {
      audit->inflow_task_switch.number_of_tied_suspension_for_implicit_resume += 1;
    }
    else if (next_task->type == GOMP_TASK_TYPE_TIED)
    {
      if (next_task->suspending_thread != NULL)
        audit->inflow_task_switch.number_of_tied_suspension_for_suspended_tied_resume += 1;
      else
        audit->inflow_task_switch.number_of_tied_suspension_for_tied_resume += 1;
    }
    else
    {
      if (next_task->suspending_thread != NULL)
        audit->inflow_task_switch.number_of_tied_suspension_for_suspended_untied_resume += 1;
      else
        audit->inflow_task_switch.number_of_tied_suspension_for_untied_resume += 1;
    }
  }
  else
  {
    if (next_task->kind == GOMP_TASK_IMPLICIT)
    {
      audit->inflow_task_switch.number_of_untied_suspension_for_implicit_resume += 1;
    }
    else if (next_task->type == GOMP_TASK_TYPE_TIED)
    {
      if (next_task->suspending_thread != NULL)
        audit->inflow_task_switch.number_of_untied_suspension_for_suspended_tied_resume += 1;
      else
        audit->inflow_task_switch.number_of_untied_suspension_for_tied_resume += 1;
    }
    else
    {
      if (next_task->suspending_thread != NULL)
        audit->inflow_task_switch.number_of_untied_suspension_for_suspended_untied_resume += 1;
      else
        audit->inflow_task_switch.number_of_untied_suspension_for_untied_resume += 1;
    }
  }
}
#endif

static inline __attribute__((always_inline)) void
gomp_print_task_switch_audit (struct gomp_task_switch_audit **audit, unsigned nthreads)
{
  unsigned i;
  size_t length;
  time_t rawtime;
  struct tm *timeinfo;
  char filename[128];
  struct gomp_task_switch_audit local;

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  if (gomp_global_icv.ult_var)
    length = strftime(filename, 128, "ult_omp_task_switch_occurrences_%d-%m-%Y_%H-%M-%S-", timeinfo);
  else
    length = strftime(filename, 128, "bsl_omp_task_switch_occurrences_%d-%m-%Y_%H-%M-%S-", timeinfo);
  snprintf(&filename[length], (128 - length), "(%llu).txt", (unsigned long long int) RDTSC());

  memset(&local, 0, sizeof(struct gomp_task_switch_audit));
  for (i = 0; i < nthreads; i++)
  	if (audit[i] != NULL)
  	{
#if _LIBGOMP_IN_FLOW_TASK_SWITCH_AUDITING_
      local.inflow_task_switch.number_of_invocations += audit[i]->inflow_task_switch.number_of_invocations;
      local.inflow_task_switch.number_of_implicit_suspension_for_tied_resume += audit[i]->inflow_task_switch.number_of_implicit_suspension_for_tied_resume;
      local.inflow_task_switch.number_of_implicit_suspension_for_suspended_tied_resume += audit[i]->inflow_task_switch.number_of_implicit_suspension_for_suspended_tied_resume;
      local.inflow_task_switch.number_of_implicit_suspension_for_untied_resume += audit[i]->inflow_task_switch.number_of_implicit_suspension_for_untied_resume;
      local.inflow_task_switch.number_of_implicit_suspension_for_suspended_untied_resume += audit[i]->inflow_task_switch.number_of_implicit_suspension_for_suspended_untied_resume;
      local.inflow_task_switch.number_of_untied_suspension_for_tied_resume += audit[i]->inflow_task_switch.number_of_untied_suspension_for_tied_resume;
      local.inflow_task_switch.number_of_untied_suspension_for_suspended_tied_resume += audit[i]->inflow_task_switch.number_of_untied_suspension_for_suspended_tied_resume;
      local.inflow_task_switch.number_of_untied_suspension_for_untied_resume += audit[i]->inflow_task_switch.number_of_untied_suspension_for_untied_resume;
      local.inflow_task_switch.number_of_untied_suspension_for_suspended_untied_resume += audit[i]->inflow_task_switch.number_of_untied_suspension_for_suspended_untied_resume;
      local.inflow_task_switch.number_of_untied_suspension_for_implicit_resume += audit[i]->inflow_task_switch.number_of_untied_suspension_for_implicit_resume;
      local.inflow_task_switch.number_of_tied_suspension_for_tied_resume += audit[i]->inflow_task_switch.number_of_tied_suspension_for_tied_resume;
      local.inflow_task_switch.number_of_tied_suspension_for_suspended_tied_resume += audit[i]->inflow_task_switch.number_of_tied_suspension_for_suspended_tied_resume;
      local.inflow_task_switch.number_of_tied_suspension_for_untied_resume += audit[i]->inflow_task_switch.number_of_tied_suspension_for_untied_resume;
      local.inflow_task_switch.number_of_tied_suspension_for_suspended_untied_resume += audit[i]->inflow_task_switch.number_of_tied_suspension_for_suspended_untied_resume;
      local.inflow_task_switch.number_of_tied_suspension_for_implicit_resume += audit[i]->inflow_task_switch.number_of_tied_suspension_for_implicit_resume;
#endif
  		local.critical_task_switch.number_of_invocations += audit[i]->critical_task_switch.number_of_invocations;
      local.critical_task_switch.number_of_untied_suspension_for_tied_resume += audit[i]->critical_task_switch.number_of_untied_suspension_for_tied_resume;
      local.critical_task_switch.number_of_untied_suspension_for_suspended_tied_resume += audit[i]->critical_task_switch.number_of_untied_suspension_for_suspended_tied_resume;
  		local.critical_task_switch.number_of_untied_suspension_for_untied_resume += audit[i]->critical_task_switch.number_of_untied_suspension_for_untied_resume;
      local.critical_task_switch.number_of_untied_suspension_for_suspended_untied_resume += audit[i]->critical_task_switch.number_of_untied_suspension_for_suspended_untied_resume;
  		local.critical_task_switch.number_of_tied_suspension_for_tied_resume += audit[i]->critical_task_switch.number_of_tied_suspension_for_tied_resume;
      local.critical_task_switch.number_of_tied_suspension_for_suspended_tied_resume += audit[i]->critical_task_switch.number_of_tied_suspension_for_suspended_tied_resume;
      local.critical_task_switch.number_of_tied_suspension_for_untied_resume += audit[i]->critical_task_switch.number_of_tied_suspension_for_untied_resume;
      local.critical_task_switch.number_of_tied_suspension_for_suspended_untied_resume += audit[i]->critical_task_switch.number_of_tied_suspension_for_suspended_untied_resume;

      local.blocked_task_switch.number_of_invocations += audit[i]->blocked_task_switch.number_of_invocations;
      local.blocked_task_switch.number_of_untied_suspension_for_tied_resume += audit[i]->blocked_task_switch.number_of_untied_suspension_for_tied_resume;
      local.blocked_task_switch.number_of_untied_suspension_for_suspended_tied_resume += audit[i]->blocked_task_switch.number_of_untied_suspension_for_suspended_tied_resume;
      local.blocked_task_switch.number_of_untied_suspension_for_untied_resume += audit[i]->blocked_task_switch.number_of_untied_suspension_for_untied_resume;
      local.blocked_task_switch.number_of_untied_suspension_for_suspended_untied_resume += audit[i]->blocked_task_switch.number_of_untied_suspension_for_suspended_untied_resume;
      local.blocked_task_switch.number_of_tied_suspension_for_tied_resume += audit[i]->blocked_task_switch.number_of_tied_suspension_for_tied_resume;
      local.blocked_task_switch.number_of_tied_suspension_for_suspended_tied_resume += audit[i]->blocked_task_switch.number_of_tied_suspension_for_suspended_tied_resume;
      local.blocked_task_switch.number_of_tied_suspension_for_untied_resume += audit[i]->blocked_task_switch.number_of_tied_suspension_for_untied_resume;
      local.blocked_task_switch.number_of_tied_suspension_for_suspended_untied_resume += audit[i]->blocked_task_switch.number_of_tied_suspension_for_suspended_untied_resume;

  		local.interrupt_task_switch.number_of_invocations += audit[i]->interrupt_task_switch.number_of_invocations;
      local.interrupt_task_switch.number_of_untied_suspension_for_tied_resume += audit[i]->interrupt_task_switch.number_of_untied_suspension_for_tied_resume;
      local.interrupt_task_switch.number_of_untied_suspension_for_suspended_tied_resume += audit[i]->interrupt_task_switch.number_of_untied_suspension_for_suspended_tied_resume;
  		local.interrupt_task_switch.number_of_untied_suspension_for_untied_resume += audit[i]->interrupt_task_switch.number_of_untied_suspension_for_untied_resume;
      local.interrupt_task_switch.number_of_untied_suspension_for_suspended_untied_resume += audit[i]->interrupt_task_switch.number_of_untied_suspension_for_suspended_untied_resume;
  		local.interrupt_task_switch.number_of_tied_suspension_for_tied_resume += audit[i]->interrupt_task_switch.number_of_tied_suspension_for_tied_resume;
      local.interrupt_task_switch.number_of_tied_suspension_for_suspended_tied_resume += audit[i]->interrupt_task_switch.number_of_tied_suspension_for_suspended_tied_resume;
  		local.interrupt_task_switch.number_of_tied_suspension_for_untied_resume += audit[i]->interrupt_task_switch.number_of_tied_suspension_for_untied_resume;
      local.interrupt_task_switch.number_of_tied_suspension_for_suspended_untied_resume += audit[i]->interrupt_task_switch.number_of_tied_suspension_for_suspended_untied_resume;
      local.interrupt_task_switch.number_of_ipi_syscall += audit[i]->interrupt_task_switch.number_of_ipi_syscall;
      local.interrupt_task_switch.number_of_ipi_sent += audit[i]->interrupt_task_switch.number_of_ipi_sent;

      local.voluntary_task_switch.number_of_invocations += audit[i]->voluntary_task_switch.number_of_invocations;
      local.voluntary_task_switch.number_of_untied_suspension_for_tied_resume += audit[i]->voluntary_task_switch.number_of_untied_suspension_for_tied_resume;
      local.voluntary_task_switch.number_of_untied_suspension_for_suspended_tied_resume += audit[i]->voluntary_task_switch.number_of_untied_suspension_for_suspended_tied_resume;
      local.voluntary_task_switch.number_of_untied_suspension_for_untied_resume += audit[i]->voluntary_task_switch.number_of_untied_suspension_for_untied_resume;
      local.voluntary_task_switch.number_of_untied_suspension_for_suspended_untied_resume += audit[i]->voluntary_task_switch.number_of_untied_suspension_for_suspended_untied_resume;
      local.voluntary_task_switch.number_of_tied_suspension_for_tied_resume += audit[i]->voluntary_task_switch.number_of_tied_suspension_for_tied_resume;
      local.voluntary_task_switch.number_of_tied_suspension_for_suspended_tied_resume += audit[i]->voluntary_task_switch.number_of_tied_suspension_for_suspended_tied_resume;
      local.voluntary_task_switch.number_of_tied_suspension_for_untied_resume += audit[i]->voluntary_task_switch.number_of_tied_suspension_for_untied_resume;
      local.voluntary_task_switch.number_of_tied_suspension_for_suspended_untied_resume += audit[i]->voluntary_task_switch.number_of_tied_suspension_for_suspended_untied_resume;

      local.undeferred_task_switch.number_of_invocations += audit[i]->undeferred_task_switch.number_of_invocations;
      local.undeferred_task_switch.number_of_untied_suspension_for_tied_resume += audit[i]->undeferred_task_switch.number_of_untied_suspension_for_tied_resume;
      local.undeferred_task_switch.number_of_untied_suspension_for_suspended_tied_resume += audit[i]->undeferred_task_switch.number_of_untied_suspension_for_suspended_tied_resume;
      local.undeferred_task_switch.number_of_untied_suspension_for_untied_resume += audit[i]->undeferred_task_switch.number_of_untied_suspension_for_untied_resume;
      local.undeferred_task_switch.number_of_untied_suspension_for_suspended_untied_resume += audit[i]->undeferred_task_switch.number_of_untied_suspension_for_suspended_untied_resume;
      local.undeferred_task_switch.number_of_tied_suspension_for_tied_resume += audit[i]->undeferred_task_switch.number_of_tied_suspension_for_tied_resume;
      local.undeferred_task_switch.number_of_tied_suspension_for_suspended_tied_resume += audit[i]->undeferred_task_switch.number_of_tied_suspension_for_suspended_tied_resume;
      local.undeferred_task_switch.number_of_tied_suspension_for_untied_resume += audit[i]->undeferred_task_switch.number_of_tied_suspension_for_untied_resume;
      local.undeferred_task_switch.number_of_tied_suspension_for_suspended_untied_resume += audit[i]->undeferred_task_switch.number_of_tied_suspension_for_suspended_untied_resume;
  	}

  FILE *file = fopen(filename, "w");
#if _LIBGOMP_IN_FLOW_TASK_SWITCH_AUDITING_
  fprintf(file, "\n[In-Flow Task Switch]\n"
          "Number of Calls:                                 %u\n"
          "Implicit Suspension for Tied Resume:             %u\n"
          "Implicit Suspension for Suspended Tied Resume:   %u\n"
          "Implicit Suspension for Untied Resume:           %u\n"
          "Implicit Suspension for Suspended Untied Resume: %u\n"
          "Untied Suspension for Tied Resume:               %u\n"
          "Untied Suspension for Suspended Tied Resume:     %u\n"
          "Untied Suspension for Untied Resume:             %u\n"
          "Untied Suspension for Suspended Untied Resume:   %u\n"
          "Untied Suspension for Implicit Resume:           %u\n"
          "Tied Suspension for Tied Resume:                 %u\n"
          "Tied Suspension for Suspended Tied Resume:       %u\n"
          "Tied Suspension for Untied Resume:               %u\n"
          "Tied Suspension for Suspended Untied Resume:     %u\n"
          "Tied Suspension for Implicit Resume:             %u\n",
          local.inflow_task_switch.number_of_invocations,
          local.inflow_task_switch.number_of_implicit_suspension_for_tied_resume,
          local.inflow_task_switch.number_of_implicit_suspension_for_suspended_tied_resume,
          local.inflow_task_switch.number_of_implicit_suspension_for_untied_resume,
          local.inflow_task_switch.number_of_implicit_suspension_for_suspended_untied_resume,
          local.inflow_task_switch.number_of_untied_suspension_for_tied_resume,
          local.inflow_task_switch.number_of_untied_suspension_for_suspended_tied_resume,
          local.inflow_task_switch.number_of_untied_suspension_for_untied_resume,
          local.inflow_task_switch.number_of_untied_suspension_for_suspended_untied_resume,
          local.inflow_task_switch.number_of_untied_suspension_for_implicit_resume,
          local.inflow_task_switch.number_of_tied_suspension_for_tied_resume,
          local.inflow_task_switch.number_of_tied_suspension_for_suspended_tied_resume,
          local.inflow_task_switch.number_of_tied_suspension_for_untied_resume,
          local.inflow_task_switch.number_of_tied_suspension_for_suspended_untied_resume,
          local.inflow_task_switch.number_of_tied_suspension_for_implicit_resume);
#endif
  fprintf(file, "\n[Critical Task Switch]\n"
          "Number of Calls:                               %u\n"
          "Untied Suspension for Tied Resume:             %u\n"
          "Untied Suspension for Suspended Tied Resume:   %u\n"
          "Untied Suspension for Untied Resume:           %u\n"
          "Untied Suspension for Suspended Untied Resume: %u\n"
          "Tied Suspension for Tied Resume:               %u\n"
          "Tied Suspension for Suspended Tied Resume:     %u\n"
          "Tied Suspension for Untied Resume:             %u\n"
          "Tied Suspension for Suspended Untied Resume:   %u\n",
          local.critical_task_switch.number_of_invocations,
          local.critical_task_switch.number_of_untied_suspension_for_tied_resume,
          local.critical_task_switch.number_of_untied_suspension_for_suspended_tied_resume,
          local.critical_task_switch.number_of_untied_suspension_for_untied_resume,
          local.critical_task_switch.number_of_untied_suspension_for_suspended_untied_resume,
          local.critical_task_switch.number_of_tied_suspension_for_tied_resume,
          local.critical_task_switch.number_of_tied_suspension_for_suspended_tied_resume,
          local.critical_task_switch.number_of_tied_suspension_for_untied_resume,
          local.critical_task_switch.number_of_tied_suspension_for_suspended_untied_resume);
  fprintf(file, "\n[Blocked Task Switch]\n"
          "Number of Calls:                               %u\n"
          "Untied Suspension for Tied Resume:             %u\n"
          "Untied Suspension for Suspended Tied Resume:   %u\n"
          "Untied Suspension for Untied Resume:           %u\n"
          "Untied Suspension for Suspended Untied Resume: %u\n"
          "Tied Suspension for Tied Resume:               %u\n"
          "Tied Suspension for Suspended Tied Resume:     %u\n"
          "Tied Suspension for Untied Resume:             %u\n"
          "Tied Suspension for Suspended Untied Resume:   %u\n",
          local.blocked_task_switch.number_of_invocations,
          local.blocked_task_switch.number_of_untied_suspension_for_tied_resume,
          local.blocked_task_switch.number_of_untied_suspension_for_suspended_tied_resume,
          local.blocked_task_switch.number_of_untied_suspension_for_untied_resume,
          local.blocked_task_switch.number_of_untied_suspension_for_suspended_untied_resume,
          local.blocked_task_switch.number_of_tied_suspension_for_tied_resume,
          local.blocked_task_switch.number_of_tied_suspension_for_suspended_tied_resume,
          local.blocked_task_switch.number_of_tied_suspension_for_untied_resume,
          local.blocked_task_switch.number_of_tied_suspension_for_suspended_untied_resume);
  fprintf(file, "\n[Interrupt Task Switch]\n"
          "Number of IPI Syscalls:                        %u\n"
          "Number of IPIs sent:                           %u\n"
          "Number of Calls:                               %u\n"
          "Untied Suspension for Tied Resume:             %u\n"
          "Untied Suspension for Suspended Tied Resume:   %u\n"
          "Untied Suspension for Untied Resume:           %u\n"
          "Untied Suspension for Suspended Untied Resume: %u\n"
          "Tied Suspension for Tied Resume:               %u\n"
          "Tied Suspension for Suspended Tied Resume:     %u\n"
          "Tied Suspension for Untied Resume:             %u\n"
          "Tied Suspension for Suspended Untied Resume:   %u\n",
          local.interrupt_task_switch.number_of_ipi_syscall,
          local.interrupt_task_switch.number_of_ipi_sent,
          local.interrupt_task_switch.number_of_invocations,
          local.interrupt_task_switch.number_of_untied_suspension_for_tied_resume,
          local.interrupt_task_switch.number_of_untied_suspension_for_suspended_tied_resume,
          local.interrupt_task_switch.number_of_untied_suspension_for_untied_resume,
          local.interrupt_task_switch.number_of_untied_suspension_for_suspended_untied_resume,
          local.interrupt_task_switch.number_of_tied_suspension_for_tied_resume,
          local.interrupt_task_switch.number_of_tied_suspension_for_suspended_tied_resume,
          local.interrupt_task_switch.number_of_tied_suspension_for_untied_resume,
          local.interrupt_task_switch.number_of_tied_suspension_for_suspended_untied_resume);
  fprintf(file, "\n[Voluntary Task Switch]\n"
          "Number of Calls:                               %u\n"
          "Untied Suspension for Tied Resume:             %u\n"
          "Untied Suspension for Suspended Tied Resume:   %u\n"
          "Untied Suspension for Untied Resume:           %u\n"
          "Untied Suspension for Suspended Untied Resume: %u\n"
          "Tied Suspension for Tied Resume:               %u\n"
          "Tied Suspension for Suspended Tied Resume:     %u\n"
          "Tied Suspension for Untied Resume:             %u\n"
          "Tied Suspension for Suspended Untied Resume:   %u\n",
          local.voluntary_task_switch.number_of_invocations,
          local.voluntary_task_switch.number_of_untied_suspension_for_tied_resume,
          local.voluntary_task_switch.number_of_untied_suspension_for_suspended_tied_resume,
          local.voluntary_task_switch.number_of_untied_suspension_for_untied_resume,
          local.voluntary_task_switch.number_of_untied_suspension_for_suspended_untied_resume,
          local.voluntary_task_switch.number_of_tied_suspension_for_tied_resume,
          local.voluntary_task_switch.number_of_tied_suspension_for_suspended_tied_resume,
          local.voluntary_task_switch.number_of_tied_suspension_for_untied_resume,
          local.voluntary_task_switch.number_of_tied_suspension_for_suspended_untied_resume);
  fprintf(file, "\n[Undeferred Task Switch]\n"
          "Number of Calls:                               %u\n"
          "Untied Suspension for Tied Resume:             %u\n"
          "Untied Suspension for Suspended Tied Resume:   %u\n"
          "Untied Suspension for Untied Resume:           %u\n"
          "Untied Suspension for Suspended Untied Resume: %u\n"
          "Tied Suspension for Tied Resume:               %u\n"
          "Tied Suspension for Suspended Tied Resume:     %u\n"
          "Tied Suspension for Untied Resume:             %u\n"
          "Tied Suspension for Suspended Untied Resume:   %u\n",
          local.undeferred_task_switch.number_of_invocations,
          local.undeferred_task_switch.number_of_untied_suspension_for_tied_resume,
          local.undeferred_task_switch.number_of_untied_suspension_for_suspended_tied_resume,
          local.undeferred_task_switch.number_of_untied_suspension_for_untied_resume,
          local.undeferred_task_switch.number_of_untied_suspension_for_suspended_untied_resume,
          local.undeferred_task_switch.number_of_tied_suspension_for_tied_resume,
          local.undeferred_task_switch.number_of_tied_suspension_for_suspended_tied_resume,
          local.undeferred_task_switch.number_of_tied_suspension_for_untied_resume,
          local.undeferred_task_switch.number_of_tied_suspension_for_suspended_untied_resume);
  fflush(file);
  fclose(file);
}

#endif