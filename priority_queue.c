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

/* Priority queue implementation of GOMP tasks.  */

#include "libgomp.h"

#if _LIBGOMP_CHECKING_
#include <stdio.h>

/* Sanity check to verify whether a TASK is in LIST.  Return TRUE if
   found, FALSE otherwise.

   TYPE is the type of priority queue this task resides in.  */

static inline bool
priority_queue_task_in_list_p (enum priority_queue_type type,
			       struct priority_list *list,
			       struct gomp_task *task)
{
  struct priority_node *p;
  if (task->is_blocked)
  {
    if ((p = list->blocked_tasks) != NULL)
    {
      do
      {
        if (priority_node_to_task (type, p) == task)
          return true;
        p = p->next;
      }
      while (p != list->blocked_tasks);
    }
  }
  else
  {
    if ((p = list->tasks) != NULL)
    {
      do
      {
        if (priority_node_to_task (type, p) == task)
          return true;
        p = p->next;
      }
      while (p != list->tasks);
    }
  }
  return false;
}

/* Tree version of priority_queue_task_in_list_p.  */

static inline bool
priority_queue_task_in_tree_p (enum priority_queue_type type,
			       struct priority_queue *head,
			       struct gomp_task *task)
{
  struct priority_list *list
    = priority_queue_lookup_priority (head, task->priority);
  if (!list)
    return false;
  return priority_queue_task_in_list_p (type, list, task);
}

/* Generic version of priority_queue_task_in_list_p that works for
   trees or lists.  */

bool
priority_queue_task_in_queue_p (enum priority_queue_type type,
				struct priority_queue *head,
				struct gomp_task *task)
{
  if (priority_queue_empty_p (head, MEMMODEL_RELAXED))
    return false;
  if (priority_queue_multi_p (head))
    return priority_queue_task_in_tree_p (type, head, task);
  else
    return priority_queue_task_in_list_p (type, &head->l, task);
}

/* Sanity check LIST to make sure the tasks therein are in the right
   order.  LIST is a priority list of type TYPE.

   The expected order is that GOMP_TASK_WAITING tasks come before
   GOMP_TASK_TIED/GOMP_TASK_ASYNC_RUNNING ones.

   If CHECK_DEPS is TRUE, we also check that parent_depends_on WAITING
   tasks come before !parent_depends_on WAITING tasks.  This is only
   applicable to the children queue, and the caller is expected to
   ensure that we are verifying the children queue.  */

static void
priority_list_verify (enum priority_queue_type type,
		      struct priority_list *list, bool check_deps)
{
  bool seen_tied = false;
  bool seen_plain_waiting = false;
  struct priority_node *p = list->tasks;
  if (p)
  {
    while (1)
    {
      struct gomp_task *t = priority_node_to_task (type, p);
      if (seen_tied && t->kind == GOMP_TASK_WAITING)
        gomp_fatal ("priority_queue_verify: WAITING task after TIED");
      if (t->kind >= GOMP_TASK_TIED)
        seen_tied = true;
      else if (check_deps && t->kind == GOMP_TASK_WAITING)
      {
        if (t->parent_depends_on)
        {
          if (seen_plain_waiting)
            gomp_fatal ("priority_queue_verify: parent_depends_on after !parent_depends_on");
        }
        else
          seen_plain_waiting = true;
      }
      p = p->next;
      if (p == list->tasks)
        break;
    }
  }
}

/* Callback type for priority_tree_verify_callback.  */
struct cbtype
{
  enum priority_queue_type type;
  bool check_deps;
};

/* Verify every task in NODE.

   Callback for splay_tree_foreach.  */

static void
priority_tree_verify_callback (prio_splay_tree_key key, void *data)
{
  struct cbtype *cb = (struct cbtype *) data;
  priority_list_verify (cb->type, &key->l, cb->check_deps);
}

/* Generic version of priority_list_verify.

   Sanity check HEAD to make sure the tasks therein are in the right
   order.  The priority_queue holds tasks of type TYPE.

   If CHECK_DEPS is TRUE, we also check that parent_depends_on WAITING
   tasks come before !parent_depends_on WAITING tasks.  This is only
   applicable to the children queue, and the caller is expected to
   ensure that we are verifying the children queue.  */

void
priority_queue_verify (enum priority_queue_type type,
		       struct priority_queue *head, bool check_deps)
{
  if (priority_queue_empty_p (head, MEMMODEL_RELAXED))
    return;
  if (priority_queue_multi_p (head))
    {
      struct cbtype cb = { type, check_deps };
      prio_splay_tree_foreach (&head->t,
			       priority_tree_verify_callback, &cb);
    }
  else
    priority_list_verify (type, &head->l, check_deps);
}
#endif /* _LIBGOMP_CHECKING_ */

/* Tree version of priority_list_insert.  */

void
priority_tree_insert (enum priority_queue_type type, struct priority_queue *head, struct priority_node *node,
            int priority, enum priority_insert_type pos, bool task_is_parent_depends_on, bool task_is_blocked)
{
  /* ?? The only reason this function is not inlined is because we
     need to find the ult_var variable within gomp_task_icv (which has
     not been completely defined in the header file).  If the lack of
     inlining is a concern, we could pass the ult_var variable as a
     parameter, or we could move this to libgomp.h.  */
  if (__builtin_expect (head->t.root == NULL, 0))
  {
    /* The first time around, transfer any priority 0 items to the
       tree.  */
    if (head->l.tasks != NULL)
    {
      prio_splay_tree_node k = gomp_malloc (sizeof (*k));
      k->left = NULL;
      k->right = NULL;
      /* HP-LIST */
      k->marked = false;
      k->prev = NULL;
      k->next = NULL;
      k->key.l.priority = 0;
      k->key.l.tasks = head->l.tasks;
      k->key.l.last_parent_depends_on = head->l.last_parent_depends_on;
      k->key.l.first_running_task = head->l.first_running_task;
      k->key.l.blocked_tasks = head->l.blocked_tasks;
      /* HP-LIST */
      if (priority_node_to_task (type, node)->icv.ult_var)
        prio_splay_tree_hp_list_insert (&head->t, k, (head->l.tasks && (priority_node_to_task (type, head->l.tasks)->kind == GOMP_TASK_WAITING)) ? true : false);
      else
        prio_splay_tree_insert (&head->t, k);
      head->l.tasks = NULL;
      head->l.blocked_tasks = NULL;
    }
  }
  struct priority_list *list = priority_queue_lookup_priority (head, priority);
  if (!list)
  {
    prio_splay_tree_node k = gomp_malloc (sizeof (*k));
    k->left = NULL;
    k->right = NULL;
    /* HP-LIST */
    k->marked = false;
    k->prev = NULL;
    k->next = NULL;
    k->key.l.priority = priority;
    k->key.l.tasks = NULL;
    k->key.l.last_parent_depends_on = NULL;
    k->key.l.first_running_task = NULL;
    k->key.l.blocked_tasks = NULL;
    /* HP-LIST */
    if (priority_node_to_task (type, node)->icv.ult_var)
    {
      if (prio_splay_tree_hp_list_insert (&head->t, k, !task_is_blocked) && !task_is_blocked)
        __atomic_store_n(&head->highest_priority, head->t.highest_marked->key.l.priority, MEMMODEL_RELEASE);
    }
    else
      prio_splay_tree_insert (&head->t, k);
    list = &k->key.l;
  }
  /* HP-LIST */
  else if (priority_node_to_task (type, node)->icv.ult_var && !task_is_blocked)
  {
    if (prio_splay_tree_hp_list_mark_node (&head->t, (prio_splay_tree_node) list, true))
      __atomic_store_n(&head->highest_priority, head->t.highest_marked->key.l.priority, MEMMODEL_RELEASE);
  }
  priority_list_insert (type, list, node, pos, task_is_parent_depends_on, task_is_blocked);
}

/* Tree version of priority_list_insert_running.  */

void
priority_tree_insert_running (enum priority_queue_type type, struct priority_queue *head,
                                struct priority_node *node, int priority)
{
  /* ?? The only reason this function is not inlined is because we
     need to find the ult_var variable within gomp_task_icv (which has
     not been completely defined in the header file).  If the lack of
     inlining is a concern, we could pass the ult_var variable as a
     parameter, or we could move this to libgomp.h.  */
  if (__builtin_expect (head->t.root == NULL, 0))
  {
    /* The first time around, transfer any priority 0 items to the
       tree.  */
    if (head->l.tasks != NULL)
    {
      prio_splay_tree_node k = gomp_malloc (sizeof (*k));
      k->left = NULL;
      k->right = NULL;
      /* HP-LIST */
      k->marked = false;
      k->prev = NULL;
      k->next = NULL;
      k->key.l.priority = 0;
      k->key.l.tasks = head->l.tasks;
      k->key.l.last_parent_depends_on = head->l.last_parent_depends_on;
      k->key.l.first_running_task = head->l.first_running_task;
      k->key.l.blocked_tasks = head->l.blocked_tasks;
      /* HP-LIST */
      if (priority_node_to_task (type, node)->icv.ult_var)
        prio_splay_tree_hp_list_insert (&head->t, k, (head->l.tasks && (priority_node_to_task (type, head->l.tasks)->kind == GOMP_TASK_WAITING)) ? true : false);
      else
        prio_splay_tree_insert (&head->t, k);
      head->l.tasks = NULL;
      head->l.blocked_tasks = NULL;
    }
  }
  struct priority_list *list = priority_queue_lookup_priority (head, priority);
  if (!list)
  {
    prio_splay_tree_node k = gomp_malloc (sizeof (*k));
    k->left = NULL;
    k->right = NULL;
    /* HP-LIST */
    k->marked = false;
    k->prev = NULL;
    k->next = NULL;
    k->key.l.priority = priority;
    k->key.l.tasks = NULL;
    k->key.l.last_parent_depends_on = NULL;
    k->key.l.first_running_task = NULL;
    k->key.l.blocked_tasks = NULL;
    /* HP-LIST */
    if (priority_node_to_task (type, node)->icv.ult_var)
      prio_splay_tree_hp_list_insert (&head->t, k, false);
    else
      prio_splay_tree_insert (&head->t, k);
    list = &k->key.l;
  }
  priority_list_insert_running (list, node);
}

/* Remove NODE from priority queue HEAD, wherever it may be inside the
   tree.  HEAD contains tasks of type TYPE.  */

void
priority_tree_remove (enum priority_queue_type type, struct priority_queue *head,
                          struct priority_node *node, bool task_is_blocked)
{
  /* ?? The only reason this function is not inlined is because we
     need to find the priority within gomp_task (which has not been
     completely defined in the header file).  If the lack of inlining
     is a concern, we could pass the priority number as a
     parameter, or we could move this to libgomp.h.  */
  int priority;
  struct gomp_task *task;

  task = priority_node_to_task (type, node);
  priority = task->priority;

  /* ?? We could avoid this lookup by keeping a pointer to the key in
     the priority_node.  */
  struct priority_list *list = priority_queue_lookup_priority (head, priority);
#if _LIBGOMP_CHECKING_
  if (!list)
    gomp_fatal ("Unable to find priority %d", priority);
#endif
  /* If NODE was the last in its priority, clean up the priority.  */
  if (priority_list_remove (type, list, node, task_is_blocked, MEMMODEL_RELAXED))
  {
    if (task->icv.ult_var)
    {
      /* HP-LIST */
      if (prio_splay_tree_hp_list_remove (&head->t, (prio_splay_tree_key) list))
      {
        if (head->t.highest_marked != NULL)
          __atomic_store_n(&head->highest_priority, head->t.highest_marked->key.l.priority, MEMMODEL_RELEASE);
        else
          __atomic_store_n(&head->highest_priority, 0, MEMMODEL_RELEASE);
      }
    }
    else
      prio_splay_tree_remove (&head->t, (prio_splay_tree_key) list);
    list->tasks = NULL;
    list->blocked_tasks = NULL;
#if _LIBGOMP_CHECKING_
    memset (list, 0xaf, sizeof (*list));
#endif
    free (list);
  }
  /* HP-LIST */
  else if (task->icv.ult_var && !task_is_blocked)
  {
    struct gomp_task *first_task;
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
}

/* Priority splay trees comparison function.  */
static inline int
prio_splay_compare (prio_splay_tree_key x, prio_splay_tree_key y)
{
  if (x->l.priority == y->l.priority)
    return 0;
  return x->l.priority < y->l.priority ? -1 : 1;
}

/* Define another splay tree instantiation, for priority_list's.  */
#define splay_tree_prefix prio
#define splay_tree_c
#include "splay-tree.h"
