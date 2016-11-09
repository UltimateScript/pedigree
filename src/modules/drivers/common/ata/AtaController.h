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

#ifndef ATA_CONTROLLER_H
#define ATA_CONTROLLER_H

#include <processor/types.h>
#include <machine/Device.h>
#include <machine/Disk.h>
#include <machine/Controller.h>
#include <processor/IoBase.h>
#include <processor/IoPort.h>
#include <utilities/RequestQueue.h>
#include <machine/IrqHandler.h>
#include <Log.h>

#include <scsi/ScsiController.h>

/** Base class for an ATA controller. */
class AtaController : public ScsiController, public IrqHandler
{
public:
    AtaController(Controller *pDev, int nController = 0) :
        ScsiController(pDev), m_nController(nController)
    {
        setSpecificType(String("ata-controller"));

        // Ensure we have no stupid children lying around.
        m_Children.clear();
    }
    virtual ~AtaController()
    {
    }

    virtual void getName(String &str) = 0;

    virtual bool compareRequests(const RequestQueue::Request &a, const RequestQueue::Request &b);

    // IRQ handler callback.
    virtual bool irq(irq_id_t number, InterruptState &state)
    {
        NOTICE("AtaController: irq" << Dec << number << Hex << " ignored");
        return false;
    }

    IoBase *m_pCommandRegs;
    IoBase *m_pControlRegs;
private:
    AtaController(const AtaController&);
    void operator =(const AtaController&);

protected:
    int m_nController;

    virtual size_t getNumUnits()
    {
        return getNumChildren();
    }
};

#endif
