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

#define PEDIGREE_EXTERNAL_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>
#include <pthread.h>

#include <time/Time.h>
#include <Spinlock.h>
#include <process/Mutex.h>
#include <process/ConditionVariable.h>
#include <utilities/Cache.h>

void *g_pBootstrapInfo = 0;

namespace Time
{

Timestamp getTime(bool sync)
{
    return time(NULL);
}

}  // Time

extern "C"
void panic(const char *s)
{
    fprintf(stderr, "PANIC: %s\n", s);
    abort();
}

namespace SlamSupport
{
static const size_t heapSize = 0x40000000ULL;
uintptr_t getHeapBase()
{
    static void *base = 0;
    if (base)
    {
        return reinterpret_cast<uintptr_t>(base);
    }

    base = mmap(0, heapSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED)
    {
        fprintf(stderr, "cannot get a region of memory for SlamAllocator: %s\n", strerror(errno));
        abort();
    }

    return reinterpret_cast<uintptr_t>(base);
}

uintptr_t getHeapEnd()
{
    return getHeapBase() + heapSize;
}

void getPageAt(void *addr)
{
    void *r = mmap(addr, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE |
        MAP_FIXED | MAP_ANONYMOUS, -1, 0);
    if (r == MAP_FAILED)
    {
        fprintf(stderr, "map failed: %s\n", strerror(errno));
        abort();
    }
}

void unmapPage(void *page)
{
    munmap(page, 0x1000);
}

void unmapAll()
{
    munmap((void *) getHeapBase(), heapSize);
}
}  // namespace SlamSupport

/** Spinlock implementation. */

Spinlock::Spinlock(bool bLocked, bool bAvoidTracking) :
    m_bInterrupts(), m_Atom(!bLocked), m_CpuState(0), m_Ra(0),
    m_bAvoidTracking(bAvoidTracking), m_Magic(0xdeadbaba),
    m_pOwner(0), m_bOwned(false), m_Level(0), m_OwnedProcessor(~0)
{
}

bool Spinlock::acquire(bool recurse, bool safe)
{
    while (!m_Atom.compareAndSwap(true, false))
        ;

    return true;
}

void Spinlock::release()
{
    exit();
}

void Spinlock::exit()
{
    m_Atom.compareAndSwap(false, true);
}

/** ConditionVariable implementation. */

ConditionVariable::ConditionVariable() :
    m_Lock(false), m_Waiters(), m_Private(0)
{
    pthread_cond_t *cond = new pthread_cond_t;
    *cond = PTHREAD_COND_INITIALIZER;

    pthread_cond_init(cond, 0);

    m_Private = reinterpret_cast<void *>(cond);
}

ConditionVariable::~ConditionVariable()
{
    pthread_cond_t *cond = reinterpret_cast<pthread_cond_t *>(m_Private);
    pthread_cond_destroy(cond);

    delete cond;
}

bool ConditionVariable::wait(Mutex &mutex)
{
    pthread_cond_t *cond = reinterpret_cast<pthread_cond_t *>(m_Private);
    pthread_mutex_t *m = reinterpret_cast<pthread_mutex_t *>(mutex.getPrivate());

    int r = pthread_cond_wait(cond, m);
    return r == 0;
}

void ConditionVariable::signal()
{
    pthread_cond_t *cond = reinterpret_cast<pthread_cond_t *>(m_Private);
    pthread_cond_signal(cond);
}

void ConditionVariable::broadcast()
{
    pthread_cond_t *cond = reinterpret_cast<pthread_cond_t *>(m_Private);
    pthread_cond_broadcast(cond);
}

/** Mutex implementation. */

Mutex::Mutex(bool bLocked) :
    m_Private(0)
{
    pthread_mutex_t *mutex = new pthread_mutex_t;
    *mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_mutex_init(mutex, 0);

    m_Private = reinterpret_cast<void *>(mutex);
}

Mutex::~Mutex()
{
    pthread_mutex_t *mutex = reinterpret_cast<pthread_mutex_t *>(m_Private);
    pthread_mutex_destroy(mutex);

    delete mutex;
}

bool Mutex::acquire()
{
    pthread_mutex_t *mutex = reinterpret_cast<pthread_mutex_t *>(m_Private);
    return pthread_mutex_lock(mutex) == 0;
}

void Mutex::release()
{
    pthread_mutex_t *mutex = reinterpret_cast<pthread_mutex_t *>(m_Private);
    pthread_mutex_unlock(mutex);
}

/** Cache implementation. */
#ifdef STANDALONE_CACHE
void Cache::discover_range(uintptr_t &start, uintptr_t &end)
{
    static uintptr_t alloc_start = 0;
    const size_t length = 0x80000000U;

    if (alloc_start)
    {
        start = alloc_start;
        end = start + length;
        return;
    }

    void *p = mmap(0, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p != MAP_FAILED)
    {
        alloc_start = reinterpret_cast<uintptr_t>(p);

        start = alloc_start;
        end = start + length;
    }
}
#endif
