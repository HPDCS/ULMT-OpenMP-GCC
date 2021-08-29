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

/* This file defines the OpenMP API entry points that operate on internal
   control variables.  */

#include "libgomp.h"
#include "gomp-constants.h"
#include <limits.h>

void
omp_set_num_threads (int n)
{
  struct gomp_task_icv *icv = gomp_icv (true);
  icv->nthreads_var = (n > 0 ? n : 1);
}

void
omp_set_dynamic (int val)
{
  struct gomp_task_icv *icv = gomp_icv (true);
  icv->dyn_var = val;
}

int
omp_get_dynamic (void)
{
  struct gomp_task_icv *icv = gomp_icv (false);
  return icv->dyn_var;
}

void
omp_set_work_first_sched (int val)
{
  struct gomp_task_icv *icv = gomp_icv (true);
  icv->wf_sched_var = val;
}

int
omp_get_work_first_sched (void)
{
  struct gomp_task_icv *icv = gomp_icv (false);
  return icv->wf_sched_var;
}

void
omp_set_untied_block (int val)
{
  struct gomp_task_icv *icv = gomp_icv (true);
  icv->untied_block_var = val;
}

int
omp_get_untied_block (void)
{
  struct gomp_task_icv *icv = gomp_icv (false);
  return icv->untied_block_var;
}

void
omp_set_ult_threads (int val)
{
  struct gomp_task_icv *icv = gomp_icv (true);
  icv->ult_var = val;
}

int
omp_get_ult_threads (void)
{
  struct gomp_task_icv *icv = gomp_icv (false);
  return icv->ult_var;
}

void
omp_set_ult_stack (unsigned long val)
{
  struct gomp_task_icv *icv = gomp_icv (true);
  icv->ult_stack_size = (val > ULT_STACK_SIZE ? val : ULT_STACK_SIZE);
}

unsigned long
omp_get_ult_stack (void)
{
  struct gomp_task_icv *icv = gomp_icv (false);
  return icv->ult_stack_size;
}

#if defined HAVE_TLS || defined USE_EMUTLS
void
omp_set_text_section_addresses(void *start, void *end)
{
  struct gomp_task_icv *icv = gomp_icv (true);
  icv->text_section_start = start;
  icv->text_section_end = end;
}
#endif

void
omp_set_nested (int val)
{
  struct gomp_task_icv *icv = gomp_icv (true);
  icv->nest_var = val;
}

int
omp_get_nested (void)
{
  struct gomp_task_icv *icv = gomp_icv (false);
  return icv->nest_var;
}

void
omp_set_schedule (omp_sched_t kind, int chunk_size)
{
  struct gomp_task_icv *icv = gomp_icv (true);
  switch (kind)
    {
    case omp_sched_static:
      if (chunk_size < 1)
	chunk_size = 0;
      icv->run_sched_chunk_size = chunk_size;
      break;
    case omp_sched_dynamic:
    case omp_sched_guided:
      if (chunk_size < 1)
	chunk_size = 1;
      icv->run_sched_chunk_size = chunk_size;
      break;
    case omp_sched_auto:
      break;
    default:
      return;
    }
  icv->run_sched_var = kind;
}

void
omp_get_schedule (omp_sched_t *kind, int *chunk_size)
{
  struct gomp_task_icv *icv = gomp_icv (false);
  *kind = icv->run_sched_var;
  *chunk_size = icv->run_sched_chunk_size;
}

int
omp_get_max_threads (void)
{
  struct gomp_task_icv *icv = gomp_icv (false);
  return icv->nthreads_var;
}

int
omp_get_thread_limit (void)
{
  struct gomp_task_icv *icv = gomp_icv (false);
  return icv->thread_limit_var > INT_MAX ? INT_MAX : icv->thread_limit_var;
}

void
omp_set_max_active_levels (int max_levels)
{
  if (max_levels >= 0)
    gomp_max_active_levels_var = max_levels;
}

int
omp_get_max_active_levels (void)
{
  return gomp_max_active_levels_var;
}

int
omp_get_cancellation (void)
{
  return gomp_cancel_var;
}

int
omp_get_autocutoff (void)
{
  return gomp_auto_cutoff_var;
}

int
omp_get_signal_unblock (void)
{
  return gomp_signal_unblock;
}

int
omp_get_ipi (void)
{
  return gomp_ipi_var;
}

double
omp_get_ipi_decision_model (void)
{
  return gomp_ipi_decision_model;
}

void
omp_set_ipi_priority_gap (unsigned long val)
{
  gomp_ipi_priority_gap = val;
}

int
omp_get_ipi_priority_gap (void)
{
  return gomp_ipi_priority_gap;
}

void
omp_set_ipi_sending_cap (unsigned long val)
{
  gomp_ipi_sending_cap = val;
}

int
omp_get_ipi_sending_cap (void)
{
  return gomp_ipi_sending_cap;
}

int
omp_get_ibs_rate (void)
{
  return gomp_ibs_rate_var;
}

void
omp_set_queue_policy (unsigned long val)
{
  gomp_queue_policy_var = val;
}

int
omp_get_queue_policy (void)
{
  return gomp_queue_policy_var;
}

int
omp_get_max_task_priority (void)
{
  return gomp_max_task_priority_var;
}

omp_proc_bind_t
omp_get_proc_bind (void)
{
  struct gomp_task_icv *icv = gomp_icv (false);
  return icv->bind_var;
}

int
omp_get_initial_device (void)
{
  return GOMP_DEVICE_HOST_FALLBACK;
}

int
omp_get_num_places (void)
{
  return gomp_places_list_len;
}

int
omp_get_place_num (void)
{
  if (gomp_places_list == NULL)
    return -1;

  struct gomp_thread *thr = gomp_thread ();
  if (thr->place == 0)
    gomp_init_affinity ();

  return (int) thr->place - 1;
}

int
omp_get_partition_num_places (void)
{
  if (gomp_places_list == NULL)
    return 0;

  struct gomp_thread *thr = gomp_thread ();
  if (thr->place == 0)
    gomp_init_affinity ();

  return thr->ts.place_partition_len;
}

void
omp_get_partition_place_nums (int *place_nums)
{
  if (gomp_places_list == NULL)
    return;

  struct gomp_thread *thr = gomp_thread ();
  if (thr->place == 0)
    gomp_init_affinity ();

  unsigned int i;
  for (i = 0; i < thr->ts.place_partition_len; i++)
    *place_nums++ = thr->ts.place_partition_off + i;
}

ialias (omp_set_dynamic)
ialias (omp_set_nested)
ialias (omp_set_work_first_sched)
ialias (omp_get_work_first_sched)
ialias (omp_set_ult_threads)
ialias (omp_set_ult_stack)
ialias (omp_set_text_section_addresses)
ialias (omp_set_num_threads)
ialias (omp_get_dynamic)
ialias (omp_get_nested)
ialias (omp_get_ult_threads)
ialias (omp_get_ult_stack)
ialias (omp_set_schedule)
ialias (omp_get_schedule)
ialias (omp_get_max_threads)
ialias (omp_get_thread_limit)
ialias (omp_set_max_active_levels)
ialias (omp_get_max_active_levels)
ialias (omp_get_cancellation)
ialias (omp_get_autocutoff)
ialias (omp_get_ipi)
ialias (omp_get_ipi_decision_model)
ialias (omp_get_signal_unblock)
ialias (omp_set_ipi_priority_gap)
ialias (omp_get_ipi_priority_gap)
ialias (omp_set_ipi_sending_cap)
ialias (omp_get_ipi_sending_cap)
ialias (omp_get_ibs_rate)
ialias (omp_set_queue_policy)
ialias (omp_get_queue_policy)
ialias (omp_get_proc_bind)
ialias (omp_get_initial_device)
ialias (omp_get_max_task_priority)
ialias (omp_get_num_places)
ialias (omp_get_place_num)
ialias (omp_get_partition_num_places)
ialias (omp_get_partition_place_nums)
