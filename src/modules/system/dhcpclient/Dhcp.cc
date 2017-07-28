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

#include "modules/Module.h"
#include "modules/system/network-stack/ConnectionlessEndpoint.h"
#include "modules/system/network-stack/NetworkStack.h"
#include "modules/system/network-stack/UdpManager.h"
#include "pedigree/kernel/Log.h"

#include "modules/system/network-stack/Ndp.h"

#include "pedigree/kernel/ServiceManager.h"
#include "pedigree/kernel/processor/Processor.h"

#include "DhcpService.h"

#define MAX_OPTIONS_SIZE \
    (1400 - 28 /* UDP header + IP header */ - 236 /* DhcpPacket size, below */)

/** Defines a DHCP packet (RFC 2131) */
struct DhcpPacket
{
    uint8_t opcode;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
} __attribute__((packed));

#define OP_BOOTREQUEST 1
#define OP_BOOTREPLY 2

enum DhcpMessageTypes
{
    DISCOVER = 1,
    OFFER,
    REQUEST,
    DECLINE,
    ACK,
    NACK,
    RELEASE,
    INFORM
};

/** Defines the beginning of a DHCP option */
struct DhcpOption
{
    uint8_t code;
    uint8_t len;
} __attribute__((packed));

/** Magic cookie (RFC1048 extensions) */
#define MAGIC_COOKIE 0x63538263
struct DhcpOptionMagicCookie
{
    DhcpOptionMagicCookie() : cookie(MAGIC_COOKIE)
    {
    }

    uint32_t cookie;
} __attribute__((packed));

/** Defines the Subnet-Mask DHCP option (code=1) */
#define DHCP_SUBNETMASK 1
struct DhcpOptionSubnetMask
{
    DhcpOptionSubnetMask()
        : code(DHCP_SUBNETMASK), len(4), a1(0), a2(0), a3(0), a4(0)
    {
    }

    uint8_t code;
    uint8_t len;
    uint8_t a1;
    uint8_t a2;
    uint8_t a3;
    uint8_t a4;
} __attribute__((packed));

/** Defines the Default-Gateway DHCP option (code=3) */
#define DHCP_DEFGATEWAY 3
struct DhcpOptionDefaultGateway
{
    DhcpOptionDefaultGateway()
        : code(DHCP_DEFGATEWAY), len(4), a1(0), a2(0), a3(0), a4(0)
    {
    }

    uint8_t code;
    uint8_t len;
    uint8_t a1;
    uint8_t a2;
    uint8_t a3;
    uint8_t a4;
} __attribute__((packed));

#define DHCP_DNSSERVERS 6
struct DhcpOptionDnsServers
{
    DhcpOptionDnsServers() : code(DHCP_DNSSERVERS), len(4)
    {
    }

    uint8_t code;
    uint8_t len;  // following this header are len/4 IP addresses
} __attribute__((packed));

/** Defines the Adddress Request DHCP option (code=50) */
#define DHCP_ADDRREQ 50
struct DhcpOptionAddrReq
{
    DhcpOptionAddrReq() : code(DHCP_ADDRREQ), len(4), a1(0), a2(0), a3(0), a4(0)
    {
    }

    uint8_t code;
    uint8_t len;
    uint8_t a1;
    uint8_t a2;
    uint8_t a3;
    uint8_t a4;
} __attribute__((packed));

/** Defines the Message Type DHCP option (code=53) */
#define DHCP_MSGTYPE 53
struct DhcpOptionMsgType
{
    DhcpOptionMsgType() : code(DHCP_MSGTYPE), len(1), opt(DISCOVER)
    {
    }

    uint8_t code;  // = 0x53
    uint8_t len;   // = 1
    uint8_t opt;
} __attribute__((packed));

/** Defines the Server Identifier DHCP option (code=54) */
#define DHCP_SERVERIDENT 54
struct DhcpOptionServerIdent
{
    DhcpOptionServerIdent()
        : code(DHCP_SERVERIDENT), len(4), a1(0), a2(0), a3(0), a4(0)
    {
    }

    uint8_t code;
    uint8_t len;
    uint8_t a1;
    uint8_t a2;
    uint8_t a3;
    uint8_t a4;
} __attribute__((packed));

/** Defines the Parameter Request DHCP option (code=55) */
#define DHCP_PARAMREQUEST 55
struct DhcpOptionParamRequest
{
    DhcpOptionParamRequest() : code(DHCP_PARAMREQUEST), len(0)
    {
    }

    uint8_t code;
    uint8_t
        len;  // Following this are len bytes, each a valid DHCP option code.
} __attribute__((packed));

/** Defines the End DHCP option (code=0xff) */
#define DHCP_MSGEND 0xff
struct DhcpOptionEnd
{
    DhcpOptionEnd() : code(DHCP_MSGEND)
    {
    }

    uint8_t code;
} __attribute__((packed));

static size_t addOption(void *opt, size_t sz, size_t offset, void *dest)
{
    MemoryCopy(reinterpret_cast<char *>(dest) + offset, opt, sz);
    return offset + sz;
}

static DhcpOption *getNextOption(DhcpOption *opt, size_t *currOffset)
{
    DhcpOption *ret = reinterpret_cast<DhcpOption *>(
        reinterpret_cast<uintptr_t>(opt) +
        (*currOffset));  // skip code and len as well
    (*currOffset) += (opt->len + 2);
    return ret;
}

static Service *pService = 0;
static ServiceFeatures *pFeatures = 0;

static bool dhcpClient(Network *pCard)
{
    StationInfo info = pCard->getStationInfo();

    /// \todo Find a better way to check for the loopback address
    if (info.ipv4 == Network::convertToIpv4(127, 0, 0, 1))
        return false;

    IpAddress broadcast(0xffffffff);
    ConnectionlessEndpoint *e = static_cast<ConnectionlessEndpoint *>(
        UdpManager::instance().getEndpoint(broadcast, 68, 67));

#define BUFFSZ 2048
    uint8_t *buff = new uint8_t[BUFFSZ];

    // Can be used for anything
    DhcpPacket *dhcp;

    enum DhcpState
    {
        DISCOVER_SENT = 0,  // We've sent DHCP DISCOVER
        OFFER_RECVD,        // We've received an offer
        REQUEST_SENT,       // We've sent the request
        ACK_RECVD,          // We've received the final ACK
        UNKNOWN
    } currentState;

    if (e)
    {
        // DHCP DISCOVER to find potential DHCP servers
        e->acceptAnyAddress(true);

        Endpoint::RemoteEndpoint remoteHost;
        remoteHost.remotePort = 67;
        remoteHost.ip = broadcast;

        currentState = DISCOVER_SENT;

        // Zero out and fill the initial packet
        ByteSet(buff, 0, BUFFSZ);
        dhcp = reinterpret_cast<DhcpPacket *>(buff);
        dhcp->opcode = OP_BOOTREQUEST;
        dhcp->htype = 1;  // Ethernet
        dhcp->hlen = 6;   // 6 bytes in a MAC address
        /// \todo use this to track requests and responses
        dhcp->xid = HOST_TO_BIG32(12345);
        MemoryCopy(dhcp->chaddr, info.mac, 6);

        void *optionsStart = adjust_pointer(buff, sizeof(*dhcp));

        DhcpOptionMagicCookie cookie;
        DhcpOptionMsgType msgTypeOpt;
        DhcpOptionParamRequest paramRequest;
        DhcpOptionEnd endOpt;

        msgTypeOpt.opt = DISCOVER;

        size_t byteOffset = 0;
        byteOffset =
            addOption(&cookie, sizeof(cookie), byteOffset, optionsStart);
        byteOffset = addOption(
            &msgTypeOpt, sizeof(msgTypeOpt), byteOffset, optionsStart);

        // Want a subnet mask, router, and DNS server(s)
        const uint8_t paramsWanted[] = {1, 3, 6};

        paramRequest.len = sizeof paramsWanted;

        byteOffset = addOption(
            &paramRequest, sizeof(paramRequest), byteOffset, optionsStart);
        MemoryCopy(
            adjust_pointer(optionsStart, byteOffset), &paramsWanted,
            sizeof paramsWanted);
        byteOffset += sizeof paramsWanted;

        byteOffset =
            addOption(&endOpt, sizeof(endOpt), byteOffset, optionsStart);

        // Throw into the send buffer and send it out
        // MemoryCopy(buff, &dhcp, sizeof(dhcp));
        bool success =
            e->send(
                sizeof(*dhcp) + byteOffset, reinterpret_cast<uintptr_t>(dhcp),
                remoteHost, true, pCard) >= 0;
        if (!success)
        {
            WARNING(
                "DHCP: Couldn't send DHCP DISCOVER packet on an interface!");
            UdpManager::instance().returnEndpoint(e);
            delete[] buff;
            return false;
        }

        // Find and respond to the first DHCP offer

        uint32_t myIpWillBe = 0;
        DhcpOptionServerIdent dhcpServer;

        int n = 0;
        if (e->dataReady(true, 5) == false)
        {
            WARNING(
                "DHCP: Did not receive a reply to DHCP DISCOVER (timed out)!");
            UdpManager::instance().returnEndpoint(e);
            delete[] buff;
            return false;
        }
        while ((n = e->recv(
                    reinterpret_cast<uintptr_t>(buff), BUFFSZ, false,
                    &remoteHost)) > 0)
        {
            DhcpPacket *incoming = reinterpret_cast<DhcpPacket *>(buff);

            if (incoming->opcode != OP_BOOTREPLY)
                continue;

            myIpWillBe = incoming->yiaddr;

            void *incomingOptions = adjust_pointer(incoming, sizeof(*incoming));

            // NOTICE("yiaddr = " << incoming->yiaddr << ", siaddr = " <<
            // incoming->siaddr << ".");

            size_t dhcpSizeWithoutOptions = n - sizeof(DhcpPacket);
            if (dhcpSizeWithoutOptions == 0)
            {
                // No magic cookie, so extensions aren't available
                dhcpServer.a4 = (incoming->siaddr & 0xFF000000) >> 24;
                dhcpServer.a3 = (incoming->siaddr & 0x00FF0000) >> 16;
                dhcpServer.a2 = (incoming->siaddr & 0x0000FF00) >> 8;
                dhcpServer.a1 = (incoming->siaddr & 0x000000FF);
                currentState = OFFER_RECVD;
                break;
            }
            DhcpOptionMagicCookie *thisCookie =
                reinterpret_cast<DhcpOptionMagicCookie *>(incomingOptions);
            if (thisCookie->cookie != MAGIC_COOKIE)
            {
                NOTICE("DHCP: Magic cookie incorrect!");
                break;
            }

            // Check the options for the magic cookie and DHCP OFFER
            byteOffset = 0;
            DhcpOption *opt = 0;
            while (static_cast<int>(
                       byteOffset + sizeof(cookie) + sizeof(DhcpPacket)) < n)
            {
                opt = reinterpret_cast<DhcpOption *>(adjust_pointer(
                    incomingOptions, byteOffset + sizeof(cookie)));
                if (opt->code == DHCP_MSGEND)
                    break;
                else if (opt->code == DHCP_MSGTYPE)
                {
                    DhcpOptionMsgType *p =
                        reinterpret_cast<DhcpOptionMsgType *>(opt);
                    if (p->opt == OFFER)
                        currentState = OFFER_RECVD;
                }
                else if (opt->code == DHCP_SERVERIDENT)
                {
                    DhcpOptionServerIdent *p =
                        reinterpret_cast<DhcpOptionServerIdent *>(opt);
                    dhcpServer = *p;
                }

                byteOffset += opt->len + 2;
            }

            // If YOUR-IP is set and no DHCP Message Type was given, assume this
            // is an offer
            if (myIpWillBe && (currentState != OFFER_RECVD))
                currentState = OFFER_RECVD;
        }

        if (currentState != OFFER_RECVD)
        {
            WARNING("DHCP: Couldn't get a valid offer packet.");
            UdpManager::instance().returnEndpoint(e);
            delete[] buff;
            return false;
        }

        // We want to accept this offer by requesting it from the DHCP server
        ByteSet(buff, 0, BUFFSZ);
        dhcp = reinterpret_cast<DhcpPacket *>(buff);
        dhcp->opcode = OP_BOOTREQUEST;
        dhcp->htype = 1;  // Ethernet
        dhcp->hlen = 6;   // 6 bytes in a MAC address
        /// \todo use this to track requests and responses
        dhcp->xid = HOST_TO_BIG32(12345);
        MemoryCopy(dhcp->chaddr, info.mac, 6);

        currentState = REQUEST_SENT;

        DhcpOptionAddrReq addrReq;

        paramRequest.len = sizeof paramsWanted;

        addrReq.a4 = (myIpWillBe & 0xFF000000) >> 24;
        addrReq.a3 = (myIpWillBe & 0x00FF0000) >> 16;
        addrReq.a2 = (myIpWillBe & 0x0000FF00) >> 8;
        addrReq.a1 = (myIpWillBe & 0x000000FF);

        msgTypeOpt.opt = REQUEST;

        byteOffset = 0;
        byteOffset =
            addOption(&cookie, sizeof(cookie), byteOffset, optionsStart);
        byteOffset = addOption(
            &msgTypeOpt, sizeof(msgTypeOpt), byteOffset, optionsStart);
        byteOffset =
            addOption(&addrReq, sizeof(addrReq), byteOffset, optionsStart);
        byteOffset = addOption(
            &dhcpServer, sizeof(dhcpServer), byteOffset, optionsStart);

        byteOffset = addOption(
            &paramRequest, sizeof(paramRequest), byteOffset, optionsStart);
        MemoryCopy(
            adjust_pointer(optionsStart, byteOffset), &paramsWanted,
            sizeof paramsWanted);
        byteOffset += sizeof paramsWanted;

        byteOffset =
            addOption(&endOpt, sizeof(endOpt), byteOffset, optionsStart);

        // Throw into the send buffer and send it out
        remoteHost.ip = Network::convertToIpv4(
            dhcpServer.a1, dhcpServer.a2, dhcpServer.a3, dhcpServer.a4);
        success =
            e->send(
                sizeof(*dhcp) + byteOffset, reinterpret_cast<uintptr_t>(dhcp),
                remoteHost, false, pCard) >= 0;
        if (!success)
        {
            WARNING("DHCP: Couldn't send DHCP REQUEST packet on an interface!");
            UdpManager::instance().returnEndpoint(e);
            delete[] buff;
            return false;
        }

        // Grab the ACK and update the card

        DhcpOptionSubnetMask subnetMask;
        bool subnetMaskSet = false;

        DhcpOptionDefaultGateway defGateway;
        bool gatewaySet = false;

        // DNS servers
        DhcpOptionDnsServers dnsServs;
        IpAddress *dnsServers = 0;
        size_t nDnsServers = 0;

        if (e->dataReady(true) == false)
        {
            WARNING(
                "DHCP: Did not receive a reply to DHCP REQUEST (timed out)!");
            UdpManager::instance().returnEndpoint(e);
            delete[] buff;
            return false;
        }
        while ((n = e->recv(
                    reinterpret_cast<uintptr_t>(buff), BUFFSZ, false,
                    &remoteHost)) > 0)
        {
            DhcpPacket *incoming = reinterpret_cast<DhcpPacket *>(buff);

            if (incoming->opcode != OP_BOOTREPLY)
                continue;

            myIpWillBe = incoming->yiaddr;

            void *incomingOptions = adjust_pointer(incoming, sizeof(*incoming));

            size_t dhcpSizeWithoutOptions = n - sizeof(DhcpPacket);
            if (dhcpSizeWithoutOptions == 0)
            {
                // no magic cookie, so extensions aren't available
                currentState = ACK_RECVD;
                break;
            }
            DhcpOptionMagicCookie *thisCookie =
                reinterpret_cast<DhcpOptionMagicCookie *>(incomingOptions);
            if (thisCookie->cookie != MAGIC_COOKIE)
            {
                NOTICE("DHCP: Magic cookie incorrect!");
                break;
            }

            // check the options for the magic cookie and DHCP OFFER
            byteOffset = 0;
            DhcpOption *opt = 0;
            while (static_cast<int>(
                       byteOffset + sizeof(cookie) + sizeof(DhcpPacket)) < n)
            {
                opt = reinterpret_cast<DhcpOption *>(adjust_pointer(
                    incomingOptions, byteOffset + sizeof(cookie)));

                if (opt->code == DHCP_MSGEND)
                    break;
                else if (opt->code == DHCP_MSGTYPE)
                {
                    DhcpOptionMsgType *p =
                        reinterpret_cast<DhcpOptionMsgType *>(opt);
                    if (p->opt == ACK)
                        currentState = ACK_RECVD;
                }
                else if (opt->code == DHCP_SUBNETMASK)
                {
                    DhcpOptionSubnetMask *p =
                        reinterpret_cast<DhcpOptionSubnetMask *>(opt);
                    subnetMask = *p;
                    subnetMaskSet = true;
                }
                else if (opt->code == DHCP_DEFGATEWAY)
                {
                    DhcpOptionDefaultGateway *p =
                        reinterpret_cast<DhcpOptionDefaultGateway *>(opt);
                    defGateway = *p;
                    gatewaySet = true;
                }
                else if (opt->code == DHCP_DNSSERVERS)
                {
                    DhcpOptionDnsServers *p =
                        reinterpret_cast<DhcpOptionDnsServers *>(opt);
                    dnsServs = *p;

                    nDnsServers = p->len / 4;
                    dnsServers = new IpAddress[nDnsServers];
                    uintptr_t base = reinterpret_cast<uintptr_t>(opt) +
                                     sizeof(DhcpOptionDnsServers);
                    size_t i;
                    for (i = 0; i < nDnsServers; i++)
                    {
                        uint32_t ip = *reinterpret_cast<uint32_t *>(base);
                        dnsServers[i].setIp(ip);
                        base += 4;
                    }
                }

                byteOffset += opt->len + 2;
            }

            // If YOUR-IP is set and no DHCP Message Type was given, assume this
            // is an ack
            if (myIpWillBe && (currentState != ACK_RECVD))
                currentState = ACK_RECVD;
        }

        if (currentState != ACK_RECVD)
        {
            WARNING("DHCP: Couldn't get a valid ack packet.");
            UdpManager::instance().returnEndpoint(e);
            if (dnsServers)
            {
                delete[] dnsServers;
            }
            delete[] buff;
            return false;
        }

        // Set it up
        StationInfo host = pCard->getStationInfo();

        uint32_t ipv4 = Network::convertToIpv4(
            addrReq.a1, addrReq.a2, addrReq.a3, addrReq.a4);
        uint32_t subnet = Network::convertToIpv4(
            subnetMask.a1, subnetMask.a2, subnetMask.a3, subnetMask.a4);

        host.ipv4.setIp(ipv4);

        if (subnetMaskSet)
            host.subnetMask.setIp(subnet);
        else
            host.subnetMask.setIp(Network::convertToIpv4(
                255, 255, 255, 0));  /// \todo Just fail configuration instead
        if (gatewaySet)
            host.gateway.setIp(Network::convertToIpv4(
                defGateway.a1, defGateway.a2, defGateway.a3, defGateway.a4));
        else
            host.gateway.setIp(Network::convertToIpv4(
                192, 168, 0, 1));  /// \todo Autoconfiguration IPv4 address

        if (dnsServers)
        {
            host.dnsServers = dnsServers;
            host.nDnsServers = nDnsServers;
        }
        else
        {
            host.dnsServers = 0;
            host.nDnsServers = 0;
        }

        // The number of hosts we can get on this network is found by swapping
        // bits in the subnet mask (ie, 255.255.255.0 becomes 0.0.0.255).
        uint32_t numHosts = subnet ^ 0xFFFFFFFF;

        // Then we take the IP we've been given, AND against the subnet to get
        // the bottom of the IP range, and then add the number of hosts to find
        // the broadcast address.
        uint32_t subnetBroadcast = (ipv4 & subnet) + numHosts;
        host.broadcast.setIp(subnetBroadcast);

        UdpManager::instance().returnEndpoint(e);
        delete[] buff;

        return pCard->setStationInfo(host);
    }

    delete[] buff;
    return false;
}

bool DhcpService::serve(ServiceFeatures::Type type, void *pData, size_t dataLen)
{
    if (!pData)
        return false;

    // Correct type?
    if (pFeatures->provides(type))
    {
        // We only provide Touch services
        if (type & ServiceFeatures::touch)
        {
            Network *pCard = static_cast<Network *>(pData);
            return dhcpClient(pCard);
        }
    }

    // Not provided by us, fail!
    return false;
}

static bool entry()
{
    // Install the DHCP Service
    pService = new DhcpService;
    pFeatures = new ServiceFeatures;
    pFeatures->add(ServiceFeatures::touch);
    ServiceManager::instance().addService(String("dhcp"), pService, pFeatures);

    return true;
}

static void exit()
{
}

MODULE_INFO("dhcpclient", &entry, &exit, "network-stack");
