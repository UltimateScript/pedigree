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

#ifndef ISO9660FILE_H
#define ISO9660FILE_H

#include "pedigree/kernel/LockGuard.h"
#include "pedigree/kernel/process/Mutex.h"
#include "pedigree/kernel/utilities/List.h"
#include "pedigree/kernel/utilities/Tree.h"
#include "pedigree/kernel/utilities/Vector.h"
#include <vfs/Filesystem.h>

#include "Iso9660Filesystem.h"
#include "iso9660.h"

class Iso9660File : public File
{
    friend class Iso9660Directory;

  private:
    /** Copy constructors are hidden - unused! */
    Iso9660File(const Iso9660File &);
    Iso9660File &operator=(const File &);
    Iso9660File &operator=(const Iso9660File &);

  public:
    /** Constructor, should be called only by a Filesystem. */
    Iso9660File(
        String name, Time::Timestamp accessedTime, Time::Timestamp modifiedTime,
        Time::Timestamp creationTime, uintptr_t inode,
        class Iso9660Filesystem *pFs, size_t size, Iso9660DirRecord &record,
        File *pParent = 0)
        : File(
              name, accessedTime, modifiedTime, creationTime, inode, pFs, size,
              pParent),
          m_pFs(pFs), m_Dir(record)
    {
    }
    virtual ~Iso9660File()
    {
    }

    inline Iso9660DirRecord &getDirRecord()
    {
        return m_Dir;
    }

  protected:
    virtual uintptr_t readBlock(uint64_t location);

    virtual size_t getBlockSize() const
    {
        return 2048;
    }

  private:
    // Filesystem object
    Iso9660Filesystem *m_pFs;

    // Our internal directory information (info about *this* directory, not the
    // child)
    Iso9660DirRecord m_Dir;
};

#endif
