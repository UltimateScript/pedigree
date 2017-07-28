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

#include "pedigree/kernel/processor/Processor.h"

size_t Processor::m_Initialised = 0;
#if !defined(MULTIPROCESSOR)
ProcessorInformation Processor::m_ProcessorInformation(0);
#else
Vector<ProcessorInformation *> Processor::m_ProcessorInformation;
ProcessorInformation Processor::m_SafeBspProcessorInformation(0);
#endif

size_t Processor::m_nProcessors = 1;

size_t Processor::isInitialised()
{
    return m_Initialised;
}

#if !defined(MULTIPROCESSOR)
ProcessorId Processor::id()
{
    return 0;
}

ProcessorInformation &Processor::information()
{
    return m_ProcessorInformation;
}

size_t Processor::getCount()
{
    return 1;
}
#endif

#ifndef PEDIGREE_BENCHMARK
EnsureInterrupts::EnsureInterrupts(bool desired)
{
    m_bPrevious = Processor::getInterrupts();
    Processor::setInterrupts(desired);
}

EnsureInterrupts::~EnsureInterrupts()
{
    Processor::setInterrupts(m_bPrevious);
}
#endif
