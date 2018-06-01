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

#ifndef FAT_DIRECTORY_H
#define FAT_DIRECTORY_H

#include "fat.h"
#include "modules/system/vfs/Directory.h"
#include "pedigree/kernel/process/Mutex.h"
#include "pedigree/kernel/processor/types.h"
#include "pedigree/kernel/utilities/String.h"

class File;

/** A File is a file, a directory or a symlink. */
class FatDirectory : public Directory
{
  private:
    /** Copy constructors are hidden - unused! */
    FatDirectory(const FatDirectory &file);
    FatDirectory &operator=(const FatDirectory &);

  public:
    /** Constructor, should be called only by a Filesystem. */
    FatDirectory(
        String name, uintptr_t cluster, class FatFilesystem *pFs, File *pParent,
        FatFileInfo &info, uint32_t dirClus = 0, uint32_t dirOffset = 0);
    /** Destructor */
    virtual ~FatDirectory();

    void truncate()
    {
    }

    /** Reads directory contents into File* cache. */
    virtual void cacheDirectoryContents();

    /** Adds a directory entry. */
    virtual bool addEntry(String filename, File *pFile, size_t type);
    /** Removes a directory entry. */
    virtual bool removeEntry(File *pFile);

    /** Updates inode attributes. */
    void fileAttributeChanged();

    /** Set the internal cluster (in the case of FAT) */
    virtual void setInode(uintptr_t inode);

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

    static const String &symlinkSuffix()
    {
        static String s(".__sym");
        return s;
    }

  private:
    uint32_t m_DirClus;
    uint32_t m_DirOffset;

    FatType m_Type;
    uintptr_t m_BlockSize;
    bool m_bRootDir;

    /** Lock for add/remove/cache operations, so we don't break horribly. */
    Mutex m_Lock;

    /** Number of bytes to iterate over for this directory */
    uint32_t m_DirBlockSize;
};

#endif
