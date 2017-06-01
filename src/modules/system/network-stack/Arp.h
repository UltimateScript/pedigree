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

#ifndef MACHINE_ARP_H
#define MACHINE_ARP_H

#include <machine/Machine.h>
#include <machine/Network.h>
#include <process/Semaphore.h>
#include <processor/state.h>
#include <processor/types.h>
#include <utilities/RequestQueue.h>
#include <utilities/String.h>
#include <utilities/Tree.h>
#include <utilities/Vector.h>

#include "Ethernet.h"
#include "NetworkStack.h"

#define ARP_OP_REQUEST 0x0001
#define ARP_OP_REPLY 0x0002

/**
 * The Pedigree network stack - ARP layer
 */
class Arp : public RequestQueue
{
  public:
    Arp();
    virtual ~Arp();

    /** For access to the stack without declaring an instance of it */
    static Arp &instance()
    {
        return arpInstance;
    }

    /** Packet arrival callback */
    void
    receive(size_t nBytes, uintptr_t packet, Network *pCard, uint32_t offset);

    /** Sends an ARP request */
    void send(IpAddress req, Network *pCard = 0);

    /** Gets an entry from the ARP cache, and optionally resolves it if needed.
     */
    bool
    getFromCache(IpAddress ip, bool resolve, MacAddress *ent, Network *pCard);

    /** Direct cache manipulation */
    bool isInCache(IpAddress ip);
    void insertToCache(IpAddress ip, MacAddress mac);
    void removeFromCache(IpAddress ip);

  private:
    /** ARP request doer */
    virtual uint64_t executeRequest(
        uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5,
        uint64_t p6, uint64_t p7, uint64_t p8);

    static Arp arpInstance;

    struct arpHeader
    {
        uint16_t hwType;
        uint16_t protocol;
        uint8_t hwSize;
        uint8_t protocolSize;
        uint16_t opcode;
        uint16_t hwSrc[3];
        uint32_t ipSrc;
        uint16_t hwDest[3];
        uint32_t ipDest;
    } __attribute__((packed));

    // an entry in the arp cache
    /// \todo Will need to store *time* and *type* - time for removing from
    /// cache
    ///       and type for static entries
    struct arpEntry
    {
        arpEntry() : valid(false), ip(), mac()
        {
        }

        bool valid;
        IpAddress ip;
        MacAddress mac;
    };

    // an ARP request we've sent
    class ArpRequest  // : public TimerHandler
    {
      public:
        ArpRequest()
            : destIp(), mac(), cond(), mutex(false), complete(false),
              success(false)
        {
        }

        IpAddress destIp;
        MacAddress mac;

        ConditionVariable cond;
        Mutex mutex;

        bool complete;
        bool success;
    };

    // ARP Cache
    Tree<size_t, arpEntry *> m_ArpCache;

    // ARP request list
    Vector<ArpRequest *> m_ArpRequests;
};

#endif
