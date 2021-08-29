/* This file provides the functions needed to handle a thread-local
   list of already allocated and at start-up time initialized free
   "gomp_task_state" structures that ara available for execution.  */

#include "libgomp.h"


static inline struct gomp_task_state *
gomp_task_state_list_alloc(size_t state_size, size_t stack_size, unsigned num)
{
	void *ptr;
	unsigned i;
	size_t full_size;
	struct gomp_task_state *list;
	struct gomp_task_state *state;
	struct gomp_task_context context;

	full_size = state_size + stack_size;

	list = gomp_malloc(full_size * num);
	ptr = (void *) list;

	for (i=0; i<num; i++)
	{
		state = (struct gomp_task_state *) ptr;
		state->switch_task = NULL;
		state->switch_from_task = NULL;
		state->stack_size = stack_size;
		state->stack = ptr + state_size;
		gomp_context_create(&context, &state->context, state->stack, state->stack_size);
		ptr = ptr + full_size;
		state->next = (i<num-1) ? ((struct gomp_task_state *)ptr) : NULL;
		state->next_to_free = NULL;
	}

	return list;
}

int
gomp_task_state_list_init(struct gomp_task_state_list_group *pool, unsigned list_idx, struct gomp_task_icv *icv)
{
	if (pool == NULL || pool->thread_list == NULL || icv == NULL)
		return 1;

	if (list_idx >= pool->thread_list_number)
		return 1;

	pool->thread_list[list_idx] = gomp_malloc (sizeof(struct gomp_task_state_list));
	pool->thread_list[list_idx]->list = gomp_task_state_list_alloc(sizeof(struct gomp_task_state), icv->ult_stack_size, NUM_INIT_STATE);
	pool->thread_list[list_idx]->list_to_free = (struct gomp_task_state *) pool->thread_list[list_idx]->list;
	pool->thread_list[list_idx]->list_size = NUM_INIT_STATE;

	__atomic_add_fetch (&pool->allocated_tasks, NUM_INIT_STATE, __ATOMIC_RELAXED);

	return 0;
}

static inline void
gomp_task_state_list_end(struct gomp_task_state_list_group *pool, unsigned list_idx)
{
	struct gomp_task_state *state;

	state = pool->thread_list[list_idx]->list_to_free;

	while (state)
	{
		pool->thread_list[list_idx]->list_to_free = state->next_to_free;
		free (state);
		state = pool->thread_list[list_idx]->list_to_free;
	}

	pool->thread_list[list_idx]->list_size = 0;
}

int
gomp_task_state_list_group_init(struct gomp_task_state_list_group *pool, unsigned nthreads, struct gomp_task_icv *icv)
{
	unsigned i;

	if (pool == NULL || nthreads == 0 || icv == NULL)
		return 1;

	if (pool->initialized)
		return 1;

	pool->allocated_tasks = 0;

	gomp_spin_init(&pool->global_list_lock, 0);
	pool->global_list.list_size = 0;
	pool->global_list.list_to_free = NULL;
	pool->global_list.list = NULL;

	pool->thread_list_number = nthreads;
	pool->thread_list = gomp_malloc (nthreads * sizeof(struct gomp_task_state_list *));
	
	for (i=0; i<nthreads; i++)
		pool->thread_list[i] = NULL;

	pool->initialized = true;

	return 0;
}

void
gomp_task_state_list_group_end(struct gomp_task_state_list_group *pool)
{
	unsigned i;

	if (pool == NULL || !pool->initialized)
		return;

	pool->initialized = false;

	for (i=0; i<pool->thread_list_number; i++)
		gomp_task_state_list_end(pool, i);

	free(pool->thread_list);
	pool->thread_list = NULL;
	pool->thread_list_number = 0;
	
	pool->global_list.list = NULL;
	pool->global_list.list_to_free = NULL;
	pool->global_list.list_size = 0;
	gomp_spin_destroy(&pool->global_list_lock);
	
	pool->allocated_tasks = 0;
}

struct gomp_task_state *
gomp_get_task_state(struct gomp_task_state_list_group *pool, struct gomp_task_state_list *local, unsigned long ult_stack_size)
{
	int count;
	struct gomp_task_state *state;
	struct gomp_task_state *state_last;

	if (local->list)
	{
		state = (struct gomp_task_state *) local->list;
		local->list = state->next;
		local->list_size -= 1;
		state->next = NULL;
		return state;
	}

	while (1)
	{
		if (pool->global_list.list_size < NUM_ADD_STATE)
			break;

		if (gomp_spin_trylock(&pool->global_list_lock))
			continue;

		if (pool->global_list.list == NULL)
		{
			gomp_spin_unlock(&pool->global_list_lock);
			break;
		}

		count = NUM_ADD_STATE;
		state = (struct gomp_task_state *) pool->global_list.list;

		do
		{
			state_last = state;
			state = state->next;
			count -= 1;
		}
		while (state && count);

		state = (struct gomp_task_state *) pool->global_list.list;
		pool->global_list.list = state_last->next;
		pool->global_list.list_size -= (NUM_ADD_STATE-count);

		gomp_spin_unlock(&pool->global_list_lock);

		state_last->next = NULL;

		local->list = state->next;
		local->list_size += (NUM_ADD_STATE-count-1);
		state->next = NULL;

		return state;
	}
	
	local->list = gomp_task_state_list_alloc(sizeof(struct gomp_task_state), ult_stack_size, NUM_ADD_STATE);
	local->list->next_to_free = local->list_to_free;
	local->list_to_free = (struct gomp_task_state *) local->list;

	state = (struct gomp_task_state *) local->list;
	local->list = state->next;
	local->list_size = NUM_ADD_STATE-1;
	state->next = NULL;

	__atomic_add_fetch (&pool->allocated_tasks, NUM_ADD_STATE, __ATOMIC_RELAXED);

	return state;
}

void
gomp_free_task_state(struct gomp_task_state_list_group *pool, struct gomp_task_state_list *local, struct gomp_task_state *state)
{
	int count;
	unsigned level;
	unsigned surplus;
	unsigned threshold;
	unsigned picked;
	struct gomp_task_state *state_last;

	state->switch_task = NULL;
	state->switch_from_task = NULL;
	state->next = (struct gomp_task_state *) local->list;
	local->list = state;
	local->list_size += 1;

	picked = 0;
	state_last = NULL;

	while (1)
	{
		level = pool->allocated_tasks / pool->thread_list_number;
		surplus = level / pool->thread_list_number;

		if (surplus == 0)
			return;

		threshold = level + surplus;

		if (local->list_size <= threshold || pool->global_list.list_size >= level)
			return;

		count = surplus;

		if (picked == 0)
		{
			state = (struct gomp_task_state *) local->list;

			do
			{
				state_last = state;
				state = state->next;
				count -= 1;
			}
			while (state && count);

			state = (struct gomp_task_state *) local->list;
			picked = surplus - count;
		}

		if (gomp_spin_trylock(&pool->global_list_lock))
			continue;

		if (pool->global_list.list_size >= level)
		{
			gomp_spin_unlock(&pool->global_list_lock);
			return;
		}

		local->list = state_last->next;
		local->list_size -= picked;
		state_last->next = (struct gomp_task_state *) pool->global_list.list;
		pool->global_list.list = state;
		pool->global_list.list_size += picked;

		gomp_spin_unlock(&pool->global_list_lock);

		return;
	}
}