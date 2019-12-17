#if _LIBGOMP_TASK_GRANULARITY_

/* Header file for tracking tasks granularity at run-time.  */

#define HTABLE_N  251ULL
#define HTABLE_A   13ULL
#define HTABLE_B    5ULL

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

struct gomp_task_granularity
{
  uint64_t num;
  uint64_t sum;
};

struct gomp_task_granularity_node
{
	struct gomp_task_granularity_node *next;
  void (*fn) (void *);
  struct gomp_task_granularity task_granularity;
};

struct gomp_task_granularity_table
{
	struct gomp_task_granularity_node *task_granularity_list[HTABLE_N];
};

static inline __attribute__((always_inline)) void
gomp_init_task_granularity_table (struct gomp_task_granularity_table **table)
{
  if ((*table) == NULL)
    (*table) = gomp_malloc (sizeof(struct gomp_task_granularity_table));
  memset((*table), 0, sizeof(struct gomp_task_granularity_table));
}

static inline __attribute__((always_inline)) struct gomp_task_granularity_node *
gomp_alloc_task_granularity_node (struct gomp_task_granularity_node *next, void (*fn) (void *), uint64_t occur, uint64_t time)
{
  struct gomp_task_granularity_node *node = gomp_malloc (sizeof(struct gomp_task_granularity_node));
  node->next = next;
  node->fn = fn;
  node->task_granularity.num = occur;
  node->task_granularity.sum = time;
  return node;
}

static inline __attribute__((always_inline)) void
gomp_save_task_granularity (struct gomp_task_granularity_table *table, void (*fn) (void *), uint64_t time)
{
  uint64_t key;
  struct gomp_task_granularity_node *node;
  
  key = (((((uint64_t) fn) << 4) * HTABLE_A) + HTABLE_B) % HTABLE_N;

  if ((node = table->task_granularity_list[key]) != NULL)
  {
    do
    {
      if (node->fn == fn)
      {
        node->task_granularity.num += 1ULL;
        node->task_granularity.sum += time;
        return;
      }
      node = node->next;
    }
    while (node != NULL);
  }

  table->task_granularity_list[key] = gomp_alloc_task_granularity_node (table->task_granularity_list[key], fn, 1ULL, time);
}

static inline __attribute__((always_inline)) void
gomp_save_cumulative_granularities (struct gomp_task_granularity_table *table, void (*fn) (void *), uint64_t num, uint64_t sum)
{
  uint64_t key;
  struct gomp_task_granularity_node *node;
  
  key = (((((uint64_t) fn) << 4) * HTABLE_A) + HTABLE_B) % HTABLE_N;

  if ((node = table->task_granularity_list[key]) != NULL)
  {
    do
    {
      if (node->fn == fn)
      {
        node->task_granularity.num += num;
        node->task_granularity.sum += sum;
        return;
      }
      node = node->next;
    }
    while (node != NULL);
  }

  table->task_granularity_list[key] = gomp_alloc_task_granularity_node (table->task_granularity_list[key], fn, num, sum);
}

static inline __attribute__((always_inline)) void
gomp_print_task_granularity (struct gomp_task_granularity_table **table, unsigned nthreads)
{
  unsigned tid;
  uint64_t key;
  struct gomp_task_granularity_node *node;

  size_t length;
  time_t rawtime;
  struct tm *timeinfo;
  char filename[128];

  for (tid = 1; tid < nthreads; tid++)
    for (key = 0; key < HTABLE_N; key++)
      if ((node = table[tid]->task_granularity_list[key]) != NULL)
        do
        {
          gomp_save_cumulative_granularities (table[0], node->fn, node->task_granularity.num, node->task_granularity.sum);
          table[tid]->task_granularity_list[key] = node->next;
          free (node);
          node = table[tid]->task_granularity_list[key];
        }
        while (node != NULL);

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  if (gomp_global_icv.ult_var)
    length = strftime(filename, 128, "ult_omp_task_granularity_%d-%m-%Y_%H-%M-%S-", timeinfo);
  else
    length = strftime(filename, 128, "bsl_omp_task_granularity_%d-%m-%Y_%H-%M-%S-", timeinfo);
  snprintf(&filename[length], (128 - length), "(%llu).txt", (unsigned long long int) RDTSC());

  FILE *file = fopen(filename, "w");
  for (key = 0; key < HTABLE_N; key++)
    if ((node = table[0]->task_granularity_list[key]) != NULL)
      do
      {
        fprintf(file, "\nFunction: %p\nNumber: %llu\nSum: %llu\nMean: %.0f\n\n", node->fn, (unsigned long long int) node->task_granularity.num,
          (unsigned long long int) node->task_granularity.sum, ((double) node->task_granularity.sum / (double) node->task_granularity.num));
        table[0]->task_granularity_list[key] = node->next;
        free (node);
        node = table[0]->task_granularity_list[key];
      }
      while (node != NULL);
  fflush(file);
  fclose(file);
}

#endif