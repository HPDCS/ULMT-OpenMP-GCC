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

/* This is a generic implementation of the public OpenMP locking primitives in
   terms of internal gomp_mutex_t.  It is not meant to be compiled on its own.
   It is #include'd from config/{linux,nvptx}/lock.c.  */

#include "libgomp.h"
#include <string.h>

/* The internal gomp_mutex_t and the external non-recursive omp_lock_t
   have the same form.  Re-use it.  */

typedef union {
  void * ptr;
  unsigned long long int ull;
} ptr_ull;

static inline __attribute__((always_inline)) void *
gomp_get_omp_lock_locked_address(omp_lock_t *lock)
{
  ptr_ull pu;
  unsigned long long int slock;

  pu.ptr = (void *) lock;
  slock = (unsigned long long int) sizeof (omp_lock_t);

  if (((pu.ull + slock) % 64) == 0)
    pu.ptr += (size_t) slock;
  else
    pu.ptr += (size_t) (slock + (64 - ((pu.ull + slock) % 64)));

  return pu.ptr;
}

void
gomp_init_lock_30 (omp_lock_t *lock)
{
  gomp_mutex_init (lock);
  memset(gomp_get_omp_lock_locked_address(lock), 0, 128);
}

void
gomp_destroy_lock_30 (omp_lock_t *lock)
{
  gomp_mutex_destroy (lock);
}

void
gomp_set_lock_30 (omp_lock_t *lock)
{
  int ret;
  struct gomp_thread *thr = gomp_thread ();

  if (thr->task && thr->task->icv.ult_var)
  {
    __atomic_store_n (&thr->non_preemptable, thr->non_preemptable+1, MEMMODEL_SEQ_CST);
    while (gomp_mutex_trylock (lock))
    {
      __atomic_store_n (&thr->non_preemptable, thr->non_preemptable-1, MEMMODEL_SEQ_CST);
#if GOMP_ATOMIC_READ_OMPLOCK
      do
      {
#endif
        if ((ret = gomp_locked_task_switch ((struct priority_queue *) gomp_get_omp_lock_locked_address(lock), lock)) == 0)
          thr = gomp_thread ();
        else if (ret == 2)
          break;
#if GOMP_ATOMIC_READ_OMPLOCK
      }
      while (__atomic_load_n(lock, MEMMODEL_ACQUIRE));
#endif
      __atomic_store_n (&thr->non_preemptable, thr->non_preemptable+1, MEMMODEL_SEQ_CST);
    }
  }
  else
    gomp_mutex_lock (lock);
}

void
gomp_unset_lock_30 (omp_lock_t *lock)
{
  struct gomp_thread *thr = gomp_thread ();

  gomp_mutex_unlock (lock);

  if (thr->task && thr->task->icv.ult_var)
  {
    if (thr->non_preemptable)
      __atomic_store_n (&thr->non_preemptable, thr->non_preemptable-1, MEMMODEL_SEQ_CST);
    
    gomp_task_handle_locking ((struct priority_queue *) gomp_get_omp_lock_locked_address(lock));
  }
}

int
gomp_test_lock_30 (omp_lock_t *lock)
{
  struct gomp_thread *thr = gomp_thread ();

  if (thr->task && thr->task->icv.ult_var)
  {
    __atomic_store_n (&thr->non_preemptable, thr->non_preemptable+1, MEMMODEL_SEQ_CST);
    if (gomp_mutex_trylock (lock))
    {
      __atomic_store_n (&thr->non_preemptable, thr->non_preemptable-1, MEMMODEL_SEQ_CST);
      return 0;
    }
    return 1;
  }
  else
  {
    int oldval = 0;
    /* ERROR: this operation may be wrong in case omp_lock_t is
              an alias for pthread_mutex_t or sem_t.  Should be
              reported on the libGOMP's bugzilla web page.  */
    return __atomic_compare_exchange_n (lock, &oldval, 1, false, MEMMODEL_ACQUIRE, MEMMODEL_RELAXED);
  }
}

void
gomp_init_nest_lock_30 (omp_nest_lock_t *lock)
{
  memset (lock, '\0', sizeof (*lock));
}

void
gomp_destroy_nest_lock_30 (omp_nest_lock_t *lock)
{
}

void
gomp_set_nest_lock_30 (omp_nest_lock_t *lock)
{
  void *me = gomp_icv (true);

  if (lock->owner != me)
    {
      gomp_mutex_lock (&lock->lock);
      lock->owner = me;
    }

  lock->count++;
}

void
gomp_unset_nest_lock_30 (omp_nest_lock_t *lock)
{
  if (--lock->count == 0)
    {
      lock->owner = NULL;
      gomp_mutex_unlock (&lock->lock);
    }
}

int
gomp_test_nest_lock_30 (omp_nest_lock_t *lock)
{
  void *me = gomp_icv (true);
  int oldval;

  if (lock->owner == me)
    return ++lock->count;

  oldval = 0;
  if (__atomic_compare_exchange_n (&lock->lock, &oldval, 1, false,
				   MEMMODEL_ACQUIRE, MEMMODEL_RELAXED))
    {
      lock->owner = me;
      lock->count = 1;
      return 1;
    }

  return 0;
}
