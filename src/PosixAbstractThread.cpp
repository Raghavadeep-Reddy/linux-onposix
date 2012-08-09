/*
 * PosixAbstractThread.cpp
 *
 * Copyright (C) 2012 Evidence Srl - www.evidence.eu.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "PosixAbstractThread.hpp"
#include <unistd.h>
#include <csignal>
#include <strings.h>
#include "Logger.hpp"

namespace onposix {

/**
 * \brief The static function representing the code executed in the thread context.
 * This function just calls the run() function of the subclass.
 * We need this level of indirection because pthread_create() cannot accept
 * a method which is non static or virtual.
 * @param param The pointer to the concrete subclass.
 * @return Always zero.
 */
void *PosixAbstractThread::Execute(void* param)
{
	PosixAbstractThread* th = reinterpret_cast<PosixAbstractThread*>(param);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	th->run();
	return 0;
}

/**
 * \brief Constructor. Initialize the class attributes.
 */
PosixAbstractThread::PosixAbstractThread():
				isStarted_(false)
{
}

/**
 * \brief Starts execution of the thread by calling run(). If the thread is
 * already started this function does nothing.
 * @return true if the thread is started or is already started, false if
 * an error occurs.
 */
bool PosixAbstractThread::start()
{
	if (isStarted_)
		return true;

	if (pthread_create(&handle_, NULL, PosixAbstractThread::Execute,
					   (void*)this) == 0)
		isStarted_ = true;

	return isStarted_;
}

/**
 * \brief Stops the running thread.
 * @return true on success, false if an error occurs or the thread
 * has not been started before.
 */
bool PosixAbstractThread::stop()
{
	if (!isStarted_)
		return false;

	isStarted_ = false;
	if (pthread_cancel(handle_) == 0)
		return true;
	return false;
}

/**
 * \brief Blocks the calling thread until the thread associated with
 * the PosixAbstractThread object has finished execution
 * @return true on success, false if an error occurs.
 */
bool PosixAbstractThread::waitForTermination()
{
	if (pthread_join(handle_, NULL) == 0)
		return true;
	return false;
}

/**
 * \brief Masks a specific signal on this thread
 * This method allows to block a specific signal
 * The list of signals is available on /usr/include/bits/signum.h
 * @param sig the signal to be blocked
 * @return true on success; false if some error occurred
 */
bool PosixAbstractThread::blockSignal (int sig)
{
	sigset_t m;
	sigemptyset(&m);
	sigaddset(&m, sig);
	if (pthread_sigmask(SIG_BLOCK, &m, NULL) != 0) {
		DEBUG(DBG_ERROR, "Error: can't mask signal " << sig);
		return false;
	}
	return true;
}

/**
 * \brief Unmasks a signal previously masked
 * This method allows to unblock a specific signal previously blocked through
 * blockSignal().
 * The list of signals is available on /usr/include/bits/signum.h
 * @param sig the signal to be unblocked
 * @return true on success; false if some error occurred
 */
bool PosixAbstractThread::unblockSignal (int sig)
{
	sigset_t m;
	sigemptyset(&m);
	sigaddset(&m, sig);
	if (pthread_sigmask(SIG_UNBLOCK, &m, NULL) != 0) {
		DEBUG(DBG_ERROR, "Error: can't unmask signal " << sig);
		return false;
	}
	return true;
}

/**
 * \brief Sends a signal to the thread
 * This method allows to send a signal from one thread to another thread
 * The list of signals is available on /usr/include/bits/signum.h
 * @param sig the signal to be sent
 * @return true on success; false if some error occurred
 */
bool PosixAbstractThread::sendSignal(int sig)
{
	if (pthread_kill(handle_, sig) != 0){
		DEBUG(DBG_ERROR, "Error: can't send signal " << sig);
		return false;
	}
	return true;
}

/**
 * \brief Set a handler for a specific signal
 * This method allows to manually set a handler for handling a specific signal.
 * The list of signals is available on /usr/include/bits/signum.h
 * Use signals less as possible, mainly for standard situations.
 * During the execution of the handler other signals may arrive. This can lead
 * to inconsistent states. The handler must be short.
 * It must just update the internal state and/or kill the application.
 * Not all library functions can be called inside the handler without having
 * strange behaviors (see man signal). In particular, it's not safe calling
 * functions of the standard library, like printf() or exit(), or other
 * functions defined inside the application itself. The access to global
 * variables is not safe either, unless they have been defined as volatile.
 * @param sig the signal to be sent
 * @return true on success; false if some error occurred
 */
bool PosixAbstractThread::setSignalHandler(int sig, void (*handler) (int))
{
	bool ret = true;
	sigset_t oldset, set;
	struct sigaction sa;
	/* mask all signals until the handlers are installed */
	sigfillset(&set);
	sigprocmask(SIG_SETMASK, &set, &oldset);
	bzero( &sa, sizeof(sa) );
	sa.sa_handler = handler;
	if (sigaction(sig, &sa, NULL) < 0) {
		DEBUG(DBG_ERROR, "Error: can't set signal " << sig);
		ret = false;
	}
	/* remove the mask */
	sigprocmask(SIG_SETMASK, &oldset,NULL);
	return ret;
}

} /* onposix */
