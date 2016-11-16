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

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <Atomic.h>
#include <processor/types.h>
#include <process/Event.h>
#include <process/eventNumbers.h>
#include <Spinlock.h>
#include <utilities/List.h>

/**
 * A counting semaphore.
 */
class Semaphore
{
public:
    /** Constructor
     * \param nInitialValue The initial value of the semaphore.
     * \param canInterrupt If false, acquire() retries after interrupt rather
     *      than returning a failure status.*/
    Semaphore(size_t nInitialValue, bool canInterrupt = true);
    /** Destructor */
    virtual ~Semaphore();

    /** Attempts to acquire n items from the semaphore. This will block until the semaphore
     *  is non-zero.
     * \param n The number of semaphore items required. Must be non-zero.
     * \param timeoutSecs Timeout value in seconds - if zero, no timeout.
     * \return True if acquire succeeded, false otherwise (timeout). */
    bool acquire(size_t n=1, size_t timeoutSecs=0, size_t timeoutUsecs=0);

    /** Attempts to acquire n items from the semaphore. This will not block.
     * \param n The number of semaphore items required. Must be non-zero.
     * \return True if acquire succeeded, false otherwise. */
    bool tryAcquire(size_t n=1);

    /** Releases n items from the semaphore.
     * \param n The number of semaphore items to release. Must be non-zero. */
    void release(size_t n=1);

    /** Gets the current value of the semaphore */
    ssize_t getValue();

private:
    /** Private copy constructor
        \note NOT implemented. */
    Semaphore(const Semaphore&);
    /** Private operator=
        \note NOT implemented. */
    void operator =(const Semaphore&);

    /** Removes the given pointer from the thread queue. */
    void removeThread(class Thread *pThread);

    /** Internal event class - just interrupts the calling thread
        (sets wasInterrupted and sets the thread status to Ready). */
    class SemaphoreEvent : public Event
    {
    public:
        SemaphoreEvent();
        virtual ~SemaphoreEvent()
        {}

        virtual size_t serialize(uint8_t *pBuffer)
        {return 0;}
        static bool unserialize(uint8_t *pBuffer, SemaphoreEvent &event)
        {return true;}

        virtual size_t getNumber()
        {return EventNumbers::Interrupt;}
    };

    size_t magic;

    Atomic<ssize_t> m_Counter;
    Spinlock m_BeingModified;
    List<class Thread*> m_Queue;
    bool m_bCanInterrupt;
};

#endif
