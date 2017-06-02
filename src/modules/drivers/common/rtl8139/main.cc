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

#include "Rtl8139.h"
#include "pedigree/kernel/Log.h"
#include <Module.h>
#include "pedigree/kernel/machine/Device.h"
#include "pedigree/kernel/machine/Network.h"
#include "pedigree/kernel/processor/Processor.h"
#include "pedigree/kernel/processor/types.h"

void probeDevice(Device *pDev)
{
    NOTICE("RTL8139 found");

    // Create a new RTL8139 node
    Rtl8139 *pRtl8139 = new Rtl8139(reinterpret_cast<Network *>(pDev));

    // Replace pDev with pRtl8139.
    pRtl8139->setParent(pDev->getParent());
    pDev->getParent()->replaceChild(pDev, pRtl8139);
}

void entry()
{
    /// \todo replace with foreach (which can do the replacement for us)
    Device::searchByVendorIdAndDeviceId(
        RTL8139_VENDOR_ID, RTL8139_DEVICE_ID, probeDevice);
}

void exit()
{
}

MODULE_INFO("rtl8139", &entry, &exit, "network-stack");
