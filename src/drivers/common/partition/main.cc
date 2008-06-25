/*
 * Copyright (c) 2008 James Molloy, Jörg Pfähler, Matthew Iselin
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
#include <machine/Device.h>
#include <machine/Disk.h>
#include "msdos.h"
#include <Log.h>

void probeDevice(Disk *pDev)
{
  // Does the disk have an MS-DOS partition table?
  if (msdosProbeDisk(pDev))
    return;
  
  // No? how about an Apple_Map?
  //if (appleProbeDisk(pDev))
    //return;
  
  // Oh well, better luck next time.
}

void searchNode(Device *pDev)
{
  for (unsigned int i = 0; i < pDev->getNumChildren(); i++)
  {
    Device *pChild = pDev->getChild(i);
    // Is this a disk?
    if (pChild->getType() == Device::Disk)
    {
      // Check that none of its children are Partitions (in which case we've probed this before!)
      bool hasPartitions = false;
      for (unsigned int i = 0; i < pChild->getNumChildren(); i++)
      {
        String name;
        pDev->getChild(i)->getName(name);
        if (!strcmp(name, "msdos-partition") || !strcmp(name, "apple-partition"))
        {
          hasPartitions = true;
          break;
        }
      }
      if (!hasPartitions)
        probeDevice(reinterpret_cast<Disk*> (pChild));
    }
    // Recurse.
    searchNode(pChild);
  }
}

void entry()
{
  // Walk the device tree looking for disks that don't have "partition" children.
  Device *pDev = &Device::root();
  searchNode(pDev);
}

void exit()
{

}

const char *g_pModuleName = "partition";
ModuleEntry g_pModuleEntry = &entry;
ModuleExit  g_pModuleExit  = &exit;
