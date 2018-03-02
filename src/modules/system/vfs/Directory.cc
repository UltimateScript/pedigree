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

#include "Directory.h"
#include "Filesystem.h"

Directory::Directory() : File(), m_Cache(), m_bCachePopulated(false)
{
}

Directory::Directory(
    const String &name, Time::Timestamp accessedTime,
    Time::Timestamp modifiedTime, Time::Timestamp creationTime, uintptr_t inode,
    Filesystem *pFs, size_t size, File *pParent)
    : File(
          name, accessedTime, modifiedTime, creationTime, inode, pFs, size,
          pParent),
      m_Cache(pFs->isCaseSensitive()), m_LinearCache(), m_bCachePopulated(false)
{
}

Directory::~Directory()
{
}

File *Directory::getChild(size_t n)
{
    if (!m_bCachePopulated)
    {
        cacheDirectoryContents();
        m_bCachePopulated = true;
    }

    if (!m_LinearCache.count())
    {
        return 0;
    }
    else if (n >= m_LinearCache.count())
    {
        return 0;
    }

    return m_LinearCache[n]->get();
}

size_t Directory::getNumChildren()
{
    if (!m_bCachePopulated)
    {
        cacheDirectoryContents();
        m_bCachePopulated = true;
    }

    return m_LinearCache.count();
}

void Directory::cacheDirectoryContents()
{
}

File *Directory::lookup(const String &s) const
{
    if (!m_bCachePopulated)
    {
        return 0;
    }

    DirectoryEntry *entry = m_Cache.lookup(s);
    if (!entry)
    {
        return nullptr;
    }
    else
    {
        return entry->get();
    }
}

void Directory::remove(const String &s)
{
    DirectoryEntry *v = m_Cache.lookup(s);
    if (v)
    {
        m_Cache.remove(s);

        for (auto it = m_LinearCache.begin(); it != m_LinearCache.end(); ++it)
        {
            if ((*it) == v)
            {
                m_LinearCache.erase(it);
                break;
            }
        }

        delete v;
    }
}

void Directory::addDirectoryEntry(const String &name, File *pTarget)
{
    DirectoryEntry *entry = new DirectoryEntry(pTarget);

    m_Cache.insert(name, entry);
    m_LinearCache.pushBack(entry);

    m_bCachePopulated = true;
}

void Directory::addDirectoryEntry(const String &name, const DirectoryEntryMetadata &meta)
{
    DirectoryEntry *entry = new DirectoryEntry(meta);

    m_Cache.insert(name, entry);
    m_LinearCache.pushBack(entry);

    m_bCachePopulated = true;
}

Directory *Directory::getReparsePoint() const
{
    return m_ReparseTarget;
}

void Directory::setReparsePoint(Directory *pTarget)
{
    m_ReparseTarget = pTarget;
}

bool Directory::addEphemeralFile(File *pFile)
{
    if (!m_bCachePopulated)
    {
        cacheDirectoryContents();
        m_bCachePopulated = true;
    }

    String name = pFile->getName();
    if (m_Cache.lookup(name))
    {
        // already exists!
        return false;
    }

    /// \todo removal will still want to hit the Filesystem here! not good!
    DirectoryEntry *entry = new DirectoryEntry(pFile);

    m_Cache.insert(name, entry);
    m_LinearCache.pushBack(entry);

    return true;
}

File *Directory::evaluateEntry(const DirectoryEntryMetadata &meta)
{
    if (!meta.pDirectory)
    {
        return nullptr;
    }
    return meta.pDirectory->convertToFile(meta);
}

void Directory::destroyEntry(File *file)
{
    /// \todo figure out how to destroy File objects!
    // Thinking maybe something on VFS that tracks File objects and once they
    // hit zero references, they get culled (i.e. once no more file descriptors
    // etc are present)
}

File *Directory::convertToFile(const DirectoryEntryMetadata &meta)
{
    return nullptr;
}
