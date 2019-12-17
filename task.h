#ifndef _TASK_H_
#define _TASK_H_


#include "libgomp.h"
#include "doacross.h"


/*
 *                       Queue Insertion Policy
 *
 *  Index:  31                     16 15                     0
 *  Bitmap:  0000  0000   0000  0000   0000  0000   0000  0000
 *           ----------   ----------   ----------   ----------
 *           Team|Child   Team|Child   Team|Child   Team|Child
 *           ----------   ----------   ----------   ----------
 *            (Unused)    Unblocking   Non-Blocked  Creation
 *                        Insertion    Suspension   Insertion
 *
 *  Default: 0000  0000   1111  1111   1111  1111   0000  1111
 *            --    --    END   BEGIN  END   BEGIN  END   BEGIN
 */

#define POLICY_MASK   0xFUL

#define INS_NEW_CHILD_POLICY(bitmap)    (((bitmap) & POLICY_MASK) ?         PRIORITY_INSERT_BEGIN : PRIORITY_INSERT_END)
#define INS_NEW_GROUP_POLICY(bitmap)    ((((bitmap) >> 4) & POLICY_MASK) ?  PRIORITY_INSERT_BEGIN : PRIORITY_INSERT_END)
#define INS_NEW_TEAM_POLICY(bitmap)     ((((bitmap) >> 4) & POLICY_MASK) ?  PRIORITY_INSERT_BEGIN : PRIORITY_INSERT_END)

#define INS_SUSP_CHILD_POLICY(bitmap)   ((((bitmap) >> 8) & POLICY_MASK) ?  PRIORITY_INSERT_BEGIN : PRIORITY_INSERT_END)
#define INS_SUSP_GROUP_POLICY(bitmap)   ((((bitmap) >> 12) & POLICY_MASK) ? PRIORITY_INSERT_BEGIN : PRIORITY_INSERT_END)
#define INS_SUSP_TEAM_POLICY(bitmap)    ((((bitmap) >> 12) & POLICY_MASK) ? PRIORITY_INSERT_BEGIN : PRIORITY_INSERT_END)

#define INS_UNBLK_CHILD_POLICY(bitmap)  ((((bitmap) >> 16) & POLICY_MASK) ? PRIORITY_INSERT_BEGIN : PRIORITY_INSERT_END)
#define INS_UNBLK_GROUP_POLICY(bitmap)  ((((bitmap) >> 20) & POLICY_MASK) ? PRIORITY_INSERT_BEGIN : PRIORITY_INSERT_END)
#define INS_UNBLK_TEAM_POLICY(bitmap)   ((((bitmap) >> 20) & POLICY_MASK) ? PRIORITY_INSERT_BEGIN : PRIORITY_INSERT_END)


#define GET_NUM_CHILDREN(task)          (__atomic_load_n(&((task)->tied_children_queue.num_priority_node), MEMMODEL_ACQUIRE) + \
                                         __atomic_load_n(&((task)->untied_children_queue.num_priority_node), MEMMODEL_ACQUIRE))
#define GET_NUM_WAITING_CHILDREN(task)  (__atomic_load_n(&((task)->tied_children_queue.num_waiting_priority_node), MEMMODEL_ACQUIRE) + \
                                         __atomic_load_n(&((task)->untied_children_queue.num_waiting_priority_node), MEMMODEL_ACQUIRE))

#define IS_BLOCKED_TIED(task)           (GET_NUM_CHILDREN(task) > 0)
#define IS_NOT_BLOCKED_TIED(task)       (GET_NUM_CHILDREN(task) == 0)

#define IS_BLOCKED_UNTIED(task)         ((GET_NUM_CHILDREN(task) > 0) && (task->icv.untied_block_var || (GET_NUM_WAITING_CHILDREN(task) == 0)))
#define IS_NOT_BLOCKED_UNTIED(task)     ((GET_NUM_CHILDREN(task) == 0) || (!task->icv.untied_block_var && (GET_NUM_WAITING_CHILDREN(task) > 0)))


static inline void gomp_clear_parent (enum priority_queue_type, struct priority_queue *);
static void gomp_task_handle_depend (struct gomp_task *, struct gomp_task *, void **);
static inline size_t gomp_task_run_post_handle_depend (struct gomp_task *, struct gomp_team *);
static inline void gomp_task_run_post_remove_parent (struct gomp_task *);
static inline void gomp_task_run_post_remove_taskgroup (struct gomp_task *);


/* Given a RUNNING task into a priority queue, move it in the
   appropriate position within the same queue.  */

static inline void
priority_list_adjust_task (enum priority_queue_type type, struct priority_list *list,
                          struct gomp_task *task, bool task_is_parent_depends_on)
{
  struct priority_node *node = task_to_priority_node (type, task);
  priority_list_remove (type, list, node, false, MEMMODEL_RELAXED);
  /* TODO:  as soon as we are using the same blocking conditions used for TIED
            tasks, the comparison below loses any sense. It can be removed.  */
  if (task->taskwait && task->taskwait->in_taskwait && GET_NUM_CHILDREN(task) > 0)
    priority_list_insert (type, list, node, PRIORITY_INSERT_END, task_is_parent_depends_on, false);
  else
  {
    if (type == PQ_CHILDREN_UNTIED)
      priority_list_insert (type, list, node, INS_SUSP_CHILD_POLICY(gomp_queue_policy_var), task_is_parent_depends_on, false);
    else
      priority_list_insert (type, list, node, INS_SUSP_GROUP_POLICY(gomp_queue_policy_var), task_is_parent_depends_on, false);
  }
}


/* Tree version of priority_*_adjust.  */

static inline void
priority_tree_adjust_task (enum priority_queue_type type, struct priority_queue *head,
              struct gomp_task *task, int priority, bool task_is_parent_depends_on)
{
  struct priority_list *list = priority_queue_lookup_priority (head, priority);
  priority_list_adjust_task (type, list, task, task_is_parent_depends_on);
  /* HP-LIST */
  if (prio_splay_tree_hp_list_mark_node (&head->t, (prio_splay_tree_node) list, true))
    __atomic_store_n(&head->highest_priority, head->t.highest_marked->key.l.priority, MEMMODEL_RELEASE);
}


/* Generic version of priority_*_adjust. This function allows to suspend in
   Children's queue an UNTIED task that was running so as to allow this thread
   or other threads to resume it later.

   @INPUT (non of them can be NULL)
      type:                       team's or children's queue
      head:                       the head of the queue
      task:                       the UNTIED task to suspend
      task_is_parent_depends_on   this task's parent is waiting for task termination or not

   @CONDITION
      task->type:   must be UNTIED  */

static inline void
priority_queue_adjust_task (enum priority_queue_type type, struct priority_queue *head,
                                struct gomp_task *task, bool task_is_parent_depends_on)
{
#if _LIBGOMP_CHECKING_
  if (!priority_queue_task_in_queue_p (type, head, task))
    gomp_fatal ("Attempt to adjust non-existing task %p", task);
#endif
  if (priority_queue_multi_p (head) || __builtin_expect (task->priority > 0, 0))
    priority_tree_adjust_task (type, head, task, task->priority, task_is_parent_depends_on);
  else
    priority_list_adjust_task (type, &head->l, task, task_is_parent_depends_on);
  __atomic_store_n(&head->num_waiting_priority_node, head->num_waiting_priority_node+1, MEMMODEL_RELEASE);
}


/* Given a task into non-blocked priority queue,
   move it onto a blocked priority queue.  */

static inline void
priority_list_block_task (enum priority_queue_type type, struct priority_list *list, struct priority_node *node)
{
  priority_list_remove (type, list, node, false, MEMMODEL_RELAXED);
  priority_list_insert (type, list, node, PRIORITY_INSERT_BEGIN /* Don't care */, false /* Don't care */, true);
}


/* Tree version of priority_*_block_task.  */

static inline void
priority_tree_block_task (enum priority_queue_type type, struct priority_queue *head, struct priority_node *node, int priority)
{
  struct gomp_task *first_task;
  struct priority_list *list = priority_queue_lookup_priority (head, priority);
  priority_list_block_task (type, list, node);
  /* HP-LIST */
  if (list->tasks == NULL || ((first_task = priority_node_to_task (type, list->tasks))->kind != GOMP_TASK_WAITING && first_task->kind != GOMP_TASK_TIED_SUSPENDED))
  {
    if (prio_splay_tree_hp_list_mark_node (&head->t, (prio_splay_tree_node) list, false))
    {
      if (head->t.highest_marked != NULL)
        __atomic_store_n(&head->highest_priority, head->t.highest_marked->key.l.priority, MEMMODEL_RELEASE);
      else
        __atomic_store_n(&head->highest_priority, 0, MEMMODEL_RELEASE);
    }
  }
}


/* Generic version of priority_*_block_task. This function allows to BLOCK
   a task which is currently waiting one or more of its children in all the
   queues it belongs to. A running UNTIED task is blocked and marked as
   WAITING after suspension, while running TIED tasks are blocked and marked
   as SUSPENDED since no other thread may chose it for execution.

   @INPUT (non of them can be NULL)
      type:         team's or children's queue
      head:         the head of the queue
      task:         the task to block  */

static inline void
priority_queue_block_task (enum priority_queue_type type,
      struct priority_queue *head, struct gomp_task *task)
{
#if _LIBGOMP_CHECKING_
  if (!priority_queue_task_in_queue_p (type, head, task))
    gomp_fatal ("Attempt to block non-existing task %p", task);
#endif
  if (priority_queue_multi_p (head) || __builtin_expect (task->priority > 0, 0))
    priority_tree_block_task (type, head, task_to_priority_node (type, task), task->priority);
  else
    priority_list_block_task (type, &head->l, task_to_priority_node (type, task));
  if (task->kind == GOMP_TASK_WAITING || task->kind == GOMP_TASK_TIED_SUSPENDED)
    __atomic_store_n(&head->num_waiting_priority_node, head->num_waiting_priority_node-1, MEMMODEL_RELEASE);
}


/* Given a task into blocked priority queue,
   move it onto a non-blocked priority queue.  */

static inline void
priority_list_unblock_task (enum priority_queue_type type, struct priority_list *list,
                            struct gomp_task *task, bool task_is_parent_depends_on)
{
  struct priority_node *node = task_to_priority_node (type, task);
  priority_list_remove (type, list, node, true, MEMMODEL_RELAXED);
  /* TODO:  as soon as we are using the same blocking conditions used for TIED
            tasks, the comparison below loses any sense. It can be removed.  */
  if (task->taskwait && task->taskwait->in_taskwait && GET_NUM_CHILDREN(task) > 0)
    priority_list_insert (type, list, node, PRIORITY_INSERT_END, task_is_parent_depends_on, false);
  else
  {
    if (type == PQ_CHILDREN_TIED || type == PQ_CHILDREN_UNTIED || type >= PQ_SUSPENDED_TIED)
      priority_list_insert (type, list, node, INS_UNBLK_CHILD_POLICY(gomp_queue_policy_var), task_is_parent_depends_on, false);
    else if (type == PQ_TASKGROUP_TIED || type == PQ_TASKGROUP_UNTIED)
      priority_list_insert (type, list, node, INS_UNBLK_GROUP_POLICY(gomp_queue_policy_var), task_is_parent_depends_on, false);
    else
      priority_list_insert (type, list, node, INS_UNBLK_TEAM_POLICY(gomp_queue_policy_var), task_is_parent_depends_on, false);
  }
}


/* Tree version of priority_*_unblock_task.  */

static inline void
priority_tree_unblock_task (enum priority_queue_type type, struct priority_queue *head,
      struct gomp_task *task, int priority, bool task_is_parent_depends_on)
{
  struct priority_list *list = priority_queue_lookup_priority (head, priority);
  priority_list_unblock_task (type, list, task, task_is_parent_depends_on);
  /* HP-LIST */
  if (task->kind == GOMP_TASK_WAITING || task->kind == GOMP_TASK_TIED_SUSPENDED)
    if (prio_splay_tree_hp_list_mark_node (&head->t, (prio_splay_tree_node) list, true))
      __atomic_store_n(&head->highest_priority, head->t.highest_marked->key.l.priority, MEMMODEL_RELEASE);
}


/* Generic version of priority_*_unblock_task. This function allows to UNBLOCK
   a task that was previously waiting one or more of its children. The task may
   be either WAITING UNTIED or SUSPENDED TIED to distinguish from running tasks
   maintained in the Children's or Taskgroup's queues.

   @INPUT (non of them can be NULL)
      type:         team's or children's queue
      head:         the head of the queue
      task:         the task to unblock  */

static inline void
priority_queue_unblock_task (enum priority_queue_type type,
        struct priority_queue *head, struct gomp_task *task)
{
#if _LIBGOMP_CHECKING_
  if (!priority_queue_task_in_queue_p (type, head, task))
    gomp_fatal ("Attempt to unblock non-existing task %p", task);
#endif
  if (priority_queue_multi_p (head) || __builtin_expect (task->priority > 0, 0))
    priority_tree_unblock_task (type, head, task, task->priority, task->parent_depends_on);
  else
    priority_list_unblock_task (type, &head->l, task, task->parent_depends_on);
  __atomic_store_n(&head->num_waiting_priority_node, head->num_waiting_priority_node+1, MEMMODEL_RELEASE);
}


/* This function is invoked each time a thread inserts or completes a child task
   which parent should be notified by unlocking its blocked state, or by moving
   it in a blocked task queue in case it cannot get out from the taskwait routine.

   @INPUT (non of them can be NULL)
      team:  the team to which the thread belongs
      task:  the task that could be blocked/unblocked

   @CONDITION
      team->task_lock: must be acquired  */

static void
gomp_task_handle_blocking (struct gomp_team *team, struct gomp_task *task)
{
  if (task->type == GOMP_TASK_TYPE_TIED)
  {
    if (task->kind != GOMP_TASK_TIED_SUSPENDED)
      return;
    if (task->is_blocked && IS_NOT_BLOCKED_TIED(task))
    {
      task->is_blocked = false;
      if (task->suspending_thread != NULL)
        priority_queue_unblock_task (PQ_SUSPENDED_TIED, &task->suspending_thread->tied_suspended, task);
    }
    else if (!task->is_blocked && IS_BLOCKED_TIED(task))
    {
      task->is_blocked = true;
      if (task->suspending_thread != NULL)
        priority_queue_block_task (PQ_SUSPENDED_TIED, &task->suspending_thread->tied_suspended, task);
    }
  }
  else
  {
    if (task->kind != GOMP_TASK_WAITING)
      return;
    if (task->is_blocked && IS_NOT_BLOCKED_UNTIED(task))
    {
      if (task->undeferred_ancestor == NULL)
      {
        if (task->parent)
          priority_queue_unblock_task (PQ_CHILDREN_UNTIED, &task->parent->untied_children_queue, task);
        if (task->taskgroup)
          priority_queue_unblock_task (PQ_TASKGROUP_UNTIED, &task->taskgroup->untied_taskgroup_queue, task);
      }
      priority_queue_unblock_task (PQ_TEAM_UNTIED, &team->untied_task_queue, task);
      task->is_blocked = false;
      if (task->suspending_thread != NULL)
        priority_queue_unblock_task (PQ_SUSPENDED_UNTIED, &task->suspending_thread->untied_suspended, task);

      ++team->task_queued_count;
      gomp_team_barrier_set_task_pending (&team->barrier);

      if (task->parent && task->parent->taskwait && task->parent->taskwait->in_taskwait)
        gomp_task_handle_blocking (team, task->parent);
    }
    else if (!task->is_blocked && IS_BLOCKED_UNTIED(task))
    {
      if (task->undeferred_ancestor == NULL)
      {
        if (task->parent)
          priority_queue_block_task (PQ_CHILDREN_UNTIED, &task->parent->untied_children_queue, task);
        if (task->taskgroup)
          priority_queue_block_task (PQ_TASKGROUP_UNTIED, &task->taskgroup->untied_taskgroup_queue, task);
      }
      priority_queue_block_task (PQ_TEAM_UNTIED, &team->untied_task_queue, task);
      task->is_blocked = true;
      if (task->suspending_thread != NULL)
        priority_queue_block_task (PQ_SUSPENDED_UNTIED, &task->suspending_thread->untied_suspended, task);

      if (--team->task_queued_count == 0)
        gomp_team_barrier_clear_task_pending (&team->barrier);

      if (task->parent && task->parent->taskwait && task->parent->taskwait->in_taskwait)
        gomp_task_handle_blocking (team, task->parent);
    }
  }
}


/* This function implements the first part of the procedure that allow to
   terminate and free task data structures once ita has completed to execute.

   @INPUT (non of them can be NULL)
      team:       the team to which the thread belongs
      task:       the task within which scope termination operations occur
      next_task:  the task subjects to the termination operations

   @CONDITION
      team->task_lock: must be acquired  */

static inline int
gomp_terminate_task_pre (struct gomp_team *team, struct gomp_task *task,
                          struct gomp_task *next_task)
{
	int do_wake;
	size_t new_tasks;

  do_wake = 0;
  new_tasks = gomp_task_run_post_handle_depend (next_task, team);

  if (task == next_task->parent)
  {
  	if (next_task->type == GOMP_TASK_TYPE_TIED)
  	{
	    priority_queue_remove (PQ_CHILDREN_TIED, &task->tied_children_queue,
                      next_task, false, MEMMODEL_RELAXED);
	    next_task->pnode[PQ_CHILDREN_TIED].next = NULL;
			next_task->pnode[PQ_CHILDREN_TIED].prev = NULL;
  	}
  	else
  	{
	    priority_queue_remove (PQ_CHILDREN_UNTIED, &task->untied_children_queue,
                      next_task, false, MEMMODEL_RELAXED);
	    next_task->pnode[PQ_CHILDREN_UNTIED].next = NULL;
	    next_task->pnode[PQ_CHILDREN_UNTIED].prev = NULL;
  	}
  }
  else
    gomp_task_run_post_remove_parent (next_task);

  gomp_clear_parent (PQ_CHILDREN_TIED, &next_task->tied_children_queue);
  gomp_clear_parent (PQ_CHILDREN_UNTIED, &next_task->untied_children_queue);

  gomp_task_run_post_remove_taskgroup (next_task);

  if (next_task->kind != GOMP_TASK_UNDEFERRED)
    team->task_count--;
  
  if (new_tasks > 1)
  {
    do_wake = team->nthreads - team->task_running_count - !task->in_tied_task;
    if (do_wake > new_tasks)
      do_wake = new_tasks;
  }

  return do_wake;
}


/* This function implements the second part of the procedure that allow to
   terminate and free task data structures once ita has completed to execute.

   @INPUT (non of them can be NULL)
      thr:        the thread performing these operations
      team:       the team to which the thread belongs
      next_task:  the task subjects to the termination operations
      do_wake:    the number of sleepeng threads that must be awaken

   @CONDITION
      team->task_lock: must be released  */

static inline void
gomp_terminate_task_post(struct gomp_thread *thr, struct gomp_team *team,
                          struct gomp_task *next_task, int do_wake)
{
  if (do_wake)
    gomp_team_barrier_wake (&team->barrier, do_wake);

  gomp_finish_task (next_task);

  if (next_task->icv.ult_var)
  {
    gomp_free_task_state (thr->global_task_state_group, thr->local_task_state_list, next_task->state);
  }

  free (next_task);
}


/* Given a gomp_task pointers array, this function selects the highest priority
   task.

   @INPUT (non of them can be NULL)
      next: the array containing gomp_task pointers
      t:    the number of gomp_task pointers in the array

   @CONDITION
      next: must be at least t gomp_task pointers size long  */

static inline struct gomp_task *
get_higher_priority_task (struct gomp_task **next, unsigned t)
{
	unsigned i;
	int priority = -1;
	bool task_is_parent_depends_on = false;
	struct gomp_task *next_task = NULL;

	for (i=0; i<t; i++)
  {
		if (next[i] != NULL && next[i]->fn != NULL)
    {
      if (next[i]->priority > priority || (next[i]->priority == priority
                && next[i]->parent_depends_on && !task_is_parent_depends_on))
			{
				next_task = next[i];
				priority = next_task->priority;
				task_is_parent_depends_on = next_task->parent_depends_on;
			}
    }
  }

  return next_task;
}


/* Given a gomp_task pointers array, this function selects the highest priority
   task among those ones are not in Taskwait.

   @INPUT (non of them can be NULL)
      next: the array containing gomp_task pointers
      t:    the number of gomp_task pointers in the array

   @CONDITION
      next: must be at least t gomp_task pointers size long  */

static inline struct gomp_task *
get_not_in_taskwait_higher_priority_task (struct gomp_task **next, unsigned t)
{
  unsigned i;
  int priority = -1;
  bool task_is_parent_depends_on = false;
  struct gomp_task *next_task = NULL;

  for (i=0; i<t; i++)
  {
    if (next[i] != NULL && next[i]->fn != NULL && (next[i]->taskwait == NULL || !next[i]->taskwait->in_taskwait))
    {
      if (next[i]->priority > priority || (next[i]->priority == priority
            && next[i]->parent_depends_on && !task_is_parent_depends_on))
      {
        next_task = next[i];
        priority = next_task->priority;
        task_is_parent_depends_on = next_task->parent_depends_on;
      }
    }
  }

  return next_task;
}


/* This function helps to improve locality by caching a per-thread "gomp_task_state"
   data structure. This structure will be linked to the next task to run for the first
   time along the execution of path of the current thread.

   @INPUT (non of them can be NULL)
      thr:              current thread

   @CONDITION
      team->task_lock:  must be released  */

static inline void
gomp_put_task_state_in_cache (struct gomp_thread *thr)
{
  if (thr->cached_state == NULL)
    thr->cached_state = gomp_get_task_state(thr->global_task_state_group, thr->local_task_state_list, gomp_icv (false)->ult_stack_size);
}


/* This function helps to improve locality by providing a previously cached "gomp_task_state"
   data structure. This structure has been locally allocated by the current thread, avoiding
   to read remote metadata.

   @INPUT (non of them can be NULL)
      thr:        current thread
      task:       task to which assign the cached state  */

static inline void
gomp_get_task_state_from_cache (struct gomp_thread *thr, struct gomp_task *task)
{
  if (task->state == NULL)
  {
    if (thr->cached_state == NULL)
      gomp_fatal ("Attempting to assign non-cached state data structures to task %p", (void *) task);
    task->state = thr->cached_state;
    thr->cached_state = NULL;
  }
}


/* This function allows to insert a TIED task into the list of all that tasks that are
   TIED to the current thread.

   @INPUT (none of them can be NULL)
      thr:          current thread
      task:         the task performing the insertion
      next_task:    the task to be inserted

   @CONDITION
      next_task->type:  must be TIED  */

static inline void
gomp_insert_task_in_tied_list (struct gomp_thread *thr, struct gomp_task *task, struct gomp_task *next_task)
{
  if (task->kind == GOMP_TASK_IMPLICIT)
  {
    thr->last_tied_task = next_task;
  }
  else
  {
    if (next_task->previous_tied_task == NULL && next_task->next_tied_task == NULL)
    {
      if (thr->last_tied_task != NULL && thr->last_tied_task != next_task)
      {
        thr->last_tied_task->next_tied_task = next_task;
        next_task->previous_tied_task = thr->last_tied_task;
      }

      thr->last_tied_task = next_task;
    }
  }
}


/* This function allows to remove a TIED task from the list of all that tasks that are
   TIED to the current thread.

   @INPUT (none of them can be NULL)
      thr:          current thread
      task:         the task performing the remove
      prev_task:    the task to be removed

   @CONDITION
      prev_task->type:  must be TIED  */

static inline void
gomp_remove_task_from_tied_list (struct gomp_thread *thr, struct gomp_task *task, struct gomp_task *prev_task)
{
  if (task->kind == GOMP_TASK_IMPLICIT)
  {
    thr->last_tied_task = NULL;
  }
  else
  {
    if (thr->last_tied_task == prev_task)
    {
      thr->last_tied_task = thr->last_tied_task->previous_tied_task;
      if (thr->last_tied_task)
        thr->last_tied_task->next_tied_task = NULL;
    }
    else
    {
      if (prev_task->next_tied_task != NULL)
        prev_task->next_tied_task->previous_tied_task = prev_task->previous_tied_task;
      if (prev_task->previous_tied_task != NULL)
        prev_task->previous_tied_task->next_tied_task = prev_task->next_tied_task;
    }
  }
}


/* This function allows to suspend a TIED task execution by giving control to
   another task for several purposes (priority inversion, work conservetivenes,
   etc.). Such a task-switch may arise due to IBS interrupt arrival as well as
   synchronous invokations caused by task's blockeing conditions. This would lead
   to an optimized manner to exploit available resources when, for example,
   priorities play a central role in the application execution.

   @INPUT
      thr:          current thread
      team:         the team to which the thread belongs
      task:         the TIED task to suspend locally
      next_task:    the next task that is going to be executed
      locked_tasks: if not NULL, points to the queue of locked tasks

   @CONDITION
      team->task_lock:      must be acuired  */

static inline void
gomp_suspend_tied_task_for_successor(struct gomp_thread *thr, struct gomp_team *team, struct gomp_task *task,
                                      struct gomp_task *next_task, struct priority_queue *locked_tasks)
{
  int do_wake;

  if (context_save(&task->state->context))
  {
    thr = gomp_thread ();
    team = thr->ts.team;

    next_task = task->state->switch_from_task;

    if (next_task)
    {
      task->state->switch_from_task = NULL;
      if (next_task->type == GOMP_TASK_TYPE_TIED)
        gomp_remove_task_from_tied_list (thr, task, next_task);
    }

    thr->task = task;

    if (next_task)
    {
      if (thr->hold_team_lock)
        thr->hold_team_lock = false;
      else
      {
#if _LIBGOMP_TEAM_LOCK_TIMING_
        thr->team_lock_time->entry_acquisition_time = RDTSC();
#endif
        gomp_mutex_lock (&team->task_lock);
#if _LIBGOMP_TEAM_LOCK_TIMING_
        uint64_t tmp_time = RDTSC();
        thr->team_lock_time->lock_acquisition_time += (tmp_time - thr->team_lock_time->entry_acquisition_time);
        thr->team_lock_time->entry_time = tmp_time;
#endif
      }
      if (task->kind == GOMP_TASK_TIED_SUSPENDED)
      {
        /* It is perfectly safe to execute within this task context before having removed
           it from the thread's suspended queue, even if the team's lock was not acquired,
           because this TIED task was suspended by this thread and only this thread may
           pretend to resume its context.  However, updating the suspended task's queue
           mandatory requires the team's lock be acquired because other threads can update
           the tasks blocking condition.  */
        if (locked_tasks != NULL)
        {
          priority_queue_remove (PQ_LOCKED, locked_tasks, task, false, MEMMODEL_RELEASE);
          task->pnode[PQ_LOCKED].next = NULL;
          task->pnode[PQ_LOCKED].prev = NULL;
        }

        priority_queue_remove (PQ_SUSPENDED_TIED, &task->suspending_thread->tied_suspended, task, task->is_blocked, MEMMODEL_RELAXED);
        task->pnode[PQ_SUSPENDED_TIED].next = NULL;
        task->pnode[PQ_SUSPENDED_TIED].prev = NULL;

        if (task->undeferred_ancestor)
          task->kind = GOMP_TASK_UNDEFERRED;
        else
          task->kind = GOMP_TASK_TIED;
      }
      do_wake = gomp_terminate_task_pre (team, task, next_task);
#if _LIBGOMP_TEAM_LOCK_TIMING_
      thr->team_lock_time->lock_time += (RDTSC() - thr->team_lock_time->entry_time);
#endif
      gomp_mutex_unlock (&team->task_lock);
      gomp_terminate_task_post (thr, team, next_task, do_wake);
    }
    else if (thr->hold_team_lock)
    {
      do_wake = team->task_running_count + !task->in_tied_task < team->nthreads;
#if _LIBGOMP_TEAM_LOCK_TIMING_
      thr->team_lock_time->lock_time += (RDTSC() - thr->team_lock_time->entry_time);
#endif
      gomp_mutex_unlock (&team->task_lock);
      thr->hold_team_lock = false;
      if (do_wake)
        gomp_team_barrier_wake (&team->barrier, 1);
    }

    return;
  }

  if (task->kind == GOMP_TASK_IMPLICIT)
  {
#if _LIBGOMP_TEAM_LOCK_TIMING_
    thr->team_lock_time->lock_time += (RDTSC() - thr->team_lock_time->entry_time);
#endif
    gomp_mutex_unlock (&team->task_lock);
    thr->hold_team_lock = false;

    next_task->state->switch_task = task;
    next_task->state->switch_from_task = NULL;
    task->state->switch_from_task = next_task;

    if (next_task->type == GOMP_TASK_TYPE_TIED)
      gomp_insert_task_in_tied_list (thr, task, next_task);
  }
  else
  {
    next_task->state->switch_task = task;
    next_task->state->switch_from_task = NULL;
    task->state->switch_from_task = next_task;

    if (next_task->type == GOMP_TASK_TYPE_TIED)
      gomp_insert_task_in_tied_list (thr, task, next_task);

    if (task->taskwait && task->taskwait->in_taskwait && IS_BLOCKED_TIED(task))
    {
      task->is_blocked = true;
      priority_queue_insert (PQ_SUSPENDED_TIED, &thr->tied_suspended, task, task->priority,
              /*DON'T CARE*/ PRIORITY_INSERT_BEGIN, task->parent_depends_on, task->is_blocked);
    }
    else if (locked_tasks != NULL)
    {
      priority_queue_insert (PQ_LOCKED, locked_tasks, task, task->priority,
                      PRIORITY_INSERT_END, task->parent_depends_on, false);
      task->is_blocked = true;
      priority_queue_insert (PQ_SUSPENDED_TIED, &thr->tied_suspended, task, task->priority,
              /*DON'T CARE*/ PRIORITY_INSERT_BEGIN, task->parent_depends_on, task->is_blocked);
    }
    else
    {
      task->is_blocked = false;
      priority_queue_insert (PQ_SUSPENDED_TIED, &thr->tied_suspended, task, task->priority,
        INS_SUSP_CHILD_POLICY(gomp_queue_policy_var), task->parent_depends_on, task->is_blocked);
    }

    task->suspending_thread = thr;
    task->kind = GOMP_TASK_TIED_SUSPENDED;

    thr->hold_team_lock = true;
  }

  thr->task = next_task;

  context_restore(&next_task->state->context);
}


/* This function allows to suspend an UNTIED task in favour of another.
   The task that is going to be executed may be TIED or UNTIED. In case the
   task is TIED, it's assumed to be "descendant" of any task already TIED
   to this thread.

   @INPUT
      thr:          current thread
      team:         the team to which the thread belongs
      task:         the UNTIED task to suspend
      next_task:    the next task (TIED or UNTIED) that is going to be executed
      locked_tasks: if not NULL, points to the queue of locked tasks

   @CONDITION
      team->task_lock:  must be already acquired
      next_task->type:  if TIED, it should be descendant of any task TIED to the thread  */

static inline void
gomp_suspend_untied_task_for_successor(struct gomp_thread *thr, struct gomp_team *team, struct gomp_task *task,
                                        struct gomp_task *next_task, struct priority_queue *locked_tasks)
{
  int do_wake;

  if (context_save(&task->state->context))
  {
    thr = gomp_thread ();
    team = thr->ts.team;

    thr->task = task;

    if (thr->hold_team_lock)
    {
      do_wake = team->task_running_count + !task->in_tied_task < team->nthreads;
#if _LIBGOMP_TEAM_LOCK_TIMING_
      thr->team_lock_time->lock_time += (RDTSC() - thr->team_lock_time->entry_time);
#endif
      gomp_mutex_unlock (&team->task_lock);
      thr->hold_team_lock = false;
      if (do_wake)
        gomp_team_barrier_wake (&team->barrier, 1);
    }

    return;
  }

  next_task->state->switch_task = task->state->switch_task;
  next_task->state->switch_from_task = NULL;
  task->state->switch_task->state->switch_from_task = next_task;

  if (next_task->type == GOMP_TASK_TYPE_TIED)
    gomp_insert_task_in_tied_list (thr, task, next_task);

  task->state->switch_task = NULL;
  task->state->switch_from_task = NULL;

  if (task->taskwait && task->taskwait->in_taskwait && IS_BLOCKED_UNTIED(task))
  {
    if (task->kind != GOMP_TASK_UNDEFERRED)
    {
      if (task->parent)
        priority_queue_block_task (PQ_CHILDREN_UNTIED, &task->parent->untied_children_queue, task);
      if (task->taskgroup)
        priority_queue_block_task (PQ_TASKGROUP_UNTIED, &task->taskgroup->untied_taskgroup_queue, task);
    }
    task->is_blocked = true;
    priority_queue_insert (PQ_SUSPENDED_UNTIED, &thr->untied_suspended, task, task->priority,
            /*DON'T CARE*/ PRIORITY_INSERT_BEGIN, task->parent_depends_on, task->is_blocked);

    task->suspending_thread = thr;
    task->kind = GOMP_TASK_WAITING;

    priority_queue_insert (PQ_TEAM_UNTIED, &team->untied_task_queue, task, task->priority,
      INS_SUSP_TEAM_POLICY(gomp_queue_policy_var), task->parent_depends_on, task->is_blocked);
  }
  else if (locked_tasks != NULL)
  {
    priority_queue_insert (PQ_LOCKED, locked_tasks, task, task->priority,
                      PRIORITY_INSERT_END, task->parent_depends_on, false);
    if (task->kind != GOMP_TASK_UNDEFERRED)
    {
      if (task->parent)
        priority_queue_block_task (PQ_CHILDREN_UNTIED, &task->parent->untied_children_queue, task);
      if (task->taskgroup)
        priority_queue_block_task (PQ_TASKGROUP_UNTIED, &task->taskgroup->untied_taskgroup_queue, task);
    }
    task->is_blocked = true;
    priority_queue_insert (PQ_SUSPENDED_UNTIED, &thr->untied_suspended, task, task->priority,
            /*DON'T CARE*/ PRIORITY_INSERT_BEGIN, task->parent_depends_on, task->is_blocked);

    task->suspending_thread = thr;
    task->kind = GOMP_TASK_WAITING;

    priority_queue_insert (PQ_TEAM_UNTIED, &team->untied_task_queue, task, task->priority,
      INS_SUSP_TEAM_POLICY(gomp_queue_policy_var), task->parent_depends_on, task->is_blocked);
  }
  else
  {
    if (task->kind != GOMP_TASK_UNDEFERRED)
    {
      if (task->parent)
        priority_queue_adjust_task (PQ_CHILDREN_UNTIED, &task->parent->untied_children_queue, task, task->parent_depends_on);
      if (task->taskgroup)
        priority_queue_adjust_task (PQ_TASKGROUP_UNTIED, &task->taskgroup->untied_taskgroup_queue, task, task->parent_depends_on);
    }
    task->is_blocked = false;
    priority_queue_insert (PQ_SUSPENDED_UNTIED, &thr->untied_suspended, task, task->priority,
      INS_SUSP_CHILD_POLICY(gomp_queue_policy_var), task->parent_depends_on, task->is_blocked);

    task->suspending_thread = thr;
    task->kind = GOMP_TASK_WAITING;

    priority_queue_insert (PQ_TEAM_UNTIED, &team->untied_task_queue, task, task->priority,
      INS_SUSP_TEAM_POLICY(gomp_queue_policy_var), task->parent_depends_on, task->is_blocked);

    ++team->task_queued_count;
    gomp_team_barrier_set_task_pending (&team->barrier);

    if (task->parent && task->parent->taskwait && task->parent->taskwait->in_taskwait)
      gomp_task_handle_blocking (team, task->parent);
  }

  thr->hold_team_lock = true;
  thr->task = next_task;

  context_restore(&next_task->state->context);
}


/* This function allows to create, initialize and immediately execute a task. This
   is particularly usefull when an IMPLICIT parent task intends to execute an
   UNDEFERRED child task which cannot obviously share its state with parent. So,
   we admit the association of a different state (context+stack) with this new task.
   Parent task will not be resumed as long as this new task has not completed its
   execution.

   @INPUT
      fn, data,cpyfn, arg_size,
      arg_align, flags, depend,
      priority:                     their values follow the one passed to GOMP_task  */

static inline void
gomp_suspend_implicit_for_undeferred(void (*fn) (void *), void *data, void (*cpyfn) (void *, void *),
                    long arg_size, long arg_align, unsigned flags, void **depend, int priority)
{
  struct gomp_thread *thr = gomp_thread ();
  struct gomp_task *implicit = thr->task;
  struct gomp_team *team = thr->ts.team;
  struct gomp_taskgroup *taskgroup;
  struct gomp_task *undeferred;
  size_t depend_size;
  char *arg;

  if (flags & GOMP_TASK_FLAG_DEPEND)
    depend_size = ((uintptr_t) depend[0] * sizeof (struct gomp_task_depend_entry));
  else
    depend_size = 0;
  
  undeferred = gomp_malloc (sizeof (*undeferred) + depend_size + arg_size + arg_align - 1);

  arg = (char *) (((uintptr_t) (undeferred + 1) + depend_size + arg_align - 1) & ~(uintptr_t) (arg_align - 1));

  gomp_init_task (undeferred, implicit, gomp_icv (false));

  gomp_put_task_state_in_cache (thr);

#if _LIBGOMP_TASK_TIMING_
  undeferred->creation_time = RDTSC();
#endif

  undeferred->priority = (priority > implicit->priority) ? priority : implicit->priority;
  undeferred->type = GOMP_TASK_TYPE_TIED;
  undeferred->kind = GOMP_TASK_UNDEFERRED;
  undeferred->in_tied_task = implicit->in_tied_task;
  undeferred->taskgroup = taskgroup = implicit->taskgroup;
  thr->task = undeferred;
  if (cpyfn)
  {
    if (!thr->non_preemptable)
    {
      __atomic_store_n (&thr->non_preemptable, thr->non_preemptable+1, MEMMODEL_SEQ_CST);
      cpyfn (arg, data);
      __atomic_store_n (&thr->non_preemptable, thr->non_preemptable-1, MEMMODEL_SEQ_CST);
    }
    else
      cpyfn (arg, data);
    undeferred->copy_ctors_done = true;
  }
  else
    memcpy (arg, data, arg_size);
  thr->task = implicit;
  undeferred->fn = fn;
  undeferred->fn_data = arg;
  undeferred->final_task = (flags & GOMP_TASK_FLAG_FINAL) >> 1;
  undeferred->undeferred_ancestor = undeferred;
  undeferred->state = NULL;

#if _LIBGOMP_TEAM_LOCK_TIMING_
  thr->team_lock_time->entry_acquisition_time = RDTSC();
#endif

  gomp_mutex_lock (&team->task_lock);

#if _LIBGOMP_TEAM_LOCK_TIMING_
  uint64_t tmp_time = RDTSC();
  thr->team_lock_time->lock_acquisition_time += (tmp_time - thr->team_lock_time->entry_acquisition_time);
  thr->team_lock_time->entry_time = tmp_time;
#endif

  if (__builtin_expect ((gomp_team_barrier_cancelled (&team->barrier) ||
        (taskgroup && taskgroup->cancelled)) && !undeferred->copy_ctors_done, 0))
  {
#if _LIBGOMP_TEAM_LOCK_TIMING_
    thr->team_lock_time->lock_time += (RDTSC() - thr->team_lock_time->entry_time);
#endif
    gomp_mutex_unlock (&team->task_lock);
    gomp_finish_task (undeferred);
    free (undeferred);
    return;
  }

  if (taskgroup)
    taskgroup->num_children++;

  if (depend_size)
  {
    gomp_task_handle_depend (undeferred, implicit, depend);
    if (undeferred->num_dependees)
    {
#if _LIBGOMP_TEAM_LOCK_TIMING_
      thr->team_lock_time->lock_time += (RDTSC() - thr->team_lock_time->entry_time);
#endif
      gomp_mutex_unlock (&team->task_lock);
      return;
    }
  }

  priority_queue_insert_running (PQ_CHILDREN_TIED, &implicit->tied_children_queue, undeferred, undeferred->priority);
  if (taskgroup)
    priority_queue_insert_running (PQ_TASKGROUP_TIED, &taskgroup->tied_taskgroup_queue, undeferred, undeferred->priority);

  gomp_get_task_state_from_cache (thr, undeferred);

#if (_LIBGOMP_TASK_SWITCH_AUDITING_ && _LIBGOMP_IN_FLOW_TASK_SWITCH_AUDITING_)
  gomp_save_inflow_task_switch (thr->task_switch_audit, implicit, undeferred);
#endif
  gomp_suspend_tied_task_for_successor(thr, team, implicit, undeferred, NULL);
}

#endif /* _TASK_H_ */