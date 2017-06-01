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

#ifndef KERNEL_MACHINE_X86_COMMON_APIC_H
#define KERNEL_MACHINE_X86_COMMON_APIC_H

#if defined(APIC)

#include "LocalApic.h"
#include <machine/IrqManager.h>

/** @addtogroup kernelmachinex86common
 * @{ */

/** The x86/x64 advanced programmable interrupt controller architecture as
 * IrqManager */
class Apic : public IrqManager
{
    public:
    /** The default constructor */
    inline Apic()
    {
    }
    /** The destructor */
    inline virtual ~Apic()
    {
    }

    //
    // IrqManager interface
    //
    virtual irq_id_t
    registerIsaIrqHandler(uint8_t, IrqHandler *handler, bool bEdge = false);
    virtual irq_id_t
    registerPciIrqHandler(IrqHandler *handler, Device *pDevice);
    virtual void acknowledgeIrq(irq_id_t Id);
    virtual void unregisterHandler(irq_id_t Id, IrqHandler *handler);
    virtual void enable(irq_id_t Id, bool bEnable);

    bool initialise() INITIALISATION_ONLY;

    private:
    /** The copy-constructor
     *\note NOT implemented */
    Apic(const Apic &);
    /** The assignment operator
     *\note NOT implemented */
    Apic &operator=(const Apic &);

    /** The Apic instance */
    static Apic m_Instance;
};

/** @} */

#endif

#endif
