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

#ifndef MACHINE_ENDPOINT_H
#define MACHINE_ENDPOINT_H

#include <utilities/String.h>
#include <utilities/Vector.h>
#include <processor/types.h>
#include <machine/Network.h>
#include <errors.h>

class Socket;

// Forward declaration, because Endpoint is used by ProtocolManager
class ProtocolManager;

/**
 * The Pedigree network stack - Protocol Endpoint
 *
 * Some features are global across Endpoints; they are provided here. Other
 * features are specific to connectionless and connection-based Endpoints,
 * which are handled by separate object definitions.
 */
class Endpoint
{
private:
    Endpoint(const Endpoint &e);
    const Endpoint& operator = (const Endpoint& e);
public:

    /** Type of this endpoint */
    enum EndpointType
    {
        /** Connectionless - UDP, RAW */
        Connectionless = 0,

        /** Connection-based - TCP */
        ConnectionBased
    };

    /** shutdown() parameter */
    enum ShutdownType
    {
        /** Disable sending on the Endpoint. For TCP this entails a FIN. */
        ShutSending,

        /** Disable receiving on the Endpoint. */
        ShutReceiving,

        /** Disable both sides of the Endpoint. */
        ShutBoth
    };

    /** Constructors and destructors */
    Endpoint();
    Endpoint(uint16_t local, uint16_t remote);
    Endpoint(IpAddress remoteIp, uint16_t local = 0, uint16_t remote = 0);
    virtual ~Endpoint();

    /** Endpoint type (implemented by the concrete child classes) */
    virtual EndpointType getType() const = 0;

    /** Access to internal information */
    virtual uint16_t getLocalPort() const;
    virtual uint16_t getRemotePort() const;
    virtual const IpAddress &getLocalIp() const;
    virtual const IpAddress &getRemoteIp() const;

    virtual void setLocalPort(uint16_t port);
    virtual void setRemotePort(uint16_t port);
    virtual void setLocalIp(const IpAddress &local);
    virtual void setRemoteIp(const IpAddress &remote);

    /**
     * Special address type, like stationInfo but with information about
     * the remote host's port for communications.
     */
    struct RemoteEndpoint
    {
        RemoteEndpoint() :
            type(INET), ip(), remotePort(0), localPath()
        {}

        /// Remote endpoint type.
        enum RemoteType
        {
            // INET -> IP addresses
            INET,
            // LOCAL -> paths on local filesystems
            LOCAL,
        } type;

        /// IpAddress will handle IPv6 and IPv4
        IpAddress ip;

        /// Remote host's port
        uint16_t remotePort;

        /// Local path
        String localPath;
    };

    /** Is data ready to receive? */
    virtual bool dataReady(bool block = false, uint32_t timeout = 30);

    /** <Protocol>Manager functionality */
    virtual size_t depositPayload(size_t nBytes, uintptr_t payload, RemoteEndpoint remoteHost);

    /** All endpoint types must provide a shutdown() method that shuts part of the socket */
    virtual bool shutdown(ShutdownType what) = 0;

    /**
     * This would allow an Endpoint to switch cards to utilise the best route
     * for a return packet. I'm not sure if it's used or not.
     */
    virtual void setCard(Network* pCard);

    /** Protocol management */
    ProtocolManager *getManager() const;

    void setManager(ProtocolManager *man);

    /** Connection type */
    bool isConnectionless() const;
    
    /** Adds a socket to the internal list */
    void AddSocket(Socket *s);

    /** Removes a socket from the internal list */
    void RemoveSocket(Socket *s);

    /** Retrieves the most recent error for the endpoint, if any. */
    Error::PosixError getError() const;

    /** Resets the error status of the endpoint. */
    void resetError();

    /** Report an error. */
    void reportError(Error::PosixError e);

protected:

    /** List of sockets linked to this Endpoint */
    List<Socket *> m_Sockets;

private:

    /** Our local port (sourcePort in the UDP header) */
    uint16_t m_LocalPort;

    /** Our destination port */
    uint16_t m_RemotePort;

    /** Local IP. Zero means bound to all. */
    IpAddress m_LocalIp;

    /** Remote IP */
    IpAddress m_RemoteIp;

    /** Protocol manager */
    ProtocolManager *m_Manager;

    /** Error status of the endpoint. */
    Error::PosixError m_Error;

protected:

    /** Connection-based?
      * Because the initialisation for any recv/send action on either type
      * of Endpoint (connected or connectionless) is the same across protocols
      * this can reduce the amount of repeated code.
      */
    bool m_bConnection;
};

#endif
