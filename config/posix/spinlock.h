/* This is the default PTHREADS implementation of a spinlock synchronization
   mechanism for libgomp.  This type is private to the library.  */

#ifndef GOMP_SPINLOCK_H
#define GOMP_SPINLOCK_H 1

#include <pthread.h>

typedef pthread_spinlock_t gomp_spinlock_t;

static inline int gomp_spin_init(gomp_spinlock_t *lock, int gshared)
{
	/* On SUCCESS, this function shall return zero, otherwise
	   an error number shall be returned: EBUSY, EINVAL, EAGAIN
	   and ENOMEM.  */
	if (gshared)
		return pthread_spin_init(lock, PTHREAD_PROCESS_SHARED);
	else
		return pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE);
}

static inline int gomp_spin_destroy(gomp_spinlock_t *lock)
{
	/* On SUCCESS, this function shall return zero, otherwise
	   an error number shall be returned: EBUSY, EINVAL, EAGAIN
	   and ENOMEM.  */
	return pthread_spin_destroy(lock);
}

static inline int gomp_spin_lock(gomp_spinlock_t *lock)
{
	/* On SUCCESS, this function shall return zero, otherwise
	   an error number shall be returned: EINVAL, EDEADLOCK
	   and EBUSY.  */
	return pthread_spin_lock(lock);
}

static inline int gomp_spin_trylock(gomp_spinlock_t *lock)
{
	/* On SUCCESS, this function shall return zero, otherwise
	   an error number shall be returned: EINVAL, EDEADLOCK
	   and EBUSY.  */
	return pthread_spin_trylock(lock);
}

static inline int gomp_spin_unlock(gomp_spinlock_t *lock)
{
	/* On SUCCESS, this function shall return zero, otherwise
	   an error number shall be returned: EINVAL, EDEADLOCK
	   and EBUSY.  */
	return pthread_spin_unlock(lock);
}

#endif /* GOMP_SPINLOCK_H */