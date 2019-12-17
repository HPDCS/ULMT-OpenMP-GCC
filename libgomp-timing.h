#if _LIBGOMP_LIBGOMP_TIMING_

/* Header file for tracking library function execution time.  */

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

struct gomp_libgomp_time
{
	uint64_t entry_time;
  uint64_t gomp_time;
};

static inline __attribute__((always_inline)) void
gomp_init_libgomp_time (struct gomp_libgomp_time **libgomp_time)
{
  if ((*libgomp_time) == NULL)
    (*libgomp_time) = gomp_malloc (sizeof(struct gomp_libgomp_time));
  memset((*libgomp_time), 0, sizeof(struct gomp_libgomp_time));
}

static inline __attribute__((always_inline)) void
gomp_print_libgomp_time (struct gomp_libgomp_time **libgomp_time, unsigned nthreads)
{
  unsigned tid;

  size_t length;
  time_t rawtime;
  struct tm *timeinfo;
  char filename[128];

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  if (gomp_global_icv.ult_var)
    length = strftime(filename, 128, "ult_omp_libgomp_time_%d-%m-%Y_%H-%M-%S-", timeinfo);
  else
    length = strftime(filename, 128, "bsl_omp_libgomp_time_%d-%m-%Y_%H-%M-%S-", timeinfo);
  snprintf(&filename[length], (128 - length), "(%llu).txt", (unsigned long long int) RDTSC());

  FILE *file = fopen(filename, "w");
  for (tid = 0; tid < nthreads; tid++)
    if (libgomp_time[tid] != NULL)
      fprintf(file, "Thread: %u\nLIBGOMP-Time: %llu\n\n", tid, (unsigned long long int) libgomp_time[tid]->gomp_time);
  fflush(file);
  fclose(file);
}

#endif