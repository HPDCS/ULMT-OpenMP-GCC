#if _LIBGOMP_TEAM_TIMING_

/* Header file for measuring execution time of the team.  */

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

struct gomp_team_time
{
	uint64_t start;
	uint64_t end;
};

extern struct gomp_task_icv gomp_global_icv;

static inline __attribute__((always_inline)) void
gomp_save_team_start_time (struct gomp_team_time *team_time)
{
  team_time->start = RDTSC();
}

static inline __attribute__((always_inline)) void
gomp_save_team_end_time (struct gomp_team_time *team_time)
{
  team_time->end = RDTSC();
}

static inline __attribute__((always_inline)) void
gomp_print_team_time (struct gomp_team_time *team_time)
{
  size_t length;
  time_t rawtime;
  struct tm *timeinfo;
  char filename[128];

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  if (gomp_global_icv.ult_var)
    length = strftime(filename, 128, "ult_omp_team_time_%d-%m-%Y_%H-%M-%S-", timeinfo);
  else
    length = strftime(filename, 128, "bsl_omp_team_time_%d-%m-%Y_%H-%M-%S-", timeinfo);
  snprintf(&filename[length], (128 - length), "(%llu).txt", (unsigned long long int) RDTSC());

  FILE *file = fopen(filename, "w");
  fprintf(file, "Time: %llu\n", ((unsigned long long int) (team_time->end - team_time->start)));
  fflush(file);
  fclose(file);
}

#endif