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

#ifndef NE2K_H
#define NE2K_H

#include "pedigree/kernel/machine/Device.h"
#include "pedigree/kernel/machine/IrqHandler.h"
#include "pedigree/kernel/machine/Network.h"
#include "pedigree/kernel/process/Semaphore.h"
#include "pedigree/kernel/process/Thread.h"
#include "pedigree/kernel/processor/IoBase.h"
#include "pedigree/kernel/processor/IoPort.h"
#include "pedigree/kernel/processor/types.h"
#include "pedigree/kernel/utilities/List.h"
#include "pedigree/kernel/utilities/RequestQueue.h"

#define NE2K_VENDOR_ID 0x10ec
#define NE2K_DEVICE_ID 0x8029

/** Device driver for the NE2K class of network device */
class Ne2k : public Network, public IrqHandler
{
  public:
    Ne2k(Network *pDev);
    virtual ~Ne2k();

    virtual void getName(String &str)
    {
        str = "ne2k";
    }

    virtual bool send(size_t nBytes, uintptr_t buffer);

    virtual bool setStationInfo(StationInfo info);

    virtual StationInfo getStationInfo();

    // IRQ handler callback.
    virtual bool irq(irq_id_t number, InterruptState &state);

    IoBase *m_pBase;

    bool isConnected();

  private:
    void recv();

    static int trampoline(void *p) NORETURN;

    void receiveThread() NORETURN;

    struct packet
    {
        uintptr_t ptr;
        size_t len;
    };
    uint8_t m_NextPacket;

    Semaphore m_PacketQueueSize;
    List<packet *> m_PacketQueue;

    Spinlock m_PacketQueueLock;

    Ne2k(const Ne2k &);
    void operator=(const Ne2k &);
};

#endif
