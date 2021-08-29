/* Header file for tracking execution time of tasks.  */

#define HTABLE_N  251ULL
#define HTABLE_A   13ULL
#define HTABLE_B    5ULL

#define TASK_KIND(x) ({ \
  char *kind = NULL; \
  switch(x) \
  { \
    case 0: \
      kind = "IMPLICIT"; \
      break; \
    case 1: \
      kind = "UNDEFERRED"; \
      break; \
    case 2: \
      kind = "WAITING"; \
      break; \
    case 3: \
      kind = "DEFERRED"; \
      break; \
    case 4: \
      kind = "ASYNC_RUNNING"; \
      break; \
    default: \
      kind = "?"; \
      break; \
  } \
  kind; \
})

#define TASK_TYPE(x) ({ \
  char *type = NULL; \
  switch(x) \
  { \
    case 0: \
      type = "TIED"; \
      break; \
    case 1: \
      type = "UNTIED"; \
      break; \
    default: \
      type = "?"; \
      break; \
  } \
  type; \
})

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

#if _LIBGOMP_TASK_TIMING_
extern bool gomp_ipi_var;
#endif
extern double gomp_ipi_decision_model;

struct gomp_task_time
{
#if _LIBGOMP_TASK_TIMING_
	uint64_t num;
	uint64_t sum;
#endif
  double ema;
};

struct gomp_task_time_node
{
  struct gomp_task_time_node *next;
  void (*fn) (void *);
  struct gomp_task_time task_time[DEFAULT_NUM_PRIORITIES][GOMP_TASK_KIND_ENUM_SIZE][GOMP_TASK_TYPE_ENUM_SIZE];
};

struct gomp_task_time_table
{
  struct gomp_task_time_node *task_time_list[HTABLE_N];
};

static inline __attribute__((always_inline)) struct gomp_task_time_node *
gomp_alloc_task_time_node (struct gomp_task_time_node *next, void (*fn) (void *),
                            enum gomp_task_kind kind, enum gomp_task_type type, int priority,
                              uint64_t occur, uint64_t time)
{
  struct gomp_task_time_node *node = gomp_malloc_cleared (sizeof(struct gomp_task_time_node));
  node->next = next;
  node->fn = fn;
#if _LIBGOMP_TASK_TIMING_
  node->task_time[priority][kind][type].num = occur;
  node->task_time[priority][kind][type].sum = time;
#endif
  node->task_time[priority][kind][type].ema = (double) time;
  return node;
}

static inline __attribute__((always_inline)) uint64_t
gomp_get_task_time (struct gomp_task_time_table *table, void (*fn) (void *),
                      enum gomp_task_kind kind, enum gomp_task_type type, int priority)
{
  uint64_t key;
  struct gomp_task_time_node *node;

  if (priority > DEFAULT_NUM_PRIORITIES)
    priority = DEFAULT_NUM_PRIORITIES;
  
  key = (((((uint64_t) fn) << 4) * HTABLE_A) + HTABLE_B) % HTABLE_N;

  if ((node = table->task_time_list[key]) != NULL)
  {
    do
    {
      if (node->fn == fn)
        return (uint64_t) node->task_time[priority][kind][type].ema;

      node = node->next;
    }
    while (node != NULL);
  }

  return (uint64_t) ~(0ULL);
}

static inline __attribute__((always_inline)) void
gomp_save_task_time (struct gomp_task_time_table *table, void (*fn) (void *),
                      enum gomp_task_kind kind, enum gomp_task_type type, int priority,
                        uint64_t time)
{
  uint64_t key;
  struct gomp_task_time_node *node;

  if (priority > DEFAULT_NUM_PRIORITIES)
    priority = DEFAULT_NUM_PRIORITIES;
  
  key = (((((uint64_t) fn) << 4) * HTABLE_A) + HTABLE_B) % HTABLE_N;

  if ((node = table->task_time_list[key]) != NULL)
  {
    do
    {
      if (node->fn == fn)
      {
#if _LIBGOMP_TASK_TIMING_
        node->task_time[priority][kind][type].num += 1ULL;
        node->task_time[priority][kind][type].sum += time;
        if (gomp_ipi_var && gomp_ipi_decision_model > 0.0)
#endif
        {
          node->task_time[priority][kind][type].ema = (gomp_ipi_decision_model * (double) time) + \
            ((1.0 - gomp_ipi_decision_model) * node->task_time[priority][kind][type].ema);
        }
        return;
      }
      node = node->next;
    }
    while (node != NULL);
  }

  table->task_time_list[key] = gomp_alloc_task_time_node (table->task_time_list[key], fn, kind, type, priority, 1ULL, time);
}

static inline __attribute__((always_inline)) void
gomp_init_task_time (struct gomp_task_time_table **table)
{
  if ((*table) == NULL)
    (*table) = gomp_malloc (sizeof(struct gomp_task_time_table));
  memset((*table), 0, sizeof(struct gomp_task_time_table));
}

#if _LIBGOMP_TASK_TIMING_

static inline __attribute__((always_inline)) void
gomp_save_cumulative_times (struct gomp_task_time_table *table, void (*fn) (void *),
                            enum gomp_task_kind kind, enum gomp_task_type type, int priority,
                              uint64_t num, uint64_t sum)
{
  uint64_t key;
  struct gomp_task_time_node *node;
  
  key = (((((uint64_t) fn) << 4) * HTABLE_A) + HTABLE_B) % HTABLE_N;

  if ((node = table->task_time_list[key]) != NULL)
  {
    do
    {
      if (node->fn == fn)
      {
        node->task_time[priority][kind][type].num += num;
        node->task_time[priority][kind][type].sum += sum;
        return;
      }
      node = node->next;
    }
    while (node != NULL);
  }

  table->task_time_list[key] = gomp_alloc_task_time_node (table->task_time_list[key], fn, kind, type, priority, num, sum);
}

static inline __attribute__((always_inline)) void
gomp_print_task_time (struct gomp_task_time_table **table, unsigned nthreads)
{
  int p, k, t;

  unsigned tid;
  uint64_t key;
  struct gomp_task_time_node *node;

  size_t length;
  time_t rawtime;
  struct tm *timeinfo;
  char filename[128];

  for (tid = 1; tid < nthreads; tid++)
    for (key = 0; key < HTABLE_N; key++)
      if ((node = table[tid]->task_time_list[key]) != NULL)
        do
        {
          for (p=0; p<DEFAULT_NUM_PRIORITIES; p++)
            for (k=0; k<GOMP_TASK_KIND_ENUM_SIZE; k++)
              for (t=0; t<GOMP_TASK_TYPE_ENUM_SIZE; t++)
                gomp_save_cumulative_times (table[0], node->fn, k, t, p, node->task_time[p][k][t].num, node->task_time[p][k][t].sum);
          table[tid]->task_time_list[key] = node->next;
          free (node);
          node = table[tid]->task_time_list[key];
        }
        while (node != NULL);

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  if (gomp_global_icv.ult_var)
    length = strftime(filename, 128, "ult_omp_task_time_%d-%m-%Y_%H-%M-%S-", timeinfo);
  else
    length = strftime(filename, 128, "bsl_omp_task_time_%d-%m-%Y_%H-%M-%S-", timeinfo);
  snprintf(&filename[length], (128 - length), "(%llu).txt", (unsigned long long int) RDTSC());

  FILE *file = fopen(filename, "w");
  for (key = 0; key < HTABLE_N; key++)
    if ((node = table[0]->task_time_list[key]) != NULL)
      do
      {
        for (p=0; p<DEFAULT_NUM_PRIORITIES; p++)
          for (k=0; k<GOMP_TASK_KIND_ENUM_SIZE; k++)
            for (t=0; t<GOMP_TASK_TYPE_ENUM_SIZE; t++)
              if (node->task_time[p][k][t].num > 0)
                fprintf(file, "\nFunction: %p\nPriority: %llu\nKind: %s\nType: %s\nNumber: %llu\nSum: %llu\nMean: %.0f\n\n",
                  node->fn, (unsigned long long int) p, TASK_KIND(k), TASK_TYPE(t), (unsigned long long int) node->task_time[p][k][t].num,
                    (unsigned long long int) node->task_time[p][k][t].sum, ((double) node->task_time[p][k][t].sum / (double) node->task_time[p][k][t].num));
        table[0]->task_time_list[key] = node->next;
        free (node);
        node = table[0]->task_time_list[key];
      }
      while (node != NULL);
  fflush(file);
  fclose(file);
}

#endif