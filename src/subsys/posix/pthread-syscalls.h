/*
 * Copyright (c) 2008-2014, Pedigree Developers
 *
 * Please see the CONTRIB file in the root of the source tree for a full
 * list of contributors.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _PTHREAD_SYSCALLS_H
#define _PTHREAD_SYSCALLS_H

#include <processor/Processor.h>
#include <processor/VirtualAddressSpace.h>
#include <processor/state.h>

#include "PosixSubsystem.h"
#include <process/Semaphore.h>
#include <process/Thread.h>

#include "logging.h"

#include <sys/types.h>

void pedigree_init_pthreads();
void pedigree_copy_posix_thread(
    Thread *, PosixSubsystem *, Thread *, PosixSubsystem *);

/// Creates a new wait object that threads can use to synchronise.
void *posix_pedigree_create_waiter();
int posix_pedigree_thread_wait_for(void *waiter);
int posix_pedigree_thread_trigger(void *waiter);
void posix_pedigree_destroy_waiter(void *waiter);

int posix_futex(
    int *uaddr, int futex_op, int val, const struct timespec *timeout);

pid_t posix_gettid();

#endif
