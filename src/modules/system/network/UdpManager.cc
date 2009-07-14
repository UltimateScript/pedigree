/*
 * Copyright (c) 2008 James Molloy, J�rg Pf�hler, Matthew Iselin
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

#include "UdpManager.h"
#include <Log.h>
#include <processor/Processor.h>

UdpManager UdpManager::manager;

int UdpEndpoint::send(size_t nBytes, uintptr_t buffer, RemoteEndpoint remoteHost, bool broadcast, Network* pCard)
{
  bool success = Udp::instance().send(remoteHost.ip, getLocalPort(), remoteHost.remotePort, nBytes, buffer, broadcast, pCard);
  return success ? nBytes : -1;
};

int UdpEndpoint::recv(uintptr_t buffer, size_t maxSize, RemoteEndpoint* remoteHost)
{
  if(m_DataQueue.count())
  {
    DataBlock* ptr = m_DataQueue.popFront();

    size_t nBytes = maxSize;
    bool allRead = false;

    // Only read in this block
    if(nBytes >= ptr->size)
    {
      nBytes = ptr->size;
      allRead = true;
    }

    // And only past the offset
    nBytes -= ptr->offset;

    memcpy(reinterpret_cast<void*>(buffer), reinterpret_cast<void*>(ptr->ptr + ptr->offset), nBytes);

    *remoteHost = ptr->remoteHost;

    // If the entire block has not been read, repush it with the new offset
    if(!allRead)
    {
      ptr->offset = nBytes;
      m_DataQueue.pushFront(ptr);
      return nBytes;
    }
    // Otherwise we're done - free the block and return
    else
    {
      delete reinterpret_cast<uint8_t*>(ptr->ptr);
      delete ptr;
      return nBytes;
    }
  }
  return 0;
};

void UdpEndpoint::depositPayload(size_t nBytes, uintptr_t payload, RemoteEndpoint remoteHost)
{
  /// \note Perhaps nBytes should also have an upper limit check?
  if(!nBytes || !payload)
    return;

  // Do not accept a payload if our receiver is shut down
  if(!m_bCanRecv)
    return;

  // Otherwise, grab the data block and add it to the queue
  uint8_t* data = new uint8_t[nBytes];
  memcpy(data, reinterpret_cast<void*>(payload), nBytes);

  DataBlock* newBlock = new DataBlock;
  newBlock->ptr = reinterpret_cast<uintptr_t>(data);
  newBlock->offset = 0;
  newBlock->size = nBytes;
  newBlock->remoteHost = remoteHost;

  m_DataQueue.pushBack(newBlock);
  m_DataQueueSize.release();
}

bool UdpEndpoint::dataReady(bool block, uint32_t tmout)
{
  bool timedOut = false;
  if(block)
  {
    m_DataQueueSize.acquire(1, tmout);
    if(Processor::information().getCurrentThread()->wasInterrupted())
      timedOut = true;
  }
  else
    return m_DataQueueSize.tryAcquire();
  return !timedOut;
};

void UdpManager::receive(IpAddress from, IpAddress to, uint16_t sourcePort, uint16_t destPort, uintptr_t payload, size_t payloadSize, Network* pCard)
{
  // is there an endpoint for this port?
  Endpoint* e;
  StationInfo cardInfo = pCard->getStationInfo();
  if((e = m_Endpoints.lookup(destPort)) != 0)
  {
    /** Should we pass on the packet? **/
    bool passOn = false;

    // Broadcast, and accepting broadcast?
    if(to.getIp() == 0xffffffff)
      passOn = e->acceptAnyAddress();

    // Not to us, but accepting any address?
    if(to.getIp() != cardInfo.ipv4.getIp())
      passOn = e->acceptAnyAddress();

    // To us!
    else
      passOn = true;

    if(!passOn)
      return;

    Endpoint::RemoteEndpoint host;
    host.ip = from;
    if(sourcePort)
      host.remotePort = sourcePort;
    else
      host.remotePort = destPort;
    e->depositPayload(payloadSize, payload, host);
  }
}

void UdpManager::returnEndpoint(Endpoint* e)
{
  if(e)
  {
    m_Endpoints.remove(e->getLocalPort());
    delete e;
  }
}

Endpoint* UdpManager::getEndpoint(IpAddress remoteHost, uint16_t localPort, uint16_t remotePort)
{
  // Try to find a unique port
  /// \todo Bitmap! So much neater!
  /// \todo Move into a helper function
  if(localPort == 0)
  {
    uint16_t base = 32768;
    while(base < 0xFFFF)
    {
      if(m_Endpoints.lookup(base++) == 0)
        break;
    }
    if(base == 0xFFFF)
      return 0; // no local port available
    localPort = base;
  }

  // Is there an endpoint for this port?
  /// \note If there is already an Endpoint, we do *not* reallocate it.
  /// \todo Can UDP endpoints be shared? Is it a good idea?
  Endpoint* e;
  if((e = m_Endpoints.lookup(localPort)) == 0)
  {
    e = new UdpEndpoint(remoteHost, localPort, remotePort);
    e->setManager(this);

    m_Endpoints.insert(localPort, e);
  }
  return e;
}
