/* The noreturn-function below implements the while-loop routine
   that allows to execute ULT tasks. The task function cannot be
   reached through function-call and does not belong to the original
   CFG. Execution of the function occurs in an alternate stack and
   has its own machine context. setjmp/longjmp allow to reach and to
   leave ULT to and from its entry and exit points. */

#include "libgomp.h"
#include <assert.h>


void context_create_boot(struct gomp_task_context *, struct gomp_task_context *) __attribute__ ((noreturn));
void
context_create_boot(struct gomp_task_context *context_caller, struct gomp_task_context *context_creat)
{
	int do_wake = 0;
	struct gomp_thread *thr = NULL;
	struct gomp_team *team = NULL;
	struct gomp_task *task = NULL;

	context_switch(context_creat, context_caller);

	do
	{
		thr = gomp_thread ();
		team = thr->ts.team;
		task = thr->task;

		if (thr->hold_team_lock)
		{
			do_wake = team->task_running_count +
					!task->in_tied_task < team->nthreads;
			
			gomp_mutex_unlock (&team->task_lock);
			thr->hold_team_lock = false;

#if _LIBGOMP_TEAM_LOCK_TIMING_
  			thr->team_lock_time->lock_time += (RDTSC() - thr->team_lock_time->entry_time);
#endif

			if (do_wake)
				gomp_team_barrier_wake (&team->barrier, 1);
		}

		if (thr->cached_state == NULL)
    		thr->cached_state = gomp_get_task_state(thr->global_task_state_group,
    			thr->local_task_state_list, task->icv.ult_stack_size);

#if _LIBGOMP_LIBGOMP_TIMING_
        thr->libgomp_time->gomp_time += (RDTSC() - thr->libgomp_time->entry_time);
#endif

		task->fn(task->fn_data);

		thr = gomp_thread ();

#if _LIBGOMP_TASK_TIMING_
		task->completion_time = RDTSC();
		gomp_save_task_time(thr->prio_task_time, task->fn, task->kind, task->type, task->priority, (task->completion_time-task->creation_time));
#endif

#if _LIBGOMP_LIBGOMP_TIMING_
        thr->libgomp_time->entry_time = RDTSC();
#endif

		thr->task = task->state->switch_task;

#if (_LIBGOMP_TASK_SWITCH_AUDITING_ && _LIBGOMP_IN_FLOW_TASK_SWITCH_AUDITING_)
		gomp_save_inflow_task_switch (thr->task_switch_audit, task, task->state->switch_task);
#endif
		context_switch(&task->state->context, &task->state->switch_task->state->context);
	}
	while(1);

	assert(0);
}