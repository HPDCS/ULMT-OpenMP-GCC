#define _GNU_SOURCE

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdio.h>
#include <sched.h>
#include <pthread.h>

#include "libgomp.h"


#if defined HAVE_TLS || defined USE_EMUTLS

#define IBS_ENABLE                  (1U << 2)
#define IBS_DISABLE                 (1U << 3)
#define IBS_REGISTER_THREAD         (1U << 4)
#define IBS_UNREGISTER_THREAD       (1U << 5)
#define IBS_REGISTER_SAFE_MEM_ADDR  (1U << 6)
#define IBS_REGISTER_SAFE_MEM_SIZE  (1U << 7)
#define IBS_SET_TEXT_START          (1U << 8)
#define IBS_SET_TEXT_END            (1U << 9)

#define IPI_REGISTER_THREAD         (1U << 2)
#define IPI_UNREGISTER_THREAD       (1U << 3)
#define IPI_REGISTER_SAFE_MEM_ADDR  (1U << 4)
#define IPI_REGISTER_SAFE_MEM_SIZE  (1U << 5)
#define IPI_SET_TEXT_START          (1U << 6)
#define IPI_SET_TEXT_END            (1U << 7)


static __thread cpu_set_t oldset;

static int
alloc_alternate_stack_area(void ** stack, unsigned long * stack_size)
{
    int res = 1;

    (*stack_size) = gomp_icv (false)->ult_stack_size;

    if (((*stack) = mmap(NULL, (size_t) (*stack_size), PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_STACK, 0, 0)) != MAP_FAILED)
    {
        memset((*stack), 0, (size_t) (*stack_size));

        if (mlock((const void *) (*stack), (size_t) (*stack_size)) == 0)
        	res = 0;
        else
            munmap((*stack), (size_t) (*stack_size));
    }

    return res;
}

static void
free_alternate_stack_area(void ** stack, unsigned long * stack_size)
{
    munlock((const void *) (*stack), (size_t) (*stack_size));
    munmap((*stack), (size_t) (*stack_size));
    (*stack) = NULL;
    (*stack_size) = 0UL;
}

static int
pin_thread_to_core(int * core_id)
{
	cpu_set_t cpuset;

	pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &oldset);

	CPU_ZERO(&cpuset);
	CPU_SET((*core_id), &cpuset);

	if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
	{
		(*core_id) = -1;
		return 1;
	}

	while (sched_getcpu() != (*core_id)) ;

	return 0;
}

static void
remove_thread_pinning(int * core_id)
{
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &oldset);
    (*core_id) = -1;
}

#endif

int
gomp_thread_interrupt_registration (void)
{
#if defined HAVE_TLS || defined USE_EMUTLS
	char path[32];
	struct gomp_thread *thr = gomp_thread ();
	struct gomp_task_icv *icv;

	if (thr->task == NULL || thr->ts.team == NULL)
	{
		printf("Thread has no task or it does not belong to any team.\n");
		return 1;
	}

	icv = &thr->task->icv;

	if (icv->text_section_start == 0x0 || icv->text_section_end == 0x0)
	{
		printf("Invalid value in text_start and/or text_end field.\n");
		return 1;
	}

	thr->ts.core_id = thr->ts.team_id % omp_get_num_procs();

	if (pin_thread_to_core(&thr->ts.core_id))
	{
		printf("Unable to pin thread %u to core %d.\n", thr->ts.team_id, thr->ts.core_id);
		return 1;
	}

	if (alloc_alternate_stack_area(&thr->ts.alt_stack, &thr->ts.alt_stack_size))
	{
		printf("Unable to allocate a memory-locked area to accommodate the alternate-stack for thread %u.\n", thr->ts.team_id);
		remove_thread_pinning(&thr->ts.core_id);
		return 1;
	}

	if (gomp_ibs_rate_var > 0)
	{
		snprintf(path, 32, "/dev/cpu/%d/ibs/op", thr->ts.core_id);

		if ((thr->ts.ibs_fd = open(path, (O_RDONLY | O_NONBLOCK))) < 0)
		{
			printf("Unable to open IBS device on core %d.\n", thr->ts.core_id);
			free_alternate_stack_area(&thr->ts.alt_stack, &thr->ts.alt_stack_size);
			remove_thread_pinning(&thr->ts.core_id);
			return 1;
		}

		if (ioctl(thr->ts.ibs_fd, IBS_REGISTER_THREAD, gomp_interrupt_trampoline) < 0)
		{
			printf("Unable to register thread %u for IBS support on core %d.\n", thr->ts.team_id, thr->ts.core_id);
			close(thr->ts.ibs_fd);
			free_alternate_stack_area(&thr->ts.alt_stack, &thr->ts.alt_stack_size);
			remove_thread_pinning(&thr->ts.core_id);
			return 1;
		}

		if (ioctl(thr->ts.ibs_fd, IBS_REGISTER_SAFE_MEM_ADDR, (unsigned long) thr->ts.alt_stack) < 0)
	    {
			printf("Unable to register alternate-stack memory area for thread %u.\n", thr->ts.team_id);
			ioctl(thr->ts.ibs_fd, IBS_UNREGISTER_THREAD);
			close(thr->ts.ibs_fd);
			free_alternate_stack_area(&thr->ts.alt_stack, &thr->ts.alt_stack_size);
			remove_thread_pinning(&thr->ts.core_id);
			return 1;
	    }

	    if (ioctl(thr->ts.ibs_fd, IBS_REGISTER_SAFE_MEM_SIZE, thr->ts.alt_stack_size) < 0)
	    {
			printf("Unable to register size of alternate-stack memory area for thread %u.\n", thr->ts.team_id);
			ioctl(thr->ts.ibs_fd, IBS_UNREGISTER_THREAD);
			close(thr->ts.ibs_fd);
			free_alternate_stack_area(&thr->ts.alt_stack, &thr->ts.alt_stack_size);
			remove_thread_pinning(&thr->ts.core_id);
			return 1;
	    }

		if (ioctl(thr->ts.ibs_fd, IBS_SET_TEXT_START, (unsigned long) icv->text_section_start) < 0)
		{
			printf("Unable to set text_start address for thread %u.\n", thr->ts.team_id);
			ioctl(thr->ts.ibs_fd, IBS_UNREGISTER_THREAD);
			close(thr->ts.ibs_fd);
			free_alternate_stack_area(&thr->ts.alt_stack, &thr->ts.alt_stack_size);
			remove_thread_pinning(&thr->ts.core_id);
			return 1;
		}

		if (ioctl(thr->ts.ibs_fd, IBS_SET_TEXT_END, (unsigned long) icv->text_section_end) < 0)
		{
			printf("Unable to set text_end address for thread %u.\n", thr->ts.team_id);
			ioctl(thr->ts.ibs_fd, IBS_UNREGISTER_THREAD);
			close(thr->ts.ibs_fd);
			free_alternate_stack_area(&thr->ts.alt_stack, &thr->ts.alt_stack_size);
			remove_thread_pinning(&thr->ts.core_id);
			return 1;
		}

		if (ioctl(thr->ts.ibs_fd, IBS_ENABLE, gomp_ibs_rate_var) < 0)
		{
			printf("Unable to enable IBS support on core %d.\n", thr->ts.core_id);
			ioctl(thr->ts.ibs_fd, IBS_UNREGISTER_THREAD);
			close(thr->ts.ibs_fd);
			free_alternate_stack_area(&thr->ts.alt_stack, &thr->ts.alt_stack_size);
			remove_thread_pinning(&thr->ts.core_id);
			return 1;
		}
	}

	if (gomp_ipi_var)
	{
		if ((thr->ts.ipi_fd = open("/dev/ipi", (O_RDONLY | O_NONBLOCK))) < 0)
	    {
	        printf("Unable to open IPI device. Thread will work with no IPI support.\n");
	        free_alternate_stack_area(&thr->ts.alt_stack, &thr->ts.alt_stack_size);
	        remove_thread_pinning(&thr->ts.core_id);
	        return 1;
	    }

	    if (ioctl(thr->ts.ipi_fd, IPI_REGISTER_THREAD, gomp_interrupt_trampoline) < 0)
	    {
	        printf("Unable to register thread for IPI on core %d. Thread will work with no IPI support.\n", thr->ts.core_id);
	        close(thr->ts.ipi_fd);
	        free_alternate_stack_area(&thr->ts.alt_stack, &thr->ts.alt_stack_size);
	        remove_thread_pinning(&thr->ts.core_id);
	        return 1;
	    }

	    if (ioctl(thr->ts.ipi_fd, IPI_REGISTER_SAFE_MEM_ADDR, (unsigned long) thr->ts.alt_stack) < 0)
	    {
			printf("Unable to register alternate-stack memory area for thread %u.\n", thr->ts.team_id);
			ioctl(thr->ts.ipi_fd, IPI_UNREGISTER_THREAD);
			close(thr->ts.ipi_fd);
			free_alternate_stack_area(&thr->ts.alt_stack, &thr->ts.alt_stack_size);
			remove_thread_pinning(&thr->ts.core_id);
			return 1;
	    }

	    if (ioctl(thr->ts.ipi_fd, IPI_REGISTER_SAFE_MEM_SIZE, thr->ts.alt_stack_size) < 0)
	    {
			printf("Unable to register size of alternate-stack memory area for thread %u.\n", thr->ts.team_id);
			ioctl(thr->ts.ipi_fd, IPI_UNREGISTER_THREAD);
			close(thr->ts.ipi_fd);
			free_alternate_stack_area(&thr->ts.alt_stack, &thr->ts.alt_stack_size);
			remove_thread_pinning(&thr->ts.core_id);
			return 1;
	    }

	    if (ioctl(thr->ts.ipi_fd, IPI_SET_TEXT_START, (unsigned long) icv->text_section_start) < 0)
		{
			printf("Unable to set text_start address for thread %u. Thread will work with no IPI support.\n", thr->ts.team_id);
			ioctl(thr->ts.ipi_fd, IPI_UNREGISTER_THREAD);
			close(thr->ts.ipi_fd);
			free_alternate_stack_area(&thr->ts.alt_stack, &thr->ts.alt_stack_size);
			remove_thread_pinning(&thr->ts.core_id);
			return 1;
		}

		if (ioctl(thr->ts.ipi_fd, IPI_SET_TEXT_END, (unsigned long) icv->text_section_end) < 0)
		{
			printf("Unable to set text_end address for thread %u. Thread will work with no IPI support.\n", thr->ts.team_id);
			ioctl(thr->ts.ipi_fd, IPI_UNREGISTER_THREAD);
			close(thr->ts.ipi_fd);
			free_alternate_stack_area(&thr->ts.alt_stack, &thr->ts.alt_stack_size);
			remove_thread_pinning(&thr->ts.core_id);
			return 1;
		}
	}
#endif
	return 0;
}

void
gomp_thread_interrupt_cancellation (void)
{
#if defined HAVE_TLS || defined USE_EMUTLS
	struct gomp_thread *thr = gomp_thread ();

	if (thr->task == NULL || thr->ts.team == NULL)
		return;

	if (gomp_ibs_rate_var > 0)
	{
		if (thr->ts.ibs_fd >= 0)
		{
			if (ioctl(thr->ts.ibs_fd, IBS_DISABLE) < 0)
				printf("Unable to disable IBS support on core %d.\n", thr->ts.core_id);
			if (ioctl(thr->ts.ibs_fd, IBS_UNREGISTER_THREAD) < 0)
				printf("Unable to unregister thread %u for IBS support on core %d.\n", thr->ts.team_id, thr->ts.core_id);
			close(thr->ts.ibs_fd);
			thr->ts.ibs_fd = -1;
		}
	}

	if (gomp_ipi_var)
	{
		if (thr->ts.ipi_fd >= 0)
		{
			if (ioctl(thr->ts.ipi_fd, IPI_UNREGISTER_THREAD) < 0)
		        printf("Unable to unregister thread for IPI support on core %d.\n", thr->ts.core_id);
		    close(thr->ts.ipi_fd);
		    thr->ts.ipi_fd = -1;
		}
	}

	free_alternate_stack_area(&thr->ts.alt_stack, &thr->ts.alt_stack_size);
	remove_thread_pinning(&thr->ts.core_id);
#endif
}