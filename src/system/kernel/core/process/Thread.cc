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

#ifdef THREADS

#include <process/Thread.h>
#include <process/Scheduler.h>
#include <process/SchedulingAlgorithm.h>
#include <process/ProcessorThreadAllocator.h>
#include <processor/Processor.h>
#include <processor/StackFrame.h>
#include <machine/Machine.h>
#include <Log.h>

#include <machine/InputManager.h>

#include <processor/MemoryRegion.h>
#include <processor/PhysicalMemoryManager.h>

#include <processor/ProcessorInformation.h>

Thread::Thread(Process *pParent, ThreadStartFunc pStartFunction, void *pParam,
               void *pStack, bool semiUser, bool bDontPickCore) :
    m_nStateLevel(0), m_pParent(pParent), m_Status(Ready), m_ExitCode(0), /* m_pKernelStack(0), */ m_pAllocatedStack(0), m_Id(0),
    m_Errno(0), m_bInterrupted(false), m_Lock(), m_ConcurrencyLock(), m_EventQueue(), m_DebugState(None), m_DebugStateAddress(0),
    m_UnwindState(Continue), m_pScheduler(0), m_Priority(DEFAULT_PRIORITY),
    m_PendingRequests(), m_pTlsBase(0), m_bRemovingRequests(false), m_pWaiter(0), m_bDetached(false),
    m_bInterruptible(true)
{
  if (pParent == 0)
  {
    FATAL("Thread::Thread(): Parent process was NULL!");
  }

  // Initialise our kernel stack.
  m_pAllocatedStack = 0;

  // Initialise state level zero
  m_StateLevels[0].m_pAuxillaryStack = 0;
  allocateStackAtLevel(0);

  // If we've been given a user stack pointer, we are a user mode thread.
  bool bUserMode = true;
  void *requestedStack = pStack;
  if (pStack == 0)
  {
    bUserMode = false;
    VirtualAddressSpace::Stack *kernelStack = m_StateLevels[0].m_pAuxillaryStack = m_StateLevels[0].m_pKernelStack;
    m_StateLevels[0].m_pKernelStack = 0; // No kernel stack if kernel mode thread - causes bug on PPC

    if (kernelStack)
      pStack = kernelStack->getTop();
  }

  if(semiUser)
  {
      // Still have a kernel stack for when we jump to user mode, but start the
      // thread in kernel mode first.
      bUserMode = false;

      // If no stack was given and we allocated, extract that allocated stack
      // back out again so we have a kernel stack proper.
      if (!requestedStack)
      {
        m_StateLevels[0].m_pKernelStack = m_StateLevels[0].m_pAuxillaryStack;
      }
  }

  m_Id = m_pParent->addThread(this);

  // Firstly, grab our lock so that the scheduler cannot preemptively load balance
  // us while we're starting.
  m_Lock.acquire();

  // Add to the scheduler
  if(!bDontPickCore)
  {
      ProcessorThreadAllocator::instance().addThread(this, pStartFunction, pParam, bUserMode, pStack);
  }
  else
  {
      Scheduler::instance().addThread(this, Processor::information().getScheduler());
      Processor::information().getScheduler().addThread(this, pStartFunction, pParam,
                                                        bUserMode, pStack);
  }
}

Thread::Thread(Process *pParent) :
    m_nStateLevel(0), m_pParent(pParent), m_Status(Running), m_ExitCode(0), /* m_pKernelStack(0), */ m_pAllocatedStack(0), m_Id(0),
    m_Errno(0), m_bInterrupted(false), m_Lock(), m_ConcurrencyLock(), m_EventQueue(), m_DebugState(None), m_DebugStateAddress(0),
    m_UnwindState(Continue), m_pScheduler(&Processor::information().getScheduler()), m_Priority(DEFAULT_PRIORITY),
    m_PendingRequests(), m_pTlsBase(0), m_bRemovingRequests(false), m_pWaiter(0), m_bDetached(false),
    m_bInterruptible(true)
{
  if (pParent == 0)
  {
    FATAL("Thread::Thread(): Parent process was NULL!");
  }
  m_Id = m_pParent->addThread(this);

  // Initialise our kernel stack.
  // NO! No kernel stack for kernel-mode threads. On PPC, causes bug!
  //m_pKernelStack = VirtualAddressSpace::getKernelAddressSpace().allocateStack();

  // Still add the idle thread to the Scheduler for things like threadInSchedule
  Scheduler::instance().addThread(this, *m_pScheduler);
}

Thread::Thread(Process *pParent, SyscallState &state) :
    m_nStateLevel(0), m_pParent(pParent), m_Status(Ready), m_ExitCode(0), /* m_pKernelStack(0), */ m_pAllocatedStack(0), m_Id(0),
    m_Errno(0), m_bInterrupted(false), m_Lock(), m_ConcurrencyLock(), m_EventQueue(), m_DebugState(None), m_DebugStateAddress(0),
    m_UnwindState(Continue), m_pScheduler(0), m_Priority(DEFAULT_PRIORITY),
    m_PendingRequests(), m_pTlsBase(0), m_bRemovingRequests(false), m_pWaiter(0), m_bDetached(false),
    m_bInterruptible(true)
{
  if (pParent == 0)
  {
    FATAL("Thread::Thread(): Parent process was NULL!");
  }

  // Initialise our kernel stack.
  // m_pKernelStack = VirtualAddressSpace::getKernelAddressSpace().allocateStack();
  m_pAllocatedStack = 0;

  // Initialise state level zero
  allocateStackAtLevel(0);

  m_Id = m_pParent->addThread(this);

  m_Lock.acquire();

  // Now we are ready to go into the scheduler.
  ProcessorThreadAllocator::instance().addThread(this, state);
}

Thread::~Thread()
{
  if(InputManager::instance().removeCallbackByThread(this))
  {
    WARNING("A thread is being removed, but it never removed itself from InputManager.");
    WARNING("This warning indicates an application or kernel module is buggy!");
  }

  // Before removing from the scheduler, terminate if needed.
  if (!m_bRemovingRequests)
  {
      shutdown();
  }

  // Clean up allocated stacks at each level.
  for(size_t i = 0; i < MAX_NESTED_EVENTS; i++)
  {
    if(m_StateLevels[i].m_pKernelStack)
        VirtualAddressSpace::getKernelAddressSpace().freeStack(m_StateLevels[i].m_pKernelStack);
    else if(m_StateLevels[i].m_pAuxillaryStack)
        VirtualAddressSpace::getKernelAddressSpace().freeStack(m_StateLevels[i].m_pAuxillaryStack);
    if (m_StateLevels[i].m_pUserStack && m_pParent)
        // Can't use Processor::getCurrent.. as by the time we're called
        // we may have switched address spaces to allow the thread to die.
        m_pParent->getAddressSpace()->freeStack(m_StateLevels[i].m_pUserStack);
  }

  // Clean up TLS base.
  if(m_pTlsBase && m_pParent)
  {
    // Unmap the TLS base.
    if (m_pParent->getAddressSpace()->isMapped(m_pTlsBase))
    {
        physical_uintptr_t phys = 0;
        size_t flags = 0;
        m_pParent->getAddressSpace()->getMapping(m_pTlsBase, phys, flags);
        m_pParent->getAddressSpace()->unmap(m_pTlsBase);
        PhysicalMemoryManager::instance().freePage(phys);
    }

    // Give the address space back to the process.
    uintptr_t base = reinterpret_cast<uintptr_t>(m_pTlsBase);
    m_pParent->m_Lock.acquire();
    if (m_pParent->getAddressSpace()->getDynamicStart())
        m_pParent->getDynamicSpaceAllocator().free(base, THREAD_TLS_SIZE);
    else
        m_pParent->getSpaceAllocator().free(base, THREAD_TLS_SIZE);
    m_pParent->m_Lock.release();
  }
  else if(m_pTlsBase)
  {
    ERROR("Thread: no parent, but a TLS base exists.");
  }

  // Remove us from the scheduler.
  Scheduler::instance().removeThread(this);

  if (m_pParent)
      m_pParent->removeThread(this);
}

void Thread::shutdown()
{
  // We are now removing requests from this thread - deny any other thread from
  // doing so, as that may invalidate our iterators.
  m_bRemovingRequests = true;

  if(m_PendingRequests.count())
  {
    FATAL("Thread::shutdown() - thread shutting down with pending requests.");
  }

  // Notify any waiters on this thread.
  if (m_pWaiter) {
    m_pWaiter->getLock().acquire();
    m_pWaiter->setStatus(Thread::Ready);
    m_pWaiter->getLock().release();
  }

  // Mark us as waiting for a join if we aren't detached. This ensures that join
  // will not block waiting for this thread if it is called after this point.
  m_ConcurrencyLock.acquire();
  if (!m_bDetached) {
    m_Status = AwaitingJoin;
  }
  m_ConcurrencyLock.release();
}

void Thread::forceToStartupProcessor()
{
    if (m_pScheduler == Scheduler::instance().getBootstrapProcessorScheduler())
    {
      // No need to move - we already think we're associated with the right
      // CPU, and that's all we'll do below anyway.
      return;
    }

    if (Processor::information().getCurrentThread() != this)
    {
      ERROR("Thread::forceToStartupProcessor must be run as the desired thread.");
      return;
    }

    Scheduler::instance().removeThread(this);
    m_pScheduler = Scheduler::instance().getBootstrapProcessorScheduler();
    Scheduler::instance().addThread(this, *m_pScheduler);
    Scheduler::instance().yield();
}

void Thread::setStatus(Thread::Status s)
{
  m_Status = s;
  m_pScheduler->threadStatusChanged(this);

  if(s == Thread::Zombie)
  {
    // Notify parent process we have become a zombie.
    // We do this here to avoid an amazing race between calling notifyWaiters
    // and scheduling a process into the Zombie state that can cause some
    // processes to simply never be reaped.
    if(m_pParent)
    {
        m_pParent->notifyWaiters();
    }
  }
}

SchedulerState &Thread::state()
{
    return *(m_StateLevels[m_nStateLevel].m_State);
}

SchedulerState &Thread::pushState()
{
    if ((m_nStateLevel + 1) >= MAX_NESTED_EVENTS)
    {
        ERROR("Thread: Max nested events!");
        /// \todo Take some action here - possibly kill the thread?
        return *(m_StateLevels[MAX_NESTED_EVENTS - 1].m_State);
    }
    m_nStateLevel++;
    // NOTICE("New state level: " << m_nStateLevel << "...");
    m_StateLevels[m_nStateLevel].m_InhibitMask = m_StateLevels[m_nStateLevel - 1].m_InhibitMask;
    allocateStackAtLevel(m_nStateLevel);

    setKernelStack();

    return *(m_StateLevels[m_nStateLevel - 1].m_State);
}

void Thread::popState()
{
    if (m_nStateLevel == 0)
    {
        ERROR("Thread: Potential error: popStack() called with state level 0!");
        ERROR("Thread: (ignore this if longjmp has been called)");
        return;
    }
    m_nStateLevel --;

    setKernelStack();
}

VirtualAddressSpace::Stack *Thread::getStateUserStack()
{
    return m_StateLevels[m_nStateLevel].m_pUserStack;
}

void Thread::setStateUserStack(VirtualAddressSpace::Stack *st)
{
    m_StateLevels[m_nStateLevel].m_pUserStack = st;
}

size_t Thread::getStateLevel() const
{
    return m_nStateLevel;
}

void Thread::threadExited()
{
  Processor::information().getScheduler().killCurrentThread();
}

void Thread::allocateStackAtLevel(size_t stateLevel)
{
    if(stateLevel >= MAX_NESTED_EVENTS)
        stateLevel = MAX_NESTED_EVENTS - 1;
    if(m_StateLevels[stateLevel].m_pKernelStack == 0)
        m_StateLevels[stateLevel].m_pKernelStack = VirtualAddressSpace::getKernelAddressSpace().allocateStack();
}

void *Thread::getKernelStack()
{
    if(m_nStateLevel >= MAX_NESTED_EVENTS)
        FATAL("m_nStateLevel > MAX_NESTED_EVENTS: " << m_nStateLevel << "...");
    if (m_StateLevels[m_nStateLevel].m_pKernelStack != 0)
    {
      return m_StateLevels[m_nStateLevel].m_pKernelStack->getTop();
    }
    else
    {
      return 0;
    }
}

void Thread::setKernelStack()
{
    if(m_StateLevels[m_nStateLevel].m_pKernelStack)
    {
        uintptr_t stack = reinterpret_cast<uintptr_t>(m_StateLevels[m_nStateLevel].m_pKernelStack->getTop());
        Processor::information().setKernelStack(stack);
    }
}

void Thread::pokeState(size_t stateLevel, SchedulerState &state)
{
    if (stateLevel >= MAX_NESTED_EVENTS)
    {
        ERROR("Thread::pokeState(): stateLevel `" << stateLevel << "' is over the maximum.");
        return;
    }
    *(m_StateLevels[stateLevel].m_State) = state;
}

void Thread::sendEvent(Event *pEvent)
{
    // Only need the lock to adjust the queue of events.
    m_Lock.acquire();
    m_EventQueue.pushBack(pEvent);
    m_Lock.release();

    if (m_Status == Sleeping && m_bInterruptible)
    {
        // Interrupt the sleeping thread, there's an event firing
        m_Status = Ready;

        // Notify the scheduler that we're now ready, so we get put into the
        // scheduling algorithm's ready queue.
        Scheduler::instance().threadStatusChanged(this);
    }
}

void Thread::inhibitEvent(size_t eventNumber, bool bInhibit)
{
    LockGuard<Spinlock> guard(m_Lock);
    if (bInhibit)
        m_StateLevels[m_nStateLevel].m_InhibitMask->set(eventNumber);
    else
        m_StateLevels[m_nStateLevel].m_InhibitMask->clear(eventNumber);
}

void Thread::cullEvent(Event *pEvent)
{
    LockGuard<Spinlock> guard(m_Lock);

    bool bDelete = false;
    for (List<Event*>::Iterator it = m_EventQueue.begin();
         it != m_EventQueue.end();
         it++)
    {
        if (*it == pEvent)
        {
            if ((*it)->isDeletable())
            {
                bDelete = true;
            }
            it = m_EventQueue.erase(it);
        }
    }

    // Delete last to avoid double frees.
    if (bDelete)
    {
      delete pEvent;
    }
}

void Thread::cullEvent(size_t eventNumber)
{
    LockGuard<Spinlock> guard(m_Lock);

    for (List<Event*>::Iterator it = m_EventQueue.begin();
         it != m_EventQueue.end();
         )
    {
        if ((*it)->getNumber() == eventNumber)
        {
            Event *pEvent = *it;
            it = m_EventQueue.erase(it);
            if (pEvent->isDeletable())
                delete pEvent;
        }
        else
            ++it;
    }
}

Event *Thread::getNextEvent()
{
    LockGuard<Spinlock> guard(m_Lock);

    for (size_t i = 0; i < m_EventQueue.count(); i++)
    {
        Event *e = m_EventQueue.popFront();
        if(!e)
        {
            ERROR("A null event was in a thread's event queue!");
            continue;
        }

        if (m_StateLevels[m_nStateLevel].m_InhibitMask->test(e->getNumber()) ||
            (e->getSpecificNestingLevel() != ~0UL &&
             e->getSpecificNestingLevel() != m_nStateLevel))
            m_EventQueue.pushBack(e);
        else
            return e;
    }

    return 0;
}

bool Thread::hasEvents()
{
    LockGuard<Spinlock> guard(m_Lock);

    return m_EventQueue.count() != 0;
}

void Thread::addRequest(SharedPointer<RequestQueue::Request> req)
{
    if(m_bRemovingRequests)
        return;

    m_PendingRequests.pushBack(req);
}

void Thread::removeRequest(SharedPointer<RequestQueue::Request> req)
{
    if(m_bRemovingRequests)
        return;

    for (auto it = m_PendingRequests.begin(); it != m_PendingRequests.end(); ++it)
    {
        if(req.get() == (*it).get())
        {
            m_PendingRequests.erase(it);
            return;
        }
    }
}

void Thread::unexpectedExit()
{
    NOTICE("Thread::unexpectedExit");
    if(m_bRemovingRequests)
        return;

    NOTICE("Thread::unexpectedExit COMPLETE");
}

uintptr_t Thread::getTlsBase()
{
    if(!m_StateLevels[0].m_pKernelStack)
        return 0;

    // Solves a problem where threads are created pointing to different address
    // spaces than the process that creates them (for whatever reason). Because
    // this is usually only called right after the address space switch in
    // PerProcessorScheduler, the address space is set properly.
    if(!m_pTlsBase)
    {
        // Get ourselves some space.
        uintptr_t base = 0;
        if (m_pParent->getAddressSpace()->getDynamicStart())
            m_pParent->getDynamicSpaceAllocator().allocate(THREAD_TLS_SIZE, base);
        else
            m_pParent->getSpaceAllocator().allocate(THREAD_TLS_SIZE, base);

        if (!base)
        {
            // Failed to allocate space.
            NOTICE("Thread [" << Dec << m_pParent->getId() << ":" << m_Id << Hex << "]: failed to allocate TLS area.");
            return base;
        }

        // Map.
        physical_uintptr_t phys = PhysicalMemoryManager::instance().allocatePage();
        m_pParent->getAddressSpace()->map(phys, reinterpret_cast<void *>(base), VirtualAddressSpace::Write);

        // Set up our thread ID to start with in the TLS region, now that it's
        // actually mapped into the address space.
        m_pTlsBase = reinterpret_cast<void *>(base);
        uint32_t *tlsBase = reinterpret_cast<uint32_t *>(m_pTlsBase);
#ifdef BITS_64
        *tlsBase = static_cast<uint32_t>(m_Id);
#else
        *tlsBase = m_Id;
#endif

        NOTICE("Thread [" << Dec << m_pParent->getId() << ":" << m_Id << Hex << "]: allocated TLS area at " << m_pTlsBase << ".");
    }
    return reinterpret_cast<uintptr_t>(m_pTlsBase);
}

void Thread::resetTlsBase()
{
    m_pTlsBase = 0;
    Processor::setTlsBase(getTlsBase());
}

bool Thread::join()
{
  Thread *pThisThread = Processor::information().getCurrentThread();

  m_ConcurrencyLock.acquire();

  // Can't join a detached thread.
  if (m_bDetached)
  {
    m_ConcurrencyLock.release();
    return false;
  }

  // Check thread state. Perhaps the join is just a matter of terminating this
  // thread, as it has died.
  if (m_Status != AwaitingJoin)
  {
    if (m_pWaiter)
    {
      // Another thread is already join()ing.
      m_ConcurrencyLock.release();
      return false;
    }

    m_pWaiter = pThisThread;
    pThisThread->setDebugState(Joining, reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
    m_ConcurrencyLock.release();

    while (1)
    {
      Processor::information().getScheduler().sleep(0);
      if (!(pThisThread->wasInterrupted() || pThisThread->getUnwindState() != Thread::Continue))
        break;
    }

    pThisThread->setDebugState(None, 0);
  }
  else
  {
    m_ConcurrencyLock.release();
  }

  // Thread has terminated, we may now clean up.
  delete this;
  return true;
}

bool Thread::detach()
{
  if (m_Status == AwaitingJoin)
  {
    WARNING("Thread::detach() called on a thread that has already exited.");
    return join();
  }
  else
  {
    LockGuard<Spinlock> guard(m_ConcurrencyLock);

    if (m_pWaiter) {
      ERROR("Thread::detach() called while other threads are joining.");
      return false;
    }

    m_bDetached = true;
    return true;
  }
}

Thread::StateLevel::StateLevel() :
    m_State(), m_pKernelStack(0), m_pUserStack(0), m_pAuxillaryStack(0),
    m_InhibitMask(), m_pBlockingThread(0)
{
    m_State = new SchedulerState;
    ByteSet(m_State, 0, sizeof(SchedulerState));
    m_InhibitMask = SharedPointer<ExtensibleBitmap>::allocate();
}

Thread::StateLevel::~StateLevel()
{
    delete m_State;
}

Thread::StateLevel::StateLevel(const Thread::StateLevel &s) :
    m_State(), m_pKernelStack(s.m_pKernelStack), m_pUserStack(s.m_pUserStack),
    m_pAuxillaryStack(s.m_pAuxillaryStack), m_InhibitMask(),
    m_pBlockingThread(s.m_pBlockingThread)
{
    m_State = new SchedulerState(*(s.m_State));
    m_InhibitMask = SharedPointer<ExtensibleBitmap>::allocate(*(s.m_InhibitMask));
}

Thread::StateLevel &Thread::StateLevel::operator = (const Thread::StateLevel &s)
{
    m_State = new SchedulerState(*(s.m_State));
    m_InhibitMask = SharedPointer<ExtensibleBitmap>::allocate(*(s.m_InhibitMask));
    m_pKernelStack = s.m_pKernelStack;
    return *this;
}

bool Thread::isInterruptible()
{
  return m_bInterruptible;
}

void Thread::setInterruptible(bool state)
{
  LockGuard<Spinlock> guard(m_Lock);
  m_bInterruptible = state;
}

#endif // THREADS
