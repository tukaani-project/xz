///////////////////////////////////////////////////////////////////////////////
//
/// \file       mythread.h
/// \brief      Some threading related helper macros and functions
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef MYTHREAD_H
#define MYTHREAD_H

#include "sysdefs.h"


#ifdef HAVE_PTHREAD

////////////////////
// Using pthreads //
////////////////////

#include <sys/time.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>


#ifdef __VMS
// Do nothing on OpenVMS. It doesn't have pthread_sigmask().
#define mythread_sigmask(how, set, oset) do { } while (0)
#else
/// \brief      Set the process signal mask
///
/// If threads are disabled, sigprocmask() is used instead
/// of pthread_sigmask().
#define mythread_sigmask(how, set, oset) \
	pthread_sigmask(how, set, oset)
#endif

/// \brief      Call the given function once
///
/// If threads are disabled, a thread-unsafe version is used.
#define mythread_once(func) \
do { \
	static pthread_once_t once_ = PTHREAD_ONCE_INIT; \
	pthread_once(&once_, &func); \
} while (0)


/// \brief      Lock a mutex for a duration of a block
///
/// Perform pthread_mutex_lock(&mutex) in the beginning of a block
/// and pthread_mutex_unlock(&mutex) at the end of the block. "break"
/// may be used to unlock the mutex and jump out of the block.
/// mythread_sync blocks may be nested.
///
/// Example:
///
///     mythread_sync(mutex) {
///         foo();
///         if (some_error)
///             break; // Skips bar()
///         bar();
///     }
///
/// At least GCC optimizes the loops completely away so it doesn't slow
/// things down at all compared to plain pthread_mutex_lock(&mutex)
/// and pthread_mutex_unlock(&mutex) calls.
///
#define mythread_sync(mutex) mythread_sync_helper(mutex, __LINE__)
#define mythread_sync_helper(mutex, line) \
	for (unsigned int mythread_i_ ## line = 0; \
			mythread_i_ ## line \
				? (pthread_mutex_unlock(&(mutex)), 0) \
				: (pthread_mutex_lock(&(mutex)), 1); \
			mythread_i_ ## line = 1) \
		for (unsigned int mythread_j_ ## line = 0; \
				!mythread_j_ ## line; \
				mythread_j_ ## line = 1)


typedef struct {
	/// Condition variable
	pthread_cond_t cond;

#ifdef HAVE_CLOCK_GETTIME
	/// Clock ID (CLOCK_REALTIME or CLOCK_MONOTONIC) associated with
	/// the condition variable
	clockid_t clk_id;
#endif

} mythread_cond;


/// \brief      Initialize a condition variable to use CLOCK_MONOTONIC
///
/// Using CLOCK_MONOTONIC instead of the default CLOCK_REALTIME makes the
/// timeout in pthread_cond_timedwait() work correctly also if system time
/// is suddenly changed. Unfortunately CLOCK_MONOTONIC isn't available
/// everywhere while the default CLOCK_REALTIME is, so the default is
/// used if CLOCK_MONOTONIC isn't available.
static inline int
mythread_cond_init(mythread_cond *mycond)
{
#ifdef HAVE_CLOCK_GETTIME
	// NOTE: HAVE_DECL_CLOCK_MONOTONIC is always defined to 0 or 1.
#	if defined(HAVE_PTHREAD_CONDATTR_SETCLOCK) && HAVE_DECL_CLOCK_MONOTONIC
	struct timespec ts;
	pthread_condattr_t condattr;

	// POSIX doesn't seem to *require* that pthread_condattr_setclock()
	// will fail if given an unsupported clock ID. Test that
	// CLOCK_MONOTONIC really is supported using clock_gettime().
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0
			&& pthread_condattr_init(&condattr) == 0) {
		int ret = pthread_condattr_setclock(
				&condattr, CLOCK_MONOTONIC);
		if (ret == 0)
			ret = pthread_cond_init(&mycond->cond, &condattr);

		pthread_condattr_destroy(&condattr);

		if (ret == 0) {
			mycond->clk_id = CLOCK_MONOTONIC;
			return 0;
		}
	}

	// If anything above fails, fall back to the default CLOCK_REALTIME.
#	endif

	mycond->clk_id = CLOCK_REALTIME;
#endif

	return pthread_cond_init(&mycond->cond, NULL);
}


/// \brief      Convert relative time to absolute time for use with timed wait
///
/// The current time of the clock associated with the condition variable
/// is added to the relative time in *ts.
static inline void
mythread_cond_abstime(const mythread_cond *mycond, struct timespec *ts)
{
#ifdef HAVE_CLOCK_GETTIME
	struct timespec now;
	clock_gettime(mycond->clk_id, &now);

	ts->tv_sec += now.tv_sec;
	ts->tv_nsec += now.tv_nsec;
#else
	(void)mycond;

	struct timeval now;
	gettimeofday(&now, NULL);

	ts->tv_sec += now.tv_sec;
	ts->tv_nsec += now.tv_usec * 1000L;
#endif

	// tv_nsec must stay in the range [0, 999_999_999].
	if (ts->tv_nsec >= 1000000000L) {
		ts->tv_nsec -= 1000000000L;
		++ts->tv_sec;
	}

	return;
}


#define mythread_cond_wait(mycondptr, mutexptr) \
	pthread_cond_wait(&(mycondptr)->cond, mutexptr)

#define mythread_cond_timedwait(mycondptr, mutexptr, abstimeptr) \
	pthread_cond_timedwait(&(mycondptr)->cond, mutexptr, abstimeptr)

#define mythread_cond_signal(mycondptr) \
	pthread_cond_signal(&(mycondptr)->cond)

#define mythread_cond_broadcast(mycondptr) \
	pthread_cond_broadcast(&(mycondptr)->cond)

#define mythread_cond_destroy(mycondptr) \
	pthread_cond_destroy(&(mycondptr)->cond)


/// \brief      Create a thread with all signals blocked
static inline int
mythread_create(pthread_t *thread, void *(*func)(void *arg), void *arg)
{
	sigset_t old;
	sigset_t all;
	sigfillset(&all);

	pthread_sigmask(SIG_SETMASK, &all, &old);
	const int ret = pthread_create(thread, NULL, func, arg);
	pthread_sigmask(SIG_SETMASK, &old, NULL);

	return ret;
}

#else

//////////////////
// No threading //
//////////////////

#define mythread_sigmask(how, set, oset) \
	sigprocmask(how, set, oset)


#define mythread_once(func) \
do { \
	static bool once_ = false; \
	if (!once_) { \
		func(); \
		once_ = true; \
	} \
} while (0)

#endif

#endif
