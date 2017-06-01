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

#include "NetManager.h"
#include "TcpManager.h"
#include <Log.h>
#include <process/Scheduler.h>
#include <syscallError.h>
#include <utilities/TimeoutGuard.h>

TcpEndpoint::TcpEndpoint()
    : ConnectionBasedEndpoint(), m_Card(0), m_ConnId(0), m_RemoteHost(),
      nBytesRemoved(0), m_Listening(false), m_IncomingConnections(),
      m_IncomingConnectionCount(0), cond(), m_bConnected(false),
      m_IncomingConnectionLock(), m_DataStream(), m_ShadowDataStream()
{
    m_bConnection = true;
}
TcpEndpoint::TcpEndpoint(uint16_t local, uint16_t remote)
    : ConnectionBasedEndpoint(local, remote), m_Card(0), m_ConnId(0),
      m_RemoteHost(), nBytesRemoved(0), m_Listening(false),
      m_IncomingConnections(), m_IncomingConnectionCount(0), cond(),
      m_bConnected(false), m_IncomingConnectionLock(), m_DataStream(),
      m_ShadowDataStream()
{
    m_bConnection = true;
}
TcpEndpoint::TcpEndpoint(IpAddress remoteIp, uint16_t local, uint16_t remote)
    : ConnectionBasedEndpoint(remoteIp, local, remote), m_Card(0), m_ConnId(0),
      m_RemoteHost(), nBytesRemoved(0), m_Listening(false),
      m_IncomingConnections(), m_IncomingConnectionCount(0), cond(),
      m_bConnected(false), m_IncomingConnectionLock(), m_DataStream(),
      m_ShadowDataStream()
{
    m_bConnection = true;
}
TcpEndpoint::TcpEndpoint(
    size_t connId, IpAddress remoteIp, uint16_t local, uint16_t remote)
    : ConnectionBasedEndpoint(remoteIp, local, remote), m_Card(0),
      m_ConnId(connId), m_RemoteHost(), nBytesRemoved(0), m_Listening(false),
      m_IncomingConnections(), m_IncomingConnectionCount(0), cond(),
      m_bConnected(false), m_IncomingConnectionLock(), m_DataStream(),
      m_ShadowDataStream()
{
    m_bConnection = true;
}

TcpEndpoint::~TcpEndpoint() = default;

ConnectionBasedEndpoint::EndpointState TcpEndpoint::state() const
{
    auto state = TcpManager::instance().getState(m_ConnId);
    switch (state)
    {
        case Tcp::LISTEN:
            return ConnectionBasedEndpoint::LISTENING;
        case Tcp::SYN_SENT:
        case Tcp::SYN_RECEIVED:
            return ConnectionBasedEndpoint::CONNECTING;
        case Tcp::ESTABLISHED:
            return ConnectionBasedEndpoint::TRANSFER;
        case Tcp::FIN_WAIT_1:
        case Tcp::FIN_WAIT_2:
        case Tcp::CLOSE_WAIT:
        case Tcp::CLOSING:
            return ConnectionBasedEndpoint::CLOSING;
        case Tcp::LAST_ACK:
        case Tcp::TIME_WAIT:
            return ConnectionBasedEndpoint::CLOSED;
        case Tcp::IGNORED:
        case Tcp::UNKNOWN:
        default:
            return ConnectionBasedEndpoint::UNKNOWN;
    }
}

Endpoint *TcpEndpoint::accept()
{
    if (TcpManager::instance().getState(m_ConnId) != Tcp::LISTEN)
    {
        ERROR("TcpEndpoint::accept() called but not in the LISTEN state!?");
        return 0;
    }

    // acquire() will return true when there is at least one connection waiting
    m_IncomingConnectionLock.acquire();
    while (!m_IncomingConnectionCount)
    {
        cond.wait(m_IncomingConnectionLock);
    }

    Endpoint *e = m_IncomingConnections.popFront();
    m_IncomingConnectionLock.release();

    return e;
}

bool TcpEndpoint::listen()
{
    // Is the state OK for this?
    if (m_bConnected)
    {
        // Connection exists, can't listen.
        /// \todo bubble up an error
        return false;
    }

    /// \todo Interface-specific connections
    m_IncomingConnections.clear();
    m_ConnId = TcpManager::instance().Listen(this, getLocalPort());
    m_Listening = true;

    /// \todo Listen() could fail?
    return true;
}

bool TcpEndpoint::connect(
    const Endpoint::RemoteEndpoint &remoteHost, bool bBlock)
{
    setRemoteHost(remoteHost);
    setRemotePort(remoteHost.remotePort);
    if (getLocalPort() == 0)
    {
        uint16_t port = TcpManager::instance().allocatePort();
        setLocalPort(port);
        if (getLocalPort() == 0)
            return false;
    }
    m_ConnId = TcpManager::instance().Connect(
        m_RemoteHost, getLocalPort(), this, bBlock);
    if (m_ConnId == 0)
        WARNING("TcpEndpoint::connect: got 0 for the connection id");
    return (m_ConnId != 0);  /// \todo Error codes
}

void TcpEndpoint::close()
{
    TcpManager::instance().Disconnect(m_ConnId);
}

int TcpEndpoint::send(size_t nBytes, uintptr_t buffer)
{
    /// \todo Send needs to return an error code or something
    return TcpManager::instance().send(m_ConnId, buffer, false, nBytes);
};

int TcpEndpoint::recv(uintptr_t buffer, size_t maxSize, bool bBlock, bool bPeek)
{
    if ((!buffer || !maxSize) && !bPeek)
        return -1;

    bool queueReady = false;
    queueReady = dataReady(bBlock);
    if (queueReady)
    {
        // Lock the data stream so we don't end up with a nasty race where
        // another thread modifies or removes the buffer from us.
        /*Mutex *lock = m_DataStream.getLock();
        lock->acquire();

        // Read off the front
        uintptr_t front = m_DataStream.getBuffer(0, false);

        // How many bytes to read?
        size_t nBytes = maxSize;
        size_t streamSize = m_DataStream.getSize(false);
        if (nBytes > streamSize)
            nBytes = streamSize;

        // Copy
        MemoryCopy(reinterpret_cast<void*>(buffer),
        reinterpret_cast<void*>(front), nBytes);

        // Remove from the buffer, we've read
        if(!bPeek)
            m_DataStream.remove(0, nBytes, false);

        // We've read in this block
        lock->release();
        return nBytes;*/

        return m_DataStream.read(buffer, maxSize, bPeek);
    }

    // no data is available - EOF?
    if (TcpManager::instance().getState(m_ConnId) > Tcp::FIN_WAIT_2)
    {
        return 0;
    }
    else
    {
        SYSCALL_ERROR(NoMoreProcesses);
        return -1;
    }
};

void TcpEndpoint::setRemoteHost(const Endpoint::RemoteEndpoint &host)
{
    m_RemoteHost = host;
    setRemoteIp(host.ip);
    setRemotePort(host.remotePort);
}

uint32_t TcpEndpoint::getConnId() const
{
    return m_ConnId;
}

size_t TcpEndpoint::depositTcpPayload(
    size_t nBytes, uintptr_t payload, uint32_t sequenceNumber, bool push)
{
    if (nBytes > 0xFFFF)
    {
        WARNING("Dud length passed to depositTcpPayload!");
        return 0;
    }

    /// \todo this is wrong but we also don't correctly handle non-PSH packets
    /// which should eventually be bubbled up to the application
    push = true;

    // If there's data to add to the shadow stream, add it now. Then, if the
    // PUSH flag is set, copy the shadow stream into the main stream. By
    // allowing a zero-byte deposit, data that did not have the PSH flag can be
    // pushed to the application when we receive a FIN.
    size_t ret = 0;
    if (nBytes && payload)
        ret = m_ShadowDataStream.write(payload, nBytes);
    if (push && m_ShadowDataStream.getDataSize())
    {
        // Copy the buffer into the real data stream
        /// \bug Caveat here: If the shadow stream read is successful, but the
        /// write
        ///      to the data stream is not, the data will be lost without any
        ///      method of regaining access to it! The shadow stream needs to be
        ///      read without removing bytes from it initially, and then if all
        ///      goes well the bytes that were written to the data stream should
        ///      be removed.
        size_t sz = m_ShadowDataStream.getDataSize();
        if (sz > m_ShadowDataStream.getSize())
        {
            ERROR(
                "TCP: Shadow data stream has potentially been corrupted: "
                << sz << ", " << m_ShadowDataStream.getSize() << "!");
            return 0;
        }
        uint8_t *buff = new uint8_t[sz];
        sz = m_ShadowDataStream.read(reinterpret_cast<uintptr_t>(buff), sz);
        size_t o = m_DataStream.write(reinterpret_cast<uintptr_t>(buff), sz);
        delete[] buff;

        if (!o)
            DEBUG_LOG("TCP: wrote zero bytes to a data stream!");

        // Data has arrived!
        for (List<Socket *>::Iterator it = m_Sockets.begin();
             it != m_Sockets.end(); ++it)
        {
            (*it)->endpointStateChanged();
        }
    }

    return ret;
}

bool TcpEndpoint::dataReady(bool block, uint32_t tmout)
{
    if (TcpManager::instance().getState(m_ConnId) == Tcp::LISTEN)
    {
        if (block)
        {
            // Wait for incoming connection.
            bool bRet = false;
            LockGuard<Mutex> guard(m_IncomingConnectionLock);
            if (m_IncomingConnectionCount > 0)
            {
                return true;
            }

            while (!m_IncomingConnectionCount)
            {
                if (!cond.wait(
                        m_IncomingConnectionLock,
                        tmout * Time::Multiplier::SECOND))
                {
                    return false;
                }
            }

            return true;
        }
        else
        {
            LockGuard<Mutex> guard(m_IncomingConnectionLock);
            return m_IncomingConnections.count() > 0;
        }
    }

    if (block)
    {
        TimeoutGuard guard(tmout);
        if (!guard.timedOut())
        {
            bool ret = false;
            while (true)
            {
                if (m_DataStream.getDataSize() != 0)
                {
                    ret = true;
                    break;
                }

                // If there's no more data in the stream, and we need to close,
                // do it You'd think the above would handle this, but timing is
                // an awful thing to assume Much testing has led to the addition
                // of the stream size check
                if (TcpManager::instance().getState(m_ConnId) >
                        Tcp::FIN_WAIT_2 &&
                    (m_DataStream.getDataSize() == 0))
                {
                    break;
                }

                // Yield control otherwise we're using up all the CPU time here
                Scheduler::instance().yield();
            }
            return ret;
        }
        else
            return false;
    }
    else
    {
        return (m_DataStream.getDataSize() != 0);
    }
};

void TcpEndpoint::setCard(Network *pCard)
{
    m_Card = pCard;
}

void TcpEndpoint::addIncomingConnection(TcpEndpoint *conn)
{
    if (conn)
    {
        {
            LockGuard<Mutex> guard(m_IncomingConnectionLock);
            m_IncomingConnections.pushBack(static_cast<Endpoint *>(conn));
            ++m_IncomingConnectionCount;
            cond.signal();
        }

        // Wake up listeners that might be waiting for an incoming connection.
        /// \todo This is a hack - fix stateChanged to be aware of more states.
        stateChanged(Tcp::ESTABLISHED);
    }
}

bool TcpEndpoint::shutdown(ShutdownType what)
{
    if (what == ShutSending)
    {
        // No longer able to write!
        NOTICE(
            "TCP: Endpoint " << reinterpret_cast<uintptr_t>(this)
                             << ": no longer able to send!");
        TcpManager::instance().Shutdown(m_ConnId);
        return true;
    }
    else if (what == ShutReceiving)
    {
        NOTICE(
            "TCP: Endpoint " << reinterpret_cast<uintptr_t>(this)
                             << ": no longer able to receive!");
        TcpManager::instance().Shutdown(m_ConnId, true);
        return true;
    }
    else if (what == ShutBoth)
    {
        // Standard shutdown, don't block
        TcpManager::instance().Shutdown(m_ConnId);
        return true;
    }
    return false;
}

void TcpEndpoint::stateChanged(Tcp::TcpState newState)
{
    // If we've moved into a data transfer state, notify the socket.
    if (newState == Tcp::ESTABLISHED || newState == Tcp::CLOSED)
    {
        // If we've moved into a data transfer state, getsockopt() can report
        // something other than InProgress now.
        if (newState == Tcp::ESTABLISHED)
        {
            resetError();
        }

        for (List<Socket *>::Iterator it = m_Sockets.begin();
             it != m_Sockets.end(); ++it)
        {
            (*it)->endpointStateChanged();
        }
    }
}
