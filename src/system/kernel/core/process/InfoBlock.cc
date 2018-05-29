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

#include "pedigree/kernel/Log.h"
#include "pedigree/kernel/process/InfoBlock.h"
#include "pedigree/kernel/Version.h"
#include "pedigree/kernel/compiler.h"
#include "pedigree/kernel/machine/Machine.h"
#include "pedigree/kernel/machine/Timer.h"
#include "pedigree/kernel/machine/TimerHandler.h"
#include "pedigree/kernel/processor/PhysicalMemoryManager.h"
#include "pedigree/kernel/processor/VirtualAddressSpace.h"
#include "pedigree/kernel/processor/state_forward.h"
#include "pedigree/kernel/processor/types.h"
#include "pedigree/kernel/time/Time.h"
#include "pedigree/kernel/utilities/utility.h"

InfoBlockManager InfoBlockManager::m_Instance;

InfoBlockManager::InfoBlockManager()
    : TimerHandler(), m_bInitialised(false), m_pInfoBlock(0)
{
}

InfoBlockManager::~InfoBlockManager()
{
    Machine::instance().getTimer()->unregisterHandler(this);
}

InfoBlockManager &InfoBlockManager::instance()
{
    return m_Instance;
}

bool InfoBlockManager::initialise()
{
    // Prepare the mapping, if we can.
    VirtualAddressSpace &va = VirtualAddressSpace::getKernelAddressSpace();
    void *infoBlock = reinterpret_cast<void *>(va.getGlobalInfoBlock());
    if (!infoBlock)
        return false;  // Nothing to do here.

    NOTICE(
        "InfoBlockManager: Setting up global info block at " << Hex
                                                             << infoBlock);

    // Map for userspace.
    physical_uintptr_t page = PhysicalMemoryManager::instance().allocatePage();
    va.map(page, infoBlock, 0);

    // Map for the kernel - trick is, our version is a page ahead.
    m_pInfoBlock =
        reinterpret_cast<InfoBlock *>(adjust_pointer(infoBlock, 0x1000));
    va.map(
        page, m_pInfoBlock,
        VirtualAddressSpace::KernelMode | VirtualAddressSpace::Write);

    // Set up basic defaults.
    ByteSet(m_pInfoBlock, 0, sizeof(InfoBlock));
    StringCopy(m_pInfoBlock->sysname, "Pedigree");
    StringCopy(m_pInfoBlock->release, "Foster");
    StringCopy(m_pInfoBlock->version, g_pBuildRevision);
    /// \todo this isn't quite x86_64 or i686 or similar...
    StringCopy(m_pInfoBlock->machine, g_pBuildTarget);

    // Register ourselves with the main timer.
    m_bInitialised = true;
    return Machine::instance().getTimer()->registerHandler(this);
}

void InfoBlockManager::timer(uint64_t, InterruptState &)
{
    // Update the timestamp in the info block.
    m_pInfoBlock->now = Time::getTimeNanoseconds();
    m_pInfoBlock->now_s = Time::getTime();
}

void InfoBlockManager::setPid(size_t value)
{
    if (LIKELY(m_bInitialised))
        m_pInfoBlock->pid = value;
}
