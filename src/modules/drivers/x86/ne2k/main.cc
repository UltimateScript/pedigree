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

#include "Ne2k.h"
#include "modules/Module.h"
#include "pedigree/kernel/Log.h"
#include "pedigree/kernel/machine/Device.h"
#include "pedigree/kernel/utilities/new"

class Network;

static bool bFound = false;

static void probeDevice(Device *pDev)
{
    NOTICE("NE2K found");

    // Create a new NE2K node
    Ne2k *pNe2k = new Ne2k(reinterpret_cast<Network *>(pDev));

    // Replace pDev with pNe2k.
    pNe2k->setParent(pDev->getParent());
    pDev->getParent()->replaceChild(pDev, pNe2k);

    bFound = true;
}

static bool entry()
{
    Device::searchByVendorIdAndDeviceId(
        NE2K_VENDOR_ID, NE2K_DEVICE_ID, probeDevice);

    return bFound;
}

static void exit()
{
}

MODULE_NAME("ne2k");
MODULE_ENTRY(&entry);
MODULE_EXIT(&exit);
MODULE_DEPENDS("network-stack");
