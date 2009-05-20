/*
 * Copyright (c) 2008 James Molloy, Jörg Pfähler, Matthew Iselin
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

#ifndef PROCESS_H
#define PROCESS_H

#include <process/Thread.h>
#include <processor/state.h>
#include <utilities/Vector.h>
#include <utilities/StaticString.h>
#include <utilities/String.h>
#include <Atomic.h>
#include <Spinlock.h>
#include <LockGuard.h>
#include <process/Semaphore.h>
#include <process/Mutex.h>
#include <utilities/Tree.h>
#include <utilities/MemoryAllocator.h>

#include <process/SignalEvent.h>

class VirtualAddressSpace;
class File;
class FileDescriptor;
class User;
class Group;
class DynamicLinker;

/**
 * An abstraction of a Process - a container for one or more threads all running in
 * the same address space.
 */
class Process
{
public:
    /** Default constructor. */
    Process();

    /** Constructor for creating a new Process. Creates a new Process as
     * a UNIX fork() would, from the given parent process. This constructor
     * does not create any threads.
     * \param pParent The parent process. */
    Process(Process *pParent);

    /** Destructor. */
    ~Process();

    /** Adds a thread to this process.
     *  \return The thread ID to be assigned to the new Thread. */
    size_t addThread(Thread *pThread);
    /** Removes a thread from this process. */
    void removeThread(Thread *pThread);

    /** Returns the number of threads in this process. */
    size_t getNumThreads();
    /** Returns the n'th thread in this process. */
    Thread *getThread(size_t n);

    /** Creates a new process, with a single thread and a stack. */
    static uintptr_t create(uint8_t *elf, size_t elfSize, const char *name);

    /** Returns the process ID. */
    size_t getId()
    {
        return m_Id;
    }

    /** Returns the description string of this process. */
    LargeStaticString &description()
    {
        return str;
    }

    /** Returns our address space */
    VirtualAddressSpace *getAddressSpace()
    {
        return m_pAddressSpace;
    }

    /** Returns the File descriptor map - maps numbers to pointers (of undefined type -
        the subsystem decides what type). */
    Tree<size_t,FileDescriptor*> &getFdMap()
    {
        return m_FdMap;
    }

    /** Returns the next available file descriptor. */
    size_t nextFd()
    {
        LockGuard<Spinlock> guard(m_FdLock); // Must be atomic.
        return m_NextFd++;
    }
    size_t nextFd(size_t fdNum)
    {
        LockGuard<Spinlock> guard(m_FdLock); // Must be atomic.
        return (m_NextFd = fdNum);
    }

    /** Sets the exit status of the process. */
    void setExitStatus(int code)
    {
        m_ExitStatus = code;
    }
    /** Gets the exit status of the process. */
    int getExitStatus()
    {
        return m_ExitStatus;
    }

    /** Kills the process. */
    void kill();

    /** Returns the parent process. */
    Process *getParent()
    {
        return m_pParent;
    }

    /** Returns the current working directory. */
    File *getCwd()
    {
        return m_Cwd;
    }
    /** Sets the current working directory. */
    void setCwd(File *f)
    {
        m_Cwd = f;
    }

    /** Returns the memory space allocator for shared libraries. */
    MemoryAllocator &getSpaceAllocator()
    {
        return m_SpaceAllocator;
    }

    /** Gets the current user. */
    User *getUser()
    {
        return m_pUser;
    }
    /** Sets the current user. */
    void setUser(User *pUser)
    {
        m_pUser = pUser;
    }

    /** Gets the current group. */
    Group *getGroup()
    {
        return m_pGroup;
    }
    /** Sets the current group. */
    void setGroup(Group *pGroup)
    {
        m_pGroup = pGroup;
    }

    /** Public pending signal type - provides for features in sigaction */
    struct PendingSignal
    {
        /// Signal number
        size_t sig;

        /// Signal handler location
        uintptr_t sigLocation;

        /// Process signal mask to set when running this signal handler
        /// \todo Does this then get overridden when the signal handler returns?
        ///       If so, the previous mask needs to be stored somewhere as well.
        uint32_t sigMask;
    };

    /** A signal handler description */
    struct SignalHandler
    {
        /// Signal number
        size_t sig;

        /// Event for the signal handler
        SignalEvent *pEvent;

        /// Location of the handler <obsolete>
        uintptr_t handlerLocation_OBS;

        /// Signal mask to set when this signal handler is called
        uint32_t sigMask;

        /// Signal handler flags
        uint32_t flags;

        /// Type - 0 = normal, 1 = SIG_DFL, 2 = SIG_IGN
        int type;
    };

    /** Gets the pending signals list */
    List<void*>& getPendingSignals()
    {
        LockGuard<Mutex> guard(m_PendingSignalsLock);

        return m_PendingSignals;
    }

    /** Adds a new pendng signal */
    void addPendingSignal(size_t sig, PendingSignal* pending)
    {
#if 0
        LockGuard<Mutex> guard(m_PendingSignalsLock);

        if(pending)
        {
            SignalHandler* p = getSignalHandler(sig);
            if(p)
            {
                pending->sigLocation = p->handlerLocation;
                pending->sig = sig;
                m_PendingSignals.pushBack(pending);
            }
        }
#endif
    }

    /** Sets a signal handler */
    void setSignalHandler(size_t sig, SignalHandler* handler);

    /** Gets a signal handler */
    SignalHandler* getSignalHandler(size_t sig)
    {
        LockGuard<Mutex> guard(m_SignalHandlersLock);

        return reinterpret_cast<SignalHandler*>(m_SignalHandlers.lookup(sig % 32));
    }

    void setSigReturnStub(uintptr_t p)
    {
        m_SigReturnStub = p;
    }

    uintptr_t getSigReturnStub()
    {
        return m_SigReturnStub;
    }

    void setSignalMask(uint32_t mask)
    {
        m_SignalMask = mask;
    }

    uint32_t getSignalMask()
    {
        return m_SignalMask;
    }

    void setLinker(DynamicLinker *pDl)
    {
        m_pDynamicLinker = pDl;
    }
    DynamicLinker *getLinker()
    {
        return m_pDynamicLinker;
    }

private:
    Process(const Process &);
    Process &operator = (const Process &);

    /**
     * Our list of threads.
     */
    Vector<Thread*> m_Threads;
    /**
     * The next available thread ID.
     */
    Atomic<size_t> m_NextTid;
    /**
     * Our Process ID.
     */
    size_t m_Id;
    /**
     * Our description string.
     */
    LargeStaticString str;
    /**
     * Our parent process.
     */
    Process *m_pParent;
    /**
     * Our virtual address space.
     */
    VirtualAddressSpace *m_pAddressSpace;
    /**
     * The file descriptor map. Maps number to pointers, the type of which is decided
     * by the subsystem.
     */
    Tree<size_t,FileDescriptor*> m_FdMap;
    /**
     * The next available file descriptor.
     */
    size_t m_NextFd;
    /**
     * Lock to guard the next file descriptor while it is being changed.
     */
    Spinlock m_FdLock;
    /**
     * Process exit status.
     */
    int m_ExitStatus;
    /**
     * Current working directory.
     */
    File *m_Cwd;
    /**
     * Memory allocator for shared libraries - free parts of the address space.
     */
    MemoryAllocator m_SpaceAllocator;
    /** Current user. */
    User *m_pUser;
    /** Current group. */
    Group *m_pGroup;

    /** Pending signals */
    List<void*> m_PendingSignals;

    /** Signal handlers (userspace pointers) */
    Tree<size_t, void*> m_SignalHandlers;

    /** Location of the signal handler return stub */
    uintptr_t m_SigReturnStub;

    /** Signal mask - if a bit is set, that signal is masked */
    uint32_t m_SignalMask;

    /** A lock for access to the signal handlers tree */
    Mutex m_SignalHandlersLock;

    /** A lock for access to pending signals
     * \note This might not actually work - testing required!
     */
    Mutex m_PendingSignalsLock;

    /** The Process' dynamic linker. */
    DynamicLinker *m_pDynamicLinker;

public:
    Semaphore m_DeadThreads;
};

#endif
