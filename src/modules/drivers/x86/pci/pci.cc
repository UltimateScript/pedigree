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

#include "pedigree/kernel/machine/Pci.h"
#include "modules/Module.h"
#include "pci_list.h"
#include "pedigree/kernel/Log.h"
#include "pedigree/kernel/machine/Bus.h"
#include "pedigree/kernel/machine/Device.h"
#include "pedigree/kernel/machine/Machine.h"
#include "pedigree/kernel/processor/IoPort.h"
#include "pedigree/kernel/processor/types.h"
#include "pedigree/kernel/utilities/utility.h"

#define CONFIG_ADDRESS 0
#define CONFIG_DATA 4

#define MAX_BUS 4

static IoPort configSpace("PCI config space");

union ConfigAddress
{
    struct
    {
        uint32_t always0 : 2;
        uint32_t offset : 6;
        uint32_t function : 3;
        uint32_t device : 5;
        uint32_t bus : 8;
        uint32_t reserved : 7;
        uint32_t enable : 1;
    } __attribute__((packed));
    uint32_t raw;
};

static void readConfigSpace(Device *pDev, PciBus::ConfigSpace *pCs)
{
    uint32_t *pCs32 = reinterpret_cast<uint32_t *>(pCs);
    for (unsigned int i = 0; i < sizeof(PciBus::ConfigSpace) / 4; i++)
    {
        pCs32[i] = PciBus::instance().readConfigSpace(pDev, i);
    }
}

static const char *getVendor(uint16_t vendor)
{
    for (unsigned int i = 0; i < PCI_VENTABLE_LEN; i++)
    {
        if (PciVenTable[i].VenId == vendor)
            return PciVenTable[i].VenShort;
    }
    return "";
}

static const char *getDevice(uint16_t vendor, uint16_t device)
{
    for (unsigned int i = 0; i < PCI_DEVTABLE_LEN; i++)
    {
        if (PciDevTable[i].VenId == vendor && PciDevTable[i].DevId == device)
            return PciDevTable[i].ChipDesc;
    }
    return "";
}

static bool entry()
{
    for (int iBus = 0; iBus < MAX_BUS; iBus++)
    {
        // Firstly add the ISA bus.
        char *str = new char[256];
        StringFormat(str, "PCI #%d", iBus);
        Bus *pBus = new Bus(str);
        pBus->setSpecificType(String("pci"));

        for (int iDevice = 0; iDevice < 32; iDevice++)
        {
            bool bIsMultifunc = false;
            for (int iFunc = 0; iFunc < 8; iFunc++)
            {
                if (iFunc > 0 && !bIsMultifunc)
                    break;

                Device *pDevice = new Device();
                pDevice->setPciPosition(iBus, iDevice, iFunc);

                uint32_t vendorAndDeviceId =
                    PciBus::instance().readConfigSpace(pDevice, 0);
                if ((vendorAndDeviceId & 0xFFFF) == 0xFFFF ||
                    (vendorAndDeviceId & 0xFFFF) == 0)
                {
                    delete pDevice;
                    /// \note In some cases there are gaps between functions,
                    /// so it shouldn't just skip the rest of the functions
                    /// left of the current device. eddyb
                    continue;
                    // break;
                }

                PciBus::ConfigSpace cs;
                readConfigSpace(pDevice, &cs);

                if (cs.header_type & 0x80)
                    bIsMultifunc = true;

                NOTICE(
                    "PCI: " << Dec << iBus << ":" << iDevice << ":" << iFunc
                            << "\t Vendor:" << Hex << cs.vendor
                            << " Device:" << cs.device);

                char c[256];
                StringFormat(
                    c, "%s - %s", getDevice(cs.vendor, cs.device),
                    getVendor(cs.vendor));
                pDevice->setSpecificType(String(c));
                NOTICE("PCI:     " << c);
                pDevice->setPciIdentifiers(
                    cs.class_code, cs.subclass, cs.vendor, cs.device,
                    cs.progif);
                NOTICE(
                    "PCI:     Class: " << cs.class_code
                                       << " Subclass: " << cs.subclass
                                       << " ProgIF: " << cs.progif);

                for (int l = 0; l < 6; l++)
                {
                    // PCI-PCI bridges have a different layout for the last 4
                    // BARs: they hold extra data.
                    if ((cs.header_type & 0x7F) == 0x1 && l >= 2)
                    {
                        break;
                    }

                    if (cs.bar[l] == 0)
                        continue;

                    // Write the BAR with FFFFFFFF to discover the size of
                    // mapping that the device requires.
                    uint8_t offset = (0x10 + l * 4) >> 2;
                    PciBus::instance().writeConfigSpace(
                        pDevice, offset, 0xFFFFFFFF);
                    uint32_t mask =
                        PciBus::instance().readConfigSpace(pDevice, offset);
                    PciBus::instance().writeConfigSpace(
                        pDevice, offset, cs.bar[l]);

                    // Now work out how much space is required to fill that
                    // mask. Assume it doesn't need 4GB of space...
                    uint32_t size = ~(mask & 0xFFFFFFF0) +
                                    1;  // AND with ~0xF to get rid of the flags
                                        // field in the bottom 4 bits.

                    bool io = (cs.bar[l] & 0x1);
                    if (io)
                        // IO space is only 64K in size.
                        size &= 0xFFFF;

                    StringFormat(c, "bar%d", l);
                    uintptr_t s = (cs.bar[l] & 0xFFFFFFF0);

                    NOTICE(
                        "PCI:     BAR" << Dec << l << Hex << ": " << s << ".."
                                       << (s + size) << " (" << io << ")");
                    Device::Address *pAddress = new Device::Address(
                        String(c), cs.bar[l] & 0xFFFFFFF0, size,
                        (cs.bar[l] & 0x1) == 0x1);
                    pDevice->addresses().pushBack(pAddress);
                }

                NOTICE(
                    "PCI:     IRQ: L" << cs.interrupt_line << " P"
                                      << cs.interrupt_pin);
                pDevice->setInterruptNumber(cs.interrupt_line);
                pBus->addChild(pDevice);
                pDevice->setParent(pBus);

                pDevice->setPciConfigHeader(cs);
            }
        }

        // If the bus was actually populated...
        if (pBus->getNumChildren() > 0)
        {
            Device::addToRoot(pBus);
        }
        else
        {
            delete pBus;
        }
    }

    return true;
}

static void exit()
{
}

MODULE_INFO("pci", &entry, &exit);
