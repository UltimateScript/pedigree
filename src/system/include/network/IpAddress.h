/*
 * Copyright (c) 2010 Matthew Iselin, Heights College
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
#ifndef IP_ADDRESS_H
#define IP_ADDRESS_H

#include <processor/types.h>
#include <utilities/utility.h>

#include <utilities/StaticString.h>
#include <utilities/String.h>

/** An IPv4/IPv6 address */
class IpAddress
{
    public:

    /** The type of an IP address */
    enum IpType
    {
        IPv4 = 0x0800,
        IPv6 = 0x86DD
    };

    /** Constructors */
    IpAddress() :
        m_Type(IPv4), m_Ipv4(0), m_Ipv6(), m_bSet(false)
    {};
    IpAddress(IpType type) :
        m_Type(type), m_Ipv4(0), m_Ipv6(), m_bSet(false)
    {};
    IpAddress(uint32_t ipv4) :
        m_Type(IPv4), m_Ipv4(ipv4), m_Ipv6(), m_bSet(true)
    {};
    IpAddress(uint8_t *ipv6) :
        m_Type(IPv6), m_Ipv4(0), m_Ipv6(), m_bSet(true)
    {
        memcpy(m_Ipv6, ipv6, 16);
    };

    /** IP setters - only one type is valid at any one point in time */

    void setIp(uint32_t ipv4)
    {
        m_Ipv4 = ipv4;
        memset(m_Ipv6, 0, 16);
        m_bSet = true;
        m_Type = IPv4;
    }

    void setIp(uint8_t *ipv6)
    {
        m_Ipv4 = 0;
        memcpy(m_Ipv6, ipv6, 16);
        m_bSet = true;
        m_Type = IPv6;
    }

    /** IP getters */

    inline uint32_t getIp()
    {
        return m_Ipv4;
    }

    inline void getIp(uint8_t *ipv6)
    {
        if(ipv6)
            memcpy(ipv6, m_Ipv6, 16);
    }

    /** Type getter */

    IpType getType()
    {
        return m_Type;
    }

    /** Operators */

    IpAddress operator + (IpAddress a)
    {
        if(a.getType() != m_Type || m_Type == IPv6 || a.getType() == IPv6)
            return IpAddress();
        else
        {
            IpAddress ret(getIp() + a.getIp());
            return ret;
        }
    }

    IpAddress operator & (IpAddress a)
    {
        if(a.getType() != m_Type || m_Type == IPv6 || a.getType() == IPv6)
            return IpAddress();
        else
        {
            IpAddress ret(getIp() & a.getIp());
            return ret;
        }
    }

    IpAddress& operator = (IpAddress a)
    {
        if(a.getType() == IPv6)
        {
            a.getIp(m_Ipv6);
            m_Ipv4 = 0;
            m_Type = IPv6;
            m_bSet = true;
        }
        else
            setIp(a.getIp());
        return *this;
    }

    bool operator == (IpAddress a)
    {
        if(a.getType() == IPv4)
            return (a.getIp() == getIp());
        else
        {
            return !memcmp(m_Ipv6, a.m_Ipv6, 16);
        }
    }

    bool operator != (IpAddress a)
    {
        if(a.getType() == IPv4)
            return (a.getIp() != getIp());
        else
        {
            return !memcmp(m_Ipv6, a.m_Ipv6, 16);
        }
    }

    bool operator == (uint32_t a)
    {
        if(getType() == IPv4)
            return (a == getIp());
        else
            return false;
    }

    bool operator != (uint32_t a)
    {
        if(getType() != IPv4)
            return (a != getIp());
        else
            return false;
    }

    String toString();

    /// Whether the IP address is a valid multicast address.
    bool isMulticast();

    /// Whether the IP address is a valid unicast address.
    inline bool isUnicast() { return !isMulticast(); }

    private:

        IpType      m_Type;

        uint32_t    m_Ipv4; // the IPv4 address
        uint8_t     m_Ipv6[16]; // the IPv6 address

        bool        m_bSet; // has the IP been set yet?
};

#endif

