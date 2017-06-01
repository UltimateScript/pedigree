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

#include <Log.h>
#include <machine/Device.h>
#include <machine/DeviceHashTree.h>

#include <utilities/sha1/sha1.h>

DeviceHashTree DeviceHashTree::m_Instance;

DeviceHashTree::DeviceHashTree() : m_bInitialised(false), m_DeviceTree()
{
}

DeviceHashTree::~DeviceHashTree()
{
}

static Device *testDevice(Device *p)
{
    if (p->getType() != Device::Root)
        DeviceHashTree::instance().add(p);

    return p;
}

void DeviceHashTree::fill(Device *root)
{
    Device::foreach (testDevice, root);

    m_bInitialised = true;
}

void DeviceHashTree::add(Device *p)
{
    size_t hash = getHash(p);
    if (m_DeviceTree.lookup(hash))
        return;

    String dump;
    p->dump(dump);

    NOTICE("Device hash for `" << dump << "' is: " << hash << ".");
    m_DeviceTree.insert(hash, p);
}

Device *DeviceHashTree::getDevice(uint32_t hash)
{
    if (!m_bInitialised)
        return 0;
    else
        return m_DeviceTree.lookup(hash);
}

Device *DeviceHashTree::getDevice(String hash)
{
    if (!m_bInitialised)
        return 0;
    else
    {
        uint32_t inthash =
            StringToUnsignedLong(static_cast<const char *>(hash), 0, 16);
        return m_DeviceTree.lookup(inthash);
    }
}

size_t DeviceHashTree::getHash(Device *pChild)
{
    static SHA1 mySha1;

    // Grab the device information
    String name, dump;
    pChild->getName(name);
    pChild->dump(dump);
    uint32_t bus = pChild->getPciBusPosition();
    uint32_t dev = pChild->getPciDevicePosition();
    uint32_t func = pChild->getPciFunctionNumber();

    // Build the string to be hashed
    NormalStaticString theString;
    theString.clear();
    theString += name;
    theString += "-";
    theString += dump;
    theString += "-";
    theString.append(bus);
    theString += ".";
    theString.append(dev);
    theString += ".";
    theString.append(func);

    // Hash the string
    mySha1.Reset();
    mySha1.Input(static_cast<const char *>(theString), theString.length());
    unsigned int digest[5];
    mySha1.Result(digest);

    // Only use 4 bytes of the hash
    return digest[0];
}
