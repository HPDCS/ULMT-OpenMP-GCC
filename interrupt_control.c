#define _GNU_SOURCE

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#if defined HAVE_TLS || defined USE_EMUTLS
#include <sys/syscall.h>
#include <sys/types.h>
#endif
#include <stdio.h>
#include <sched.h>
#include <pthread.h>

#include "libgomp.h"


#if defined HAVE_TLS || defined USE_EMUTLS
#define ARCH_SET_GS				0x1001
#define NR_SYS_ARCH_PRCTL		158
#endif

#define IBS_ENABLE				(1U << 2)
#define IBS_DISABLE				(1U << 3)
#define IBS_REGISTER_THREAD		(1U << 4)
#define IBS_UNREGISTER_THREAD	(1U << 5)
#define IBS_SET_TEXT_START		(1U << 6)
#define IBS_SET_TEXT_END		(1U << 7)


#if defined HAVE_TLS || defined USE_EMUTLS
static inline int
arch_prctl(int code, unsigned long addr)
{
    return syscall(NR_SYS_ARCH_PRCTL, code, addr);
}

static int
pin_thread_to_core(int core)
{
	cpu_set_t cpuset;
	unsigned long gs_thr;
	struct gomp_thread *thr = gomp_thread ();

	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);

	if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
		return 1;

	while (sched_getcpu() != core) ;

	thr->me = (void *) thr;
	gs_thr = (unsigned long) thr;
	*(unsigned long *)gs_thr = gs_thr;
	if (arch_prctl(ARCH_SET_GS, gs_thr))
		printf("Unable to set GS register on core %d.\n", core);

	return 0;
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

	if (pin_thread_to_core(thr->ts.core_id))
	{
		printf("Unable to pin thread %u to core %d.\n", thr->ts.team_id, thr->ts.core_id);
		return 1;
	}

	snprintf(path, 32, "/dev/cpu/%d/ibs/op", thr->ts.core_id);

	if ((thr->ts.ibs_fd = open(path, (O_RDONLY | O_NONBLOCK))) < 0)
	{
		printf("Unable to open IBS device on core %d.\n", thr->ts.core_id);
		return 1;
	}

	if (ioctl(thr->ts.ibs_fd, IBS_REGISTER_THREAD, gomp_interrupt_trampoline) < 0)
	{
		printf("Unable to register thread %u for IBS support on core %d.\n", thr->ts.team_id, thr->ts.core_id);
		close(thr->ts.ibs_fd);
		return 1;
	}

	if (ioctl(thr->ts.ibs_fd, IBS_SET_TEXT_START, (unsigned long) icv->text_section_start) < 0)
	{
		printf("Unable to set text_start address for thread %u.\n", thr->ts.team_id);
		ioctl(thr->ts.ibs_fd, IBS_UNREGISTER_THREAD);
		close(thr->ts.ibs_fd);
		return 1;
	}

	if (ioctl(thr->ts.ibs_fd, IBS_SET_TEXT_END, (unsigned long) icv->text_section_end) < 0)
	{
		printf("Unable to set text_end address for thread %u.\n", thr->ts.team_id);
		ioctl(thr->ts.ibs_fd, IBS_UNREGISTER_THREAD);
		close(thr->ts.ibs_fd);
		return 1;
	}

	if (ioctl(thr->ts.ibs_fd, IBS_ENABLE, gomp_ibs_rate_var) < 0)
	{
		printf("Unable to enable IBS support on core %d.\n", thr->ts.core_id);
		ioctl(thr->ts.ibs_fd, IBS_UNREGISTER_THREAD);
		close(thr->ts.ibs_fd);
		return 1;
	}
#endif
	return 0;
}

int
gomp_thread_interrupt_cancellation (void)
{
#if defined HAVE_TLS || defined USE_EMUTLS
	struct gomp_thread *thr = gomp_thread ();

	if (thr->task == NULL || thr->ts.team == NULL)
	{
		printf("Thread has no task or it does not belong to any team.\n");
		return 1;
	}

	if (thr->ts.ibs_fd < 0)
	{
		printf("IBS device on core %d is not open.\n", thr->ts.core_id);
		return 1;
	}

	if (ioctl(thr->ts.ibs_fd, IBS_DISABLE) < 0)
	{
		printf("Unable to disable IBS support on core %d.\n", thr->ts.core_id);
		ioctl(thr->ts.ibs_fd, IBS_UNREGISTER_THREAD);
		close(thr->ts.ibs_fd);
		return 1;
	}

	if (ioctl(thr->ts.ibs_fd, IBS_UNREGISTER_THREAD) < 0)
	{
		printf("Unable to unregister thread %u for IBS support on core %d.\n", thr->ts.team_id, thr->ts.core_id);
		close(thr->ts.ibs_fd);
		return 1;
	}

	close(thr->ts.ibs_fd);
#endif
	return 0;
}