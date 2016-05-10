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

#include <Module.h>
#include <processor/types.h>
#include <processor/Processor.h>
#include <machine/Device.h>
#include <machine/Disk.h>
#include <machine/Controller.h>
#include "AtaController.h"
#include "IsaAtaController.h"
#include "PciAtaController.h"
#include <Log.h>

static int nController = 0;

static bool bFound = false;

// Try for a PIIX IDE controller first. We prefer the PIIX as it enables us
// to use DMA (and is a little easier to use for device detection).
static bool bPiixControllerFound = false;
static bool bFallBackISA = false;

static bool allowProbing = false;

Device *probeIsaDevice(Controller *pDev)
{
  // Create a new AtaController device node.
  IsaAtaController *pController = new IsaAtaController(pDev, nController++);

  bFound = true;

  return pController;
}

Device *probePiixController(Device *pDev)
{
  static uint8_t interrupt = 14;

  // Create a new AtaController device node.
  Controller *pDevController = new Controller(pDev);
  uintptr_t intnum = pDevController->getInterruptNumber();
  if(intnum == 0)
  {
      // No valid interrupt, handle
      pDevController->setInterruptNumber(interrupt);
      if(interrupt < 15)
          interrupt++;
      else
      {
          ERROR("PCI IDE: Controller found with no IRQ and IRQs 14 and 15 are already allocated");
          delete pDevController;

          return pDev;
      }
  }

  PciAtaController *pController = new PciAtaController(pDevController, nController++);

  bFound = true;

  return pController;
}

/// Removes the ISA ATA controllers added early in boot
Device *removeIsaAta(Device *dev)
{
    if(dev->getType() == Device::Controller)
    {
        // Get its addresses, and search for "command" and "control".
        bool foundCommand = false;
        bool foundControl = false;
        for (unsigned int j = 0; j < dev->addresses().count(); j++)
        {
            /// \todo Problem with String::operator== - fix.
            if (dev->addresses()[j]->m_Name == "command")
                foundCommand = true;
            if (dev->addresses()[j]->m_Name == "control")
                foundControl = true;
        }

        if (foundCommand && foundControl)
        {
            // Destroy and remove this device.
            return 0;
        }
    }

    return dev;
}

static Device *probeDisk(Device *pDev)
{
    // Check to see if this is an AHCI controller.
    // Class 1 = Mass Storage. Subclass 6 = SATA.
    if((!allowProbing) && (pDev->getPciClassCode() == 0x01 &&
        pDev->getPciSubclassCode() == 0x06))
    {
        // No AHCI support yet, so just log and keep going.
        WARNING("Found a SATA controller of some sort, hoping for ISA fallback.");
    }

    // Look for a PIIX controller
    if(pDev->getPciVendorId() == 0x8086)
    {
        // Okay, the vendor is right, but is it the right device?
        /// \todo ICH controller
        if(((pDev->getPciDeviceId() == 0x1230) ||    // PIIX
            (pDev->getPciDeviceId() == 0x7010) ||    // PIIX3
            (pDev->getPciDeviceId() == 0x7111)) &&   // PIIX4
            (pDev->getPciFunctionNumber() == 1))     // IDE Controller
        {
            if (allowProbing)
            {
              return probePiixController(pDev);
            }
            bPiixControllerFound = true;
        }
    }

    // No PIIX controller found, fall back to ISA
    /// \todo Could also fall back to ICH?
    if(!bPiixControllerFound && bFallBackISA)
    {
        // Is this a controller?
        if (pDev->getType() == Device::Controller)
        {
            // Check it's not an ATA controller already.
            // Get its addresses, and search for "command" and "control".
            bool foundCommand = false;
            bool foundControl = false;
            for (unsigned int j = 0; j < pDev->addresses().count(); j++)
            {
                /// \todo Problem with String::operator== - fix.
                if (pDev->addresses()[j]->m_Name == "command")
                    foundCommand = true;
                if (pDev->addresses()[j]->m_Name == "control")
                    foundControl = true;
            }
            if (allowProbing && foundCommand && foundControl)
                return probeIsaDevice(static_cast<Controller*> (pDev));
        }
    }

    return pDev;
}

static bool entry()
{
  /// \todo this iterates the device tree up to FOUR times.
  /// Needs some more thinking about how to do this better.

  // Walk the device tree looking for controllers that have 
  // "control" and "command" addresses.
  Device::foreach(probeDisk);

  // Done initial probe to find out what exists, action the findings now.
  allowProbing = true;
  if (bPiixControllerFound)
  {
    // Right, we found a PIIX controller. Let's remove the ATA
    // controllers that are created early in the boot (ISA) now
    // so that when we probe the controller we don't run into used
    // ports.
    Device::foreach(removeIsaAta);
    Device::foreach(probeDisk);
  }
  if (!bFound)
  {
    // Try again, allowing ISA devices this time.
    bFallBackISA = true;
    Device::foreach(probeDisk);
  }

  return bFound;
}

static void exit()
{
}

MODULE_INFO("ata", &entry, &exit, "scsi",
#ifdef PPC_COMMON
    "ata-specific"
#elif defined(X86_COMMON)
    "pci"
#else
    0
#endif
    );

