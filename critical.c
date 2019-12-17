/* Copyright (C) 2005-2017 Free Software Foundation, Inc.
   Contributed by Richard Henderson <rth@redhat.com>.

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

/* This file handles the CRITICAL construct.  */

#include "libgomp.h"
#include <stdlib.h>
#include <string.h>

static gomp_mutex_t default_lock;
static struct priority_queue default_locked;

void
GOMP_critical_start (void)
{
  int ret;
  struct gomp_thread *thr = gomp_thread ();

  /* There is an implicit flush on entry to a critical region. */
  __atomic_thread_fence (MEMMODEL_RELEASE);
  if (thr->task && thr->task->icv.ult_var)
  {
    __atomic_store_n (&thr->non_preemptable, thr->non_preemptable+1, MEMMODEL_SEQ_CST);
    while (gomp_mutex_trylock (&default_lock))
    {
      __atomic_store_n (&thr->non_preemptable, thr->non_preemptable-1, MEMMODEL_SEQ_CST);
#if GOMP_ATOMIC_READ_MUTEX
      do
      {
#endif
        if ((ret = gomp_locked_task_switch ((struct priority_queue *) &default_locked, &default_lock)) == 0)
          thr = gomp_thread ();
        else if (ret == 2)
          break;
#if GOMP_ATOMIC_READ_MUTEX
      }
      while (__atomic_load_n(&default_lock, MEMMODEL_ACQUIRE));
#endif
      __atomic_store_n (&thr->non_preemptable, thr->non_preemptable+1, MEMMODEL_SEQ_CST);
    }
  }
  else
    gomp_mutex_lock (&default_lock);
}

void
GOMP_critical_end (void)
{
  struct gomp_thread *thr = gomp_thread ();

  gomp_mutex_unlock (&default_lock);

  if (thr->task && thr->task->icv.ult_var)
  {
    if (thr->non_preemptable)
      __atomic_store_n (&thr->non_preemptable, thr->non_preemptable-1, MEMMODEL_SEQ_CST);

    gomp_task_handle_locking ((struct priority_queue *) &default_locked);
  }
}

#ifndef HAVE_SYNC_BUILTINS
static gomp_mutex_t create_lock_lock;
#endif

typedef union {
  void * ptr;
  unsigned long long int ull;
} ptr_ull;

static inline __attribute__((always_inline)) size_t
gomp_get_mutex_locked_size(void)
{
  size_t smutex = sizeof (gomp_mutex_t);

  if ((smutex % 64) == 0)
    return (smutex + 128);
  else
    return (smutex + (64 - (smutex % 64)) + 128);
}

static inline __attribute__((always_inline)) void *
gomp_get_mutex_locked_address(gomp_mutex_t *mutex)
{
  ptr_ull pu;
  unsigned long long int smutex;

  pu.ptr = (void *) mutex;
  smutex = (unsigned long long int) sizeof (gomp_mutex_t);

  if (((pu.ull + smutex) % 64) == 0)
    pu.ptr += (size_t) smutex;
  else
    pu.ptr += (size_t) (smutex + (64 - ((pu.ull + smutex) % 64)));

  return pu.ptr;
}

void
GOMP_critical_name_start (void **pptr)
{
  int ret;
  struct gomp_thread *thr = gomp_thread ();
  gomp_mutex_t * plock = *pptr;

  if (plock == NULL)
  {
#ifdef HAVE_SYNC_BUILTINS
    gomp_mutex_t *nlock = (gomp_mutex_t *) gomp_aligned_alloc (64, gomp_get_mutex_locked_size());

    gomp_mutex_init (nlock);
    memset(gomp_get_mutex_locked_address(nlock), 0, sizeof(void *));

    plock = __sync_val_compare_and_swap (pptr, NULL, nlock);
    if (plock != NULL)
    {
      gomp_mutex_destroy (nlock);
      free (nlock);
    }
    else
      plock = nlock;
#else
    gomp_mutex_lock (&create_lock_lock);
    plock = *pptr;
    if (plock == NULL)
    {
      plock = (gomp_mutex_t *) gomp_aligned_alloc (64, gomp_get_mutex_locked_size());

      gomp_mutex_init (plock);
      memset(gomp_get_mutex_locked_address(plock), 0, sizeof(void *));

      __sync_synchronize ();

      *pptr = plock;
    }
    gomp_mutex_unlock (&create_lock_lock);
#endif
  }

  if (thr->task && thr->task->icv.ult_var)
  {
    __atomic_store_n (&thr->non_preemptable, thr->non_preemptable+1, MEMMODEL_SEQ_CST);
    while (gomp_mutex_trylock (plock))
    {
      __atomic_store_n (&thr->non_preemptable, thr->non_preemptable-1, MEMMODEL_SEQ_CST);
#if GOMP_ATOMIC_READ_MUTEX
      do
      {
#endif
        if ((ret = gomp_locked_task_switch ((struct priority_queue *) gomp_get_mutex_locked_address(plock), plock)) == 0)
          thr = gomp_thread ();
        else if (ret == 2)
          break;
#if GOMP_ATOMIC_READ_MUTEX
      }
      while (__atomic_load_n(plock, MEMMODEL_ACQUIRE));
#endif
      __atomic_store_n (&thr->non_preemptable, thr->non_preemptable+1, MEMMODEL_SEQ_CST);
    }
  }
  else
    gomp_mutex_lock (plock);
}

void
GOMP_critical_name_end (void **pptr)
{
  struct gomp_thread *thr = gomp_thread ();
  gomp_mutex_t * plock = *pptr;

  gomp_mutex_unlock (plock);

  if (thr->task && thr->task->icv.ult_var)
  {
    if (thr->non_preemptable)
      __atomic_store_n (&thr->non_preemptable, thr->non_preemptable-1, MEMMODEL_SEQ_CST);

    gomp_task_handle_locking ((struct priority_queue *) gomp_get_mutex_locked_address(plock));
  }
}

#if !GOMP_MUTEX_INIT_0
static void __attribute__((constructor))
initialize_critical (void)
{
  gomp_mutex_init (&default_lock);
  memset(&default_locked, 0, sizeof(void *));
#ifndef HAVE_SYNC_BUILTINS
  gomp_mutex_init (&create_lock_lock);
#endif
}
#endif
