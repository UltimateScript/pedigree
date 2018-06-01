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

#include "FtdiSerialDevice.h"
#include "modules/Module.h"
#include "modules/system/usb/UsbPnP.h"
#include "pedigree/kernel/utilities/new"

class UsbDevice;

static UsbDevice *ftdiConnected(UsbDevice *pDevice)
{
    return new FtdiSerialDevice(pDevice);
}

static bool entry()
{
    UsbPnP::instance().registerCallback(0x0403, 0x6001, ftdiConnected);
    return true;
}

static void exit()
{
}

MODULE_INFO("ftdi", &entry, &exit, "usb");
