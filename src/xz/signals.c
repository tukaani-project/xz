///////////////////////////////////////////////////////////////////////////////
//
/// \file       signals.c
/// \brief      Handling signals to abort operation
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"


volatile sig_atomic_t user_abort = false;


#if !(defined(_WIN32) && !defined(__CYGWIN__))

/// If we were interrupted by a signal, we store the signal number so that
/// we can raise that signal to kill the program when all cleanups have
/// been done.
static volatile sig_atomic_t exit_signal = 0;

/// Mask of signals for which we have established a signal handler to set
/// user_abort to true.
static sigset_t hooked_signals;

/// True once signals_init() has finished. This is used to skip blocking
/// signals (with uninitialized hooked_signals) if signals_block() and
/// signals_unblock() are called before signals_init() has been called.
static bool signals_are_initialized = false;

/// signals_block() and signals_unblock() can be called recursively.
static size_t signals_block_count = 0;


static void
signal_handler(int sig)
{
	exit_signal = sig;
	user_abort = true;

#ifndef TUKLIB_DOSLIKE
	io_write_to_user_abort_pipe();
#endif

	return;
}


extern void
signals_init(void)
{
	// List of signals for which we establish the signal handler.
	static const int sigs[] = {
		SIGINT,
		SIGTERM,
#ifdef SIGHUP
		SIGHUP,
#endif
#ifdef SIGPIPE
		SIGPIPE,
#endif
#ifdef SIGXCPU
		SIGXCPU,
#endif
#ifdef SIGXFSZ
		SIGXFSZ,
#endif
	};

	// Mask of the signals for which we have established a signal handler.
	sigemptyset(&hooked_signals);
	for (size_t i = 0; i < ARRAY_SIZE(sigs); ++i)
		sigaddset(&hooked_signals, sigs[i]);

#ifdef SIGALRM
	// Add also the signals from message.c to hooked_signals.
	for (size_t i = 0; message_progress_sigs[i] != 0; ++i)
		sigaddset(&hooked_signals, message_progress_sigs[i]);
#endif

	// Using "my_sa" because "sa" may conflict with a sockaddr variable
	// from system headers on Solaris.
	struct sigaction my_sa;

	// All the signals that we handle we also blocked while the signal
	// handler runs.
	my_sa.sa_mask = hooked_signals;

	// Don't set SA_RESTART, because we want EINTR so that we can check
	// for user_abort and cleanup before exiting. We block the signals
	// for which we have established a handler when we don't want EINTR.
	my_sa.sa_flags = 0;
	my_sa.sa_handler = &signal_handler;

	for (size_t i = 0; i < ARRAY_SIZE(sigs); ++i) {
		// If the parent process has left some signals ignored,
		// we don't unignore them.
		struct sigaction old;
		if (sigaction(sigs[i], NULL, &old) == 0
				&& old.sa_handler == SIG_IGN)
			continue;

		// Establish the signal handler.
		if (sigaction(sigs[i], &my_sa, NULL))
			message_signal_handler();
	}

	signals_are_initialized = true;

	return;
}


#ifndef __VMS
extern void
signals_block(void)
{
	if (signals_are_initialized) {
		if (signals_block_count++ == 0) {
			const int saved_errno = errno;
			mythread_sigmask(SIG_BLOCK, &hooked_signals, NULL);
			errno = saved_errno;
		}
	}

	return;
}


extern void
signals_unblock(void)
{
	if (signals_are_initialized) {
		assert(signals_block_count > 0);

		if (--signals_block_count == 0) {
			const int saved_errno = errno;
			mythread_sigmask(SIG_UNBLOCK, &hooked_signals, NULL);
			errno = saved_errno;
		}
	}

	return;
}
#endif


extern void
signals_exit(void)
{
	const int sig = (int)exit_signal;

	if (sig != 0) {
#if defined(TUKLIB_DOSLIKE) || defined(__VMS)
		// Don't raise(), set only exit status. This avoids
		// printing unwanted message about SIGINT when the user
		// presses C-c.
		set_exit_status(E_ERROR);
#else
		struct sigaction sa;
		sa.sa_handler = SIG_DFL;
		sigfillset(&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(sig, &sa, NULL);
		raise(sig);
#endif
	}

	return;
}

#else

// While Windows has some very basic signal handling functions as required
// by C89, they are not really used, and e.g. SIGINT doesn't work exactly
// the way it does on POSIX (Windows creates a new thread for the signal
// handler). Instead, we use SetConsoleCtrlHandler() to catch user
// pressing C-c, because that seems to be the recommended way to do it.
//
// NOTE: This doesn't work under MSYS. Trying with SIGINT doesn't work
// either even if it appeared to work at first. So test using Windows
// console window.

static BOOL WINAPI
signal_handler(DWORD type lzma_attribute((__unused__)))
{
	// Since we don't get a signal number which we could raise() at
	// signals_exit() like on POSIX, just set the exit status to
	// indicate an error, so that we cannot return with zero exit status.
	set_exit_status(E_ERROR);
	user_abort = true;
	return TRUE;
}


extern void
signals_init(void)
{
	if (!SetConsoleCtrlHandler(&signal_handler, TRUE))
		message_signal_handler();

	return;
}

#endif


#ifdef TERMINAL_STOP_SUPPORTED
volatile sig_atomic_t terminal_stop_requested = false;


static void
terminal_stop_signal_handler(int sig lzma_attribute((__unused__)))
{
	terminal_stop_requested = true;
	// Unblock SIGTSTP in case it was blocked.
	// Since it is being removed from the hooked_signals, it would
	// not be unblocked in the next call to signals_unblock()
	sigset_t signal_stop_set;
	sigemptyset(&signal_stop_set);
	sigaddset(&signal_stop_set, SIGTSTP);
	mythread_sigmask(SIG_UNBLOCK, &signal_stop_set, NULL);

	sigdelset(&hooked_signals, SIGTSTP);
	return;
}


extern void
reset_terminal_stop_handler(void)
{
	// Use kill() because raise() may not be portable.
	if (terminal_stop_requested) {
		if (kill(getpid(), SIGTSTP) != 0)
			message_warning(_("Cannot pause process: %s"),
					strerror(errno));
		// Reset the flag after the process continues
		terminal_stop_requested = false;
	}

	struct sigaction my_sa;
	// Use the SA_RESETHAND flag so the SIGTSTP signal is only caught
	// one time. This way, we can use the default signal handler to
	// pause the process after the timing code is paused.
	my_sa.sa_flags = (int)SA_RESETHAND;
	my_sa.sa_handler = &terminal_stop_signal_handler;
	sigemptyset(&my_sa.sa_mask);

	sigaddset(&hooked_signals, SIGTSTP);

	if (sigaction(SIGTSTP, &my_sa, NULL))
		message_signal_handler();
}
#endif
