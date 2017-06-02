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

#ifndef FAT_FILE_H
#define FAT_FILE_H

#include "pedigree/kernel/processor/types.h"
#include "pedigree/kernel/time/Time.h"
#include "pedigree/kernel/utilities/Cache.h"
#include "pedigree/kernel/utilities/RadixTree.h"
#include "pedigree/kernel/utilities/String.h"
#include "modules/system/vfs/File.h"

/** A File is a file, a directory or a symlink. */
class FatFile : public File
{
  private:
    /** Copy constructors are hidden - unused! */
    FatFile(const File &file);
    File &operator=(const File &);

  public:
    /** Constructor, should be called only by a Filesystem. */
    FatFile(
        String name, Time::Timestamp accessedTime, Time::Timestamp modifiedTime,
        Time::Timestamp creationTime, uintptr_t inode, class Filesystem *pFs,
        size_t size, uint32_t dirClus = 0, uint32_t dirOffset = 0,
        File *pParent = 0);
    /** Destructor - doesn't do anything. */
    virtual ~FatFile();

    uint32_t getDirCluster()
    {
        return m_DirClus;
    }
    void setDirCluster(uint32_t custom)
    {
        m_DirClus = custom;
    }
    uint32_t getDirOffset()
    {
        return m_DirOffset;
    }
    void setDirOffset(uint32_t custom)
    {
        m_DirOffset = custom;
    }

    uintptr_t readBlock(uint64_t location);
    void writeBlock(uint64_t location, uintptr_t addr);

    void extend(size_t newSize);

    using File::sync;
    virtual void sync(size_t offset, bool async);

    virtual void pinBlock(uint64_t location);
    virtual void unpinBlock(uint64_t location);

  private:
    uint32_t m_DirClus;
    uint32_t m_DirOffset;

    Cache m_FileBlockCache;
};

#endif
