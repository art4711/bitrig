/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $OpenBSD: uthread_suspend_np.c,v 1.3 1999/05/26 00:18:26 d Exp $
 */
#include <errno.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/* Suspend a thread: */
int
pthread_suspend_np(pthread_t thread)
{
	int ret;

	/* Find the thread in the list of active threads: */
	if ((ret = _find_thread(thread)) == 0) {
		/* The thread exists. Is it running? */
		if (thread->state != PS_RUNNING &&
		    thread->state != PS_SUSPENDED) {
			/* The thread operation has been interrupted */
			_thread_seterrno(thread,EINTR);
			thread->interrupted = 1;
		}

		/*
		 * Guard against preemption by a scheduling signal.
		 * A change of thread state modifies the waiting
		 * and priority queues.
		 */
		_thread_kern_sched_defer();

		/* Suspend the thread. */
		PTHREAD_NEW_STATE(thread,PS_SUSPENDED);

		/*
		 * Reenable preemption and yield if a scheduling signal
		 * occurred while in the critical region.
		 */
		_thread_kern_sched_undefer();
	}
	return(ret);
}
#endif
