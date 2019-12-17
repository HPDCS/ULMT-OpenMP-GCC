/* Copyright (C) 2015-2017 Free Software Foundation, Inc.
   Contributed by Aldy Hernandez <aldyh@redhat.com>.

   This file is part of the GNU Offloading and Multi Processing Library
   (libgomp).

   Libgomp is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   Libgomp is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */

/* Header file for a priority queue of GOMP tasks.  */

/* ?? Perhaps all the priority_tree_* functions are complex and rare
   enough to go out-of-line and be moved to priority_queue.c.  ??  */

#ifndef _PRIORITY_QUEUE_H_
#define _PRIORITY_QUEUE_H_

/* One task.  */

struct priority_node
{
  /* Next and previous chains in a circular doubly linked list for
     tasks within this task's priority.  */
  struct priority_node *next, *prev;
};

/* All tasks within the same priority.  */

struct priority_list
{
  /* Priority of the tasks in this set.  */
  int priority;

  /* Tasks.  */
  struct priority_node *tasks;

  /* This points to the last of the higher priority WAITING tasks.
     Remember that for the CHILDREN and TASKGROUP queues, we have:

  parent_depends_on WAITING tasks.
  WAITING tasks.
  TIED tasks.

     This is a pointer to the last of the parent_depends_on WAITING
     tasks which are essentially, higher priority items within their
     priority.  */
  struct priority_node *last_parent_depends_on;

  /* This points to the first TIED (RUNNING) task.  */
  struct priority_node *first_running_task;

  /* Blocked Tasks. UNTIED tasks executed and later suspended due
     to unability to proceedes because of dependencies not yet
     satisfied.  */
  struct priority_node *blocked_tasks;
};

/* Another splay tree instantiation, for priority_list's.  */
typedef struct prio_splay_tree_node_s *prio_splay_tree_node;
typedef struct prio_splay_tree_s *prio_splay_tree;
typedef struct prio_splay_tree_key_s *prio_splay_tree_key;
struct prio_splay_tree_key_s {
  /* This structure must only containing a priority_list, as we cast
     prio_splay_tree_key to priority_list throughout.  */
  struct priority_list l;
};
#define splay_tree_prefix prio
#include "splay-tree.h"

/* The entry point into a priority queue of tasks.

   There are two alternate implementations with which to store tasks:
   as a balanced tree of sorts, or as a simple list of tasks.  If
   there are only priority-0 items (ROOT is NULL), we use the simple
   list, otherwise (ROOT is non-NULL) we use the tree.  */

struct priority_queue
{
  /* The whole number of nodes attached to any priority_list.  */
  size_t num_priority_node;
  /* The whole number of nodes attached to any priority_list that refer
     to the only WAITING tasks.  */
  size_t num_waiting_priority_node;
  /* HP-LIST */
  /* The highest priority value among all the priority_list that maintain
     at least one WAITING priority node.  */
  int highest_priority;
  /* If t.root != NULL, this is a splay tree of priority_lists to hold
     all tasks.  This is only used if multiple priorities are in play,
     otherwise we use the priority_list `l' below to hold all
     (priority-0) tasks.  */
  struct prio_splay_tree_s t;
  /* If T above is NULL, only priority-0 items exist, so keep them
     in a simple list.  */
  struct priority_list l;
};

enum priority_insert_type {
  /* Insert at the beginning of a priority list.  */
  PRIORITY_INSERT_BEGIN,
  /* Insert at the end of a priority list.  */
  PRIORITY_INSERT_END
};

/* Used to determine in which queue a given priority node belongs in.
   See pnode field of gomp_task.  */

enum priority_queue_type
{
  PQ_TEAM_TIED,         /* Node belongs in gomp_team's TIED task_queue.  */
  PQ_TEAM_UNTIED,       /* Node belongs in gomp_team's UNTIED task_queue.  */
  PQ_CHILDREN_TIED,     /* Node belongs in parent's TIED children_queue.  */
  PQ_CHILDREN_UNTIED,   /* Node belongs in parent's UNTIED children_queue.  */
  PQ_TASKGROUP_TIED,    /* Node belongs in TIED taskgroup->tied_taskgroup_queue.  */
  PQ_TASKGROUP_UNTIED,  /* Node belongs in UNTIED taskgroup->untied_taskgroup_queue.  */
  PQ_SUSPENDED_TIED,    /* Node belongs in thread's TIED suspended_queue.  */
  PQ_SUSPENDED_UNTIED,  /* Node belongs in thread's UNTIED suspended_queue.  */
  PQ_LOCKED,            /* Node belongs in locked_tasks queue.  */
  PQ_IGNORED = 999
};

/* Priority queue implementation prototypes.  */

extern bool priority_queue_task_in_queue_p (enum priority_queue_type,
					    struct priority_queue *, struct gomp_task *);
extern void priority_queue_dump (enum priority_queue_type,
              struct priority_queue *);
extern void priority_queue_verify (enum priority_queue_type,
              struct priority_queue *, bool);
extern void priority_tree_insert (enum priority_queue_type,
              struct priority_queue *, struct priority_node *,
              int priority, enum priority_insert_type, bool, bool);
extern void priority_tree_insert_running (enum priority_queue_type,
              struct priority_queue *, struct priority_node *, int);
extern void priority_tree_remove (enum priority_queue_type,
              struct priority_queue *, struct priority_node *, bool);

/* Return TRUE if there is more than one priority in HEAD.  This is
   used throughout to to choose between the fast path (priority 0 only
   items) and a world with multiple priorities.  */

static inline bool
priority_queue_multi_p (struct priority_queue *head)
{
  return __builtin_expect (head->t.root != NULL, 0);
}

/* Initialize a priority queue.  */

static inline void
priority_queue_init (struct priority_queue *head)
{
  head->num_priority_node = 0;
  head->num_waiting_priority_node = 0;
  /* HP-LIST */
  head->highest_priority = 0;
  head->t.root = NULL;
  /* HP-LIST */
  head->t.num_marked = 0;
  head->t.highest_marked = NULL;
  /* To save a few microseconds, we don't initialize head->l.priority
     to 0 here.  It is implied that priority will be 0 if head->t.root
     == NULL.

     priority_tree_insert() will fix this when we encounter multiple
     priorities.  */
  head->l.tasks = NULL;
  head->l.last_parent_depends_on = NULL;
  head->l.first_running_task = NULL;
  head->l.blocked_tasks = NULL;
}

static inline void
priority_queue_free (struct priority_queue *head)
{
  /* There's nothing to do, as tasks were freed as they were removed
     in priority_queue_remove.  */
}

/* Forward declarations.  */
static inline size_t priority_queue_offset (enum priority_queue_type);
static inline struct gomp_task *priority_node_to_task
				(enum priority_queue_type,
				 struct priority_node *);
static inline struct priority_node *task_to_priority_node
				(enum priority_queue_type,
				 struct gomp_task *);

/* Return TRUE if priority queue HEAD is empty.

   MODEL IS MEMMODEL_ACQUIRE if we should use an acquire atomic to
   read from the root of the queue, otherwise MEMMODEL_RELAXED if we
   should use a plain load.  */

static inline _Bool
priority_queue_empty_p (struct priority_queue *head, enum memmodel model)
{
  /* Note: The acquire barriers on the loads here synchronize with
     the write of a NULL in gomp_task_run_post_remove_parent.  It is
     not necessary that we synchronize with other non-NULL writes at
     this point, but we must ensure that all writes to memory by a
     child thread task work function are seen before we exit from
     GOMP_taskwait.  */
  if (priority_queue_multi_p (head))
  {
    if (model == MEMMODEL_ACQUIRE)
      return __atomic_load_n (&head->t.root, MEMMODEL_ACQUIRE) == NULL;
    return head->t.root == NULL;
  }
  if (model == MEMMODEL_ACQUIRE)
    return (__atomic_load_n (&head->l.tasks, MEMMODEL_ACQUIRE) == NULL &&
            __atomic_load_n (&head->l.blocked_tasks, MEMMODEL_ACQUIRE) == NULL);
  return (head->l.tasks == NULL && head->l.blocked_tasks == NULL);
}

/* Look for a given PRIORITY in HEAD.  Return it if found, otherwise
   return NULL.  This only applies to the tree variant in HEAD.  There
   is no point in searching for priorities in HEAD->L.  */

static inline struct priority_list *
priority_queue_lookup_priority (struct priority_queue *head, int priority)
{
  if (head->t.root == NULL)
    return NULL;
  struct prio_splay_tree_key_s k;
  k.l.priority = priority;
  return (struct priority_list *)
    prio_splay_tree_lookup (&head->t, &k);
}

/* Insert task in DATA, with PRIORITY, in the priority list in LIST.
   LIST contains items of type TYPE.

   If POS is PRIORITY_INSERT_BEGIN, the new task is inserted at the
   top of its respective priority.  If POS is PRIORITY_INSERT_END, the
   task is inserted at the end of its priority.

   If ADJUST_PARENT_DEPENDS_ON is TRUE, LIST is a children queue, and
   we must keep track of higher and lower priority WAITING tasks by
   keeping the queue's last_parent_depends_on field accurate.  This
   only applies to the children queue, and the caller must ensure LIST
   is a children queue in this case.

   If ADJUST_PARENT_DEPENDS_ON is TRUE, TASK_IS_PARENT_DEPENDS_ON is
   set to the task's parent_depends_on field.  If
   ADJUST_PARENT_DEPENDS_ON is FALSE, this field is irrelevant.

   Return the new priority_node.  */

static inline void
priority_list_insert (enum priority_queue_type type, struct priority_list *list, struct priority_node *node,
                        enum priority_insert_type pos, bool task_is_parent_depends_on, bool task_is_blocked)
{
  if (type <= PQ_TEAM_UNTIED || type >= PQ_SUSPENDED_TIED)
  {
    if (task_is_blocked)
    {
      if (list->blocked_tasks == NULL)
      {
        node->next = node;
        node->prev = node;
        list->blocked_tasks = node;
        return;
      }
      else
      {
        node->next = list->blocked_tasks;
        node->prev = list->blocked_tasks->prev;
        node->next->prev = node;
        node->prev->next = node;
        return;
      }
    }
    else if (list->tasks == NULL)
    {
      node->next = node;
      node->prev = node;
      list->tasks = node;
      return;
    }
    else
    {
      node->next = list->tasks;
      node->prev = list->tasks->prev;
      if (pos == PRIORITY_INSERT_BEGIN)
        list->tasks = node;
      node->next->prev = node;
      node->prev->next = node;
      return;
    }
  }
  else
  {
    if (task_is_blocked)
    {
      if (list->blocked_tasks == NULL)
      {
        node->next = node;
        node->prev = node;
        list->blocked_tasks = node;
        return;
      }
      else
      {
        node->next = list->blocked_tasks;
        node->prev = list->blocked_tasks->prev;
        node->next->prev = node;
        node->prev->next = node;
        return;
      }
    }
    else if (list->tasks == NULL)
    {
      node->next = node;
      node->prev = node;
      list->tasks = node;
      if (task_is_parent_depends_on)
        list->last_parent_depends_on = node;
      return;
    }
    else if (task_is_parent_depends_on)
    {
      if (pos == PRIORITY_INSERT_BEGIN || list->last_parent_depends_on == NULL)
      {
        node->next = list->tasks;
        node->prev = list->tasks->prev;
        list->tasks = node;
        if (list->last_parent_depends_on == NULL)
          list->last_parent_depends_on = node;
        node->next->prev = node;
        node->prev->next = node;
        return;
      }
      else
      {
        node->next = list->last_parent_depends_on->next;
        node->prev = list->last_parent_depends_on;
        list->last_parent_depends_on = node;
        node->next->prev = node;
        node->prev->next = node;
        return;
      }
    }
    else
    {
      if ((pos == PRIORITY_INSERT_BEGIN && list->last_parent_depends_on == NULL) ||
            (pos == PRIORITY_INSERT_END && list->first_running_task == NULL))
      {
        node->next = list->tasks;
        node->prev = list->tasks->prev;
        if (pos == PRIORITY_INSERT_BEGIN)
          list->tasks = node;
        node->next->prev = node;
        node->prev->next = node;
        return;
      }
      else if (pos == PRIORITY_INSERT_BEGIN)
      {
        node->next = list->last_parent_depends_on->next;
        node->prev = list->last_parent_depends_on;
        node->next->prev = node;
        node->prev->next = node;
        return;
      }
      else
      {
        node->next = list->first_running_task;
        node->prev = list->first_running_task->prev;
        if (list->tasks == list->first_running_task)
          list->tasks = node;
        node->next->prev = node;
        node->prev->next = node;
        return;
      }
    }
  }
}

/* Generic version of priority_*_insert.  */

static inline void
priority_queue_insert (enum priority_queue_type type, struct priority_queue *head, struct gomp_task *task,
        int priority, enum priority_insert_type pos, bool task_is_parent_depends_on, bool task_is_blocked)
{
#if _LIBGOMP_CHECKING_
  if (priority_queue_task_in_queue_p (type, head, task))
    gomp_fatal ("Attempt to insert existing task %p", task);
#endif
  if (priority_queue_multi_p (head) || __builtin_expect (priority > 0, 0))
    priority_tree_insert (type, head, task_to_priority_node (type, task), priority, pos, task_is_parent_depends_on, task_is_blocked);
  else
    priority_list_insert (type, &head->l, task_to_priority_node (type, task), pos, task_is_parent_depends_on, task_is_blocked);
  __atomic_store_n(&head->num_priority_node, head->num_priority_node+1, MEMMODEL_RELEASE);
  if (!task_is_blocked)
    __atomic_store_n(&head->num_waiting_priority_node, head->num_waiting_priority_node+1, MEMMODEL_RELEASE);
}

/* Insert a task that is going to be executed in DATA, with PRIORITY,
   in the priority list in LIST. LIST contains items of type TYPE.
   Task will be marked as TIED.  */

static inline void
priority_list_insert_running (struct priority_list *list, struct priority_node *node)
{
  if (list->tasks == NULL)
  {
    node->next = node;
    node->prev = node;
    list->tasks = node;
    list->first_running_task = node;
    return;
  }
  else
  {
    node->next = list->tasks;
    node->prev = list->tasks->prev;
    if (list->first_running_task == NULL)
      list->first_running_task = node;
    node->next->prev = node;
    node->prev->next = node;
    return;
  }
}

/* Generic version of priority_*_insert_running.  */

static inline void
priority_queue_insert_running (enum priority_queue_type type, struct priority_queue *head, struct gomp_task *task, int priority)
{
#if _LIBGOMP_CHECKING_
  if (priority_queue_task_in_queue_p (type, head, task))
    gomp_fatal ("Attempt to insert existing task %p", task);
#endif
  if (priority_queue_multi_p (head) || __builtin_expect (priority > 0, 0))
    priority_tree_insert_running (type, head, task_to_priority_node (type, task), priority);
  else
    priority_list_insert_running (&head->l, task_to_priority_node (type, task));
  __atomic_store_n(&head->num_priority_node, head->num_priority_node+1, MEMMODEL_RELEASE);
}

/* Remove NODE from LIST.

   If we are removing the one and only item in the list, and MODEL is
   MEMMODEL_RELEASE, use an atomic release to clear the list.

   If the list becomes empty after the remove, return TRUE.  */

static inline bool
priority_list_remove (enum priority_queue_type type, struct priority_list *list,
          struct priority_node *node, bool task_is_blocked, enum memmodel model)
{
  bool empty = false;
  if (task_is_blocked)
  {
    if (list->blocked_tasks == node && node->next == node)
    {
      if (model == MEMMODEL_RELEASE)
        __atomic_store_n (&list->blocked_tasks, NULL, MEMMODEL_RELEASE);
      else
        list->blocked_tasks = NULL;
      empty = (list->tasks == NULL);
      goto exit_list_remove;
    }
    else
    {
      if (list->blocked_tasks == node)
        list->blocked_tasks = node->next;
      node->next->prev = node->prev;
      node->prev->next = node->next;
      goto exit_list_remove;
    }
  }
  else if (type <= PQ_TEAM_UNTIED || type >= PQ_SUSPENDED_TIED)
  {
    if (list->tasks == node && node->next == node)
    {
      if (model == MEMMODEL_RELEASE)
        __atomic_store_n (&list->tasks, NULL, MEMMODEL_RELEASE);
      else
        list->tasks = NULL;
      empty = (list->blocked_tasks == NULL);
      goto exit_list_remove;
    }
    else
    {
      if (list->tasks == node)
        list->tasks = node->next;
      node->next->prev = node->prev;
      node->prev->next = node->next;
      goto exit_list_remove;
    }
  }
  else
  {
    if (list->tasks == node && node->next == node)
    {
      if (model == MEMMODEL_RELEASE)
        __atomic_store_n (&list->tasks, NULL, MEMMODEL_RELEASE);
      else
        list->tasks = NULL;
      if (list->first_running_task)
        list->first_running_task = NULL;
      else if (list->last_parent_depends_on)
        list->last_parent_depends_on = NULL;
      empty = (list->blocked_tasks == NULL);
      goto exit_list_remove;
    }
    else
    {
      if (list->tasks == node)
      {
        list->tasks = node->next;
        if (list->first_running_task == node)
          list->first_running_task = node->next;
        else if (list->last_parent_depends_on == node)
          list->last_parent_depends_on = NULL;
        node->next->prev = node->prev;
        node->prev->next = node->next;
        goto exit_list_remove;
      }
      else
      {
        if (list->first_running_task == node)
          list->first_running_task = (node->next != list->tasks) ? node->next : NULL;
        else if (list->last_parent_depends_on == node)
          list->last_parent_depends_on = node->prev;
        node->next->prev = node->prev;
        node->prev->next = node->next;
        goto exit_list_remove;
      }
    }
  }
exit_list_remove:
#if _LIBGOMP_CHECKING_
  memset (node, 0xaf, sizeof (*node));
#endif
  return empty;
}

/* This is the generic version of priority_list_remove.

   Remove NODE from priority queue HEAD.  HEAD contains tasks of type TYPE.

   If we are removing the one and only item in the priority queue and
   MODEL is MEMMODEL_RELEASE, use an atomic release to clear the queue.

   If the queue becomes empty after the remove, return TRUE.  */

static inline bool
priority_queue_remove (enum priority_queue_type type, struct priority_queue *head,
                struct gomp_task *task, bool task_is_blocked, enum memmodel model)
{
#if _LIBGOMP_CHECKING_
  if (!priority_queue_task_in_queue_p (type, head, task))
    gomp_fatal ("Attempt to remove missing task %p", task);
#endif
  if (priority_queue_multi_p (head))
  {
    priority_tree_remove (type, head, task_to_priority_node (type, task), task_is_blocked);
    __atomic_store_n(&head->num_priority_node, head->num_priority_node-1, MEMMODEL_RELEASE);
    if (!task_is_blocked && (type <= PQ_TEAM_UNTIED || type >= PQ_SUSPENDED_TIED))
      __atomic_store_n(&head->num_waiting_priority_node, head->num_waiting_priority_node-1, MEMMODEL_RELEASE);
    if (head->t.root == NULL)
    {
      if (model == MEMMODEL_RELEASE)
        /* Errr, we store NULL twice, the alternative would be to
           use an atomic release directly in the splay tree
           routines.  Worth it?  */
        __atomic_store_n (&head->t.root, NULL, MEMMODEL_RELEASE);
      return true;
    }
    return false;
  }
  else
  {
    bool empty = priority_list_remove (type, &head->l, task_to_priority_node (type, task), task_is_blocked, model);
    __atomic_store_n(&head->num_priority_node, head->num_priority_node-1, MEMMODEL_RELEASE);
    if (!task_is_blocked && (type <= PQ_TEAM_UNTIED || type >= PQ_SUSPENDED_TIED))
      __atomic_store_n(&head->num_waiting_priority_node, head->num_waiting_priority_node-1, MEMMODEL_RELEASE);
    return empty;
  }
}

#endif /* _PRIORITY_QUEUE_H_ */
