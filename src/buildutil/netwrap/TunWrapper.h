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

#ifndef TUNWRAPPER_H
#define TUNWRAPPER_H

#include <machine/Network.h>

class TunWrapper : public Network
{
public:
    TunWrapper();
    TunWrapper(Network *pDev);
    virtual ~TunWrapper();

    virtual Type getType();
    virtual void getName(String &str);
    virtual void dump(String &str);

    /** Sends a given packet through the device.
    * \param nBytes The number of bytes to send.
    * \param buffer A buffer with the packet to send */
    virtual bool send(size_t nBytes, uintptr_t buffer);

    /** Sets station information (such as IP addresses)
    * \param info The information to set as the station info */
    virtual bool setStationInfo(StationInfo info);

    /** Gets station information (such as IP addresses) */
    virtual StationInfo getStationInfo();

    /** Runs the wrapper's main loop with the given descriptor. */
    void run(int fd);

protected:
    int m_Fd;
    StationInfo m_StationInfo;
};

#endif  // TUNWRAPPER_H
