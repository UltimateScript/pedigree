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

#include "FileDescriptor.h"
#include "net-syscalls.h"  // to get destructor for SharedPointer<NetworkSyscalls>

#include "modules/system/vfs/File.h"
#include "subsys/posix/IoEvent.h"

#include <fcntl.h>

#define ENABLE_LOCKED_FILES 0

#if ENABLE_LOCKED_FILES
RadixTree<LockedFile *> g_PosixGlobalLockedFiles;
#endif

/// Default constructor
FileDescriptor::FileDescriptor()
    : file(0), offset(0), fd(0xFFFFFFFF), fdflags(0), flflags(0), lockedFile(0),
      networkImpl(nullptr), ioevent(nullptr)
{
}

/// Parameterised constructor
FileDescriptor::FileDescriptor(
    File *newFile, uint64_t newOffset, size_t newFd, int fdFlags, int flFlags,
    LockedFile *lf)
    : file(newFile), offset(newOffset), fd(newFd), fdflags(fdFlags),
      flflags(flFlags), lockedFile(lf), networkImpl(nullptr), ioevent(nullptr)
{
    /// \todo need a copy constructor for networkImpl
    if (file)
    {
#if ENABLE_LOCKED_FILES
        lockedFile = g_PosixGlobalLockedFiles.lookup(file->getFullPath());
#endif
        file->increaseRefCount((flflags & O_RDWR) || (flflags & O_WRONLY));
    }
}

/// Copy constructor
FileDescriptor::FileDescriptor(FileDescriptor &desc)
    : file(desc.file), offset(desc.offset), fd(desc.fd), fdflags(desc.fdflags),
      flflags(desc.flflags), lockedFile(0), networkImpl(desc.networkImpl),
      ioevent(nullptr)
{
    if (file)
    {
#if ENABLE_LOCKED_FILES
        lockedFile = g_PosixGlobalLockedFiles.lookup(file->getFullPath());
#endif
        file->increaseRefCount((flflags & O_RDWR) || (flflags & O_WRONLY));
    }

#ifdef THREADS
    if (desc.ioevent)
    {
        ioevent = new IoEvent(*desc.ioevent);
    }
#endif
}

/// Pointer copy constructor
FileDescriptor::FileDescriptor(FileDescriptor *desc)
    : file(0), offset(0), fd(0), fdflags(0), flflags(0), lockedFile(0), ioevent(nullptr)
{
    if (!desc)
        return;

    file = desc->file;
    offset = desc->offset;
    fd = desc->fd;
    fdflags = desc->fdflags;
    flflags = desc->flflags;
    networkImpl = desc->networkImpl;
    if (file)
    {
#if ENABLE_LOCKED_FILES
        lockedFile = g_PosixGlobalLockedFiles.lookup(file->getFullPath());
#endif
        file->increaseRefCount((flflags & O_RDWR) || (flflags & O_WRONLY));
    }

#ifdef THREADS
    if (desc->ioevent)
    {
        ioevent = new IoEvent(*desc->ioevent);
    }
#endif
}

/// Assignment operator implementation
FileDescriptor &FileDescriptor::operator=(FileDescriptor &desc)
{
    file = desc.file;
    offset = desc.offset;
    fd = desc.fd;
    fdflags = desc.fdflags;
    flflags = desc.flflags;
    networkImpl = desc.networkImpl;
    if (file)
    {
#if ENABLE_LOCKED_FILES
        lockedFile = g_PosixGlobalLockedFiles.lookup(file->getFullPath());
#endif
        file->increaseRefCount((flflags & O_RDWR) || (flflags & O_WRONLY));
    }
#ifdef THREADS
    if (desc.ioevent)
    {
        ioevent = new IoEvent(*desc.ioevent);
    }
    else
    {
        ioevent = nullptr;
    }
#endif
    return *this;
}

/// Destructor - decreases file reference count
FileDescriptor::~FileDescriptor()
{
    if (file)
    {
#if ENABLE_LOCKED_FILES
        // Unlock the file we have a lock on, release from the global lock table
        if (lockedFile)
        {
            g_PosixGlobalLockedFiles.remove(file->getFullPath());
            lockedFile->unlock();
            delete lockedFile;
        }
#endif
        file->decreaseRefCount((flflags & O_RDWR) || (flflags & O_WRONLY));
    }
#ifdef THREADS
    if (ioevent)
    {
        if (networkImpl)
        {
            networkImpl->unmonitor(ioevent);
        }
        delete ioevent;
    }
#endif

    /// \note sockets are cleaned up by their reference count hitting zero (SharedPointer)
}
