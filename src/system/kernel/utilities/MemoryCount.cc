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

#include "pedigree/kernel/utilities/MemoryCount.h"
#include "pedigree/kernel/Log.h"
#include "pedigree/kernel/processor/PhysicalMemoryManager.h"
#include "pedigree/kernel/processor/types.h"

MemoryCount::MemoryCount(const char *context)
{
    m_StartPages = PhysicalMemoryManager::instance().freePageCount();
    m_EndPages = 0;
    m_Context = context;
}

MemoryCount::~MemoryCount()
{
    m_EndPages = PhysicalMemoryManager::instance().freePageCount();
    ssize_t diff = static_cast<ssize_t>(m_StartPages - m_EndPages);
    NOTICE(
        "KERNELELF: Page difference while executing " << m_Context << ": "
                                                      << Dec << diff << Hex);
    NOTICE(
        "KERNELELF:   -> difference is " << Dec << ((diff * 4096) / 1024) << Hex
                                         << "K");
}
