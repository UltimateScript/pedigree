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
#include "modules/Module.h"
#include "pedigree/kernel/machine/Device.h"
#include "pedigree/kernel/machine/openfirmware/Device.h"
#include "pedigree/kernel/processor/Processor.h"
#include "pedigree/kernel/processor/types.h"

static void searchNode(Device *pDev)
{
    for (unsigned int i = 0; i < pDev->getNumChildren(); i++)
    {
        Device *pChild = pDev->getChild(i);

        /// \todo also convert to Controllers.
        if (pChild->getSpecificType() == "pci-ide")
        {
            // We've found a native-pci ATA device.

            // BAR0 is the command register address, BAR1 is the control
            // register address.
            for (int j = 0; j < pChild->addresses().count(); j++)
            {
                if (pChild->addresses()[j]->m_Name == "bar0")
                    pChild->addresses()[j]->m_Name = String("command");
                if (pChild->addresses()[j]->m_Name == "bar1")
                    pChild->addresses()[j]->m_Name = String("control");
            }
        }

        if (pChild->getSpecificType() == "ata")
        {
            OFDevice dev(pChild->getOFHandle());
            NormalStaticString compatible;
            dev.getProperty("compatible", compatible);

            if (compatible == "keylargo-ata")
            {
                // iBook's keylargo controller.

                // The reg property gives in the first 32-bits the offset into
                // the parent's BAR0 (mac-io) of all our registers.
                uintptr_t reg;
                dev.getProperty("reg", &reg, 4);

                for (unsigned int j = 0;
                     j < pChild->getParent()->addresses().count(); j++)
                {
                    if (pChild->getParent()->addresses()[j]->m_Name == "bar0")
                    {
                        reg += pChild->getParent()->addresses()[j]->m_Address;
                        break;
                    }
                }

                pChild->addresses().pushBack(new Device::Address(
                    String("command"), reg, 0x160, false, /* Not IO */
                    0x10));                               /* Padding of 16 */
                pChild->addresses().pushBack(new Device::Address(
                    String("control"), reg + 0x160, /* From linux source. */
                    0x100, false,                   /* Not IO */
                    0x10));                         /* Padding of 16 */
            }
        }

        // Recurse.
        searchNode(pChild);
    }
}

bool entry()
{
    Device *pDev = &Device::root();
    searchNode(pDev);

    /// \todo return false if nothing found
    return true;
}

void exit()
{
}

MODULE_NAME("ata-specific");
MODULE_ENTRY(&entry);
MODULE_EXIT(&exit);
MODULE_DEPENDS(0);
