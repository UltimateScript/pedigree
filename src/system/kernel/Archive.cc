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

#include "pedigree/kernel/Archive.h"
#include "pedigree/kernel/Log.h"
#include "pedigree/kernel/panic.h"
#include "pedigree/kernel/processor/PhysicalMemoryManager.h"
#include "pedigree/kernel/processor/VirtualAddressSpace.h"
#include "pedigree/kernel/utilities/StaticString.h"

Archive::Archive(uint8_t *pPhys, size_t sSize)
    :
#ifdef HOSTED
      m_pBase(pPhys)
#else
      m_Region("Archive")
#endif
{
#ifndef HOSTED
    if ((reinterpret_cast<physical_uintptr_t>(pPhys) &
         (PhysicalMemoryManager::getPageSize() - 1)) != 0)
        panic("Archive: Alignment issues");

    if (PhysicalMemoryManager::instance().allocateRegion(
            m_Region,
            (sSize + PhysicalMemoryManager::getPageSize() - 1) /
                PhysicalMemoryManager::getPageSize(),
            PhysicalMemoryManager::continuous, VirtualAddressSpace::KernelMode,
            reinterpret_cast<physical_uintptr_t>(pPhys)) == false)
    {
        ERROR("Archive: allocateRegion failed.");
    }
#endif
}

Archive::~Archive()
{
#ifndef HOSTED
    m_Region.free();
#endif
}

size_t Archive::getNumFiles()
{
    size_t i = 0;
    File *pFile = getFirst();
    while (pFile != 0)
    {
        i++;
        pFile = getNext(pFile);
    }
    return i;
}

size_t Archive::getFileSize(size_t n)
{
    File *pFile = get(n);
    NormalStaticString str(pFile->size);
    return str.intValue(8);  // Octal
}

char *Archive::getFileName(size_t n)
{
    return get(n)->name;
}

uintptr_t *Archive::getFile(size_t n)
{
    return reinterpret_cast<uintptr_t *>(
        reinterpret_cast<uintptr_t>(get(n)) + 512);
}

Archive::File *Archive::getFirst()
{
#ifdef HOSTED
    return reinterpret_cast<File *>(m_pBase);
#else
    return reinterpret_cast<File *>(m_Region.virtualAddress());
#endif
}

Archive::File *Archive::getNext(File *pFile)
{
    NormalStaticString str(pFile->size);
    size_t size = str.intValue(8);  // Octal.
    size_t nBlocks = (size + 511) / 512;
    pFile = adjust_pointer(pFile, 512 * (nBlocks + 1));
    if (pFile->name[0] == '\0')
        return 0;
    return pFile;
}

Archive::File *Archive::get(size_t n)
{
    File *pFile = getFirst();
    for (size_t i = 0; i < n; i++)
        pFile = getNext(pFile);
    return pFile;
}
