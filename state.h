#ifndef GOMP_FREE_STATE_H
#define GOMP_FREE_STATE_H 1

/* Default per-task stack size. */
#define ULT_STACK_SIZE  524288
/* Default per-thread initial number of free states. */
#define NUM_INIT_STATE	1024
/* Default per-thread additional number of free states @run-time. */
#define NUM_ADD_STATE	64


/* This structure maintains the execution context associated to a task.  */

struct gomp_task_state
{
  /* Cache-line distantiation.  */
  char __pad__[64];
  /* Points to the next "gomp_task_state" structure to be freed upon completion.  */
  struct gomp_task_state *next_to_free;
  /* Points to the next "gomp_task_state" structure in the free-list.  */
  struct gomp_task_state *next;
  /* CPU registers.  */
  struct gomp_task_context context;
  /* Task to which switch back upon completion of the current one.  */
  struct gomp_task *switch_task;
  /* To keep track of which task we have been resumed from.  */
  struct gomp_task *switch_from_task;
  /* Size in bytes of the allocated stack.  */
  size_t stack_size;
  /* Alternate stack where the associated task operates on.  */
  void *stack;
};

/* This structure handles a "gomp_task_state" list.  */

struct gomp_task_state_list
{
  /* Cache-line distantiation.  */
  char __pad__[64];
  /* Number of "gomp_task_state" structures enqueued.  */
  volatile unsigned list_size;
  /* Points to the first "gomp_task_state" structure.  */
  volatile struct gomp_task_state *list;
  /* Points to the first "gomp_task_state" structure to be freed.  */
  struct gomp_task_state *list_to_free;
};

/* This structure handles a global and a predefined number
   of per-thread "gomp_task_state_list" structures.  */

struct gomp_task_state_list_group
{
  /* Indicates whether this group has been already initialized.  */
  volatile bool initialized;
  /* The whole number of allocated tasks.  */
  volatile unsigned allocated_tasks;
  /* Needed to exclusively operates on the global list.  */
  gomp_spinlock_t global_list_lock;
  /* Number of available "thread_list" structure.  */
  unsigned thread_list_number;
  /* Array of per-thread lists.  */
  struct gomp_task_state_list **thread_list;
  /* Global list.  */
  struct gomp_task_state_list  global_list;
};


/* state.c */

extern int gomp_task_state_list_init(struct gomp_task_state_list_group *, unsigned, struct gomp_task_icv *);

extern int gomp_task_state_list_group_init(struct gomp_task_state_list_group *, unsigned, struct gomp_task_icv *);
extern void gomp_task_state_list_group_end(struct gomp_task_state_list_group *);

extern struct gomp_task_state *gomp_get_task_state(struct gomp_task_state_list_group *, struct gomp_task_state_list *, unsigned long);
extern void gomp_free_task_state(struct gomp_task_state_list_group *, struct gomp_task_state_list *, struct gomp_task_state *);

#endif /* GOMP_FREE_STATE_H */