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

#ifndef KERNEL_SPINLOCK_H
#define KERNEL_SPINLOCK_H

#include <Lock.h>
#include <Atomic.h>

class CAPABILITY("mutex") Spinlock : public Lock
{
    friend class PerProcessorScheduler;
    friend class LocksCommand;
  public:
    inline Spinlock(bool bLocked = false, bool bAvoidTracking = false)
        : m_bInterrupts(), m_Atom(!bLocked), m_Ra(0),
        m_bAvoidTracking(bAvoidTracking), m_Magic(0xdeadbaba),
        m_pOwner(0), m_bOwned(false), m_Level(0) {}

    bool acquire() ACQUIRE();

    /** Exit the critical section, without restoring interrupts. */
    void exit() RELEASE();

    /** Exit the critical section, restoring previous interrupt state. */
    void release() RELEASE();

    bool acquired()
    {
        return !m_Atom;
    }

    bool interrupts() const
    {
        return m_bInterrupts;
    }

  private:
    /** Unwind the spinlock because a thread is releasing it. */
    void unwind();

    volatile bool m_bInterrupts;
    Atomic<bool> m_Atom;

    uintptr_t m_Ra;
    bool m_bAvoidTracking;
    uint32_t m_Magic;

    void *m_pOwner;
    bool m_bOwned;
    size_t m_Level;
};

#endif
