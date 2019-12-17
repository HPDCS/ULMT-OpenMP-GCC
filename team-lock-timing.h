#if _LIBGOMP_TEAM_LOCK_TIMING_

/* Header file for tracking time spent while holding the Team's lock.  */

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

struct gomp_team_lock_time
{
  uint64_t entry_acquisition_time;
  uint64_t lock_acquisition_time;
  uint64_t entry_time;
  uint64_t lock_time;
};

static inline __attribute__((always_inline)) void
gomp_init_team_lock_time (struct gomp_team_lock_time **team_lock_time)
{
  if ((*team_lock_time) == NULL)
    (*team_lock_time) = gomp_malloc (sizeof(struct gomp_team_lock_time));
  memset((*team_lock_time), 0, sizeof(struct gomp_team_lock_time));
}

static inline __attribute__((always_inline)) void
gomp_print_team_lock_time (struct gomp_team_lock_time **team_lock_time, unsigned nthreads)
{
  unsigned tid;

  size_t length;
  time_t rawtime;
  struct tm *timeinfo;
  char filename[128];

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  if (gomp_global_icv.ult_var)
    length = strftime(filename, 128, "ult_omp_team_lock_time_%d-%m-%Y_%H-%M-%S-", timeinfo);
  else
    length = strftime(filename, 128, "bsl_omp_team_lock_time_%d-%m-%Y_%H-%M-%S-", timeinfo);
  snprintf(&filename[length], (128 - length), "(%llu).txt", (unsigned long long int) RDTSC());

  FILE *file = fopen(filename, "w");
  for (tid = 0; tid < nthreads; tid++)
    if (team_lock_time[tid] != NULL)
      fprintf(file, "Thread: %u\nAcquisition-Team-Lock-Time: %llu\nTeam-Lock-Time: %llu\n\n", tid,
        (unsigned long long int) team_lock_time[tid]->lock_acquisition_time, (unsigned long long int) team_lock_time[tid]->lock_time);
  fflush(file);
  fclose(file);
}

#endif