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

/*
 *  sha1.h
 *
 *  Copyright (C) 1998, 2009
 *  Paul E. Jones <paulej@packetizer.com>
 *  All Rights Reserved.
 *
 *****************************************************************************
 *  $Id: sha1.h 12 2009-06-22 19:34:25Z paulej $
 *****************************************************************************
 *
 *  Description:
 *      This class implements the Secure Hashing Standard as defined
 *      in FIPS PUB 180-1 published April 17, 1995.
 *
 *      Many of the variable names in this class, especially the single
 *      character names, were used because those were the names used
 *      in the publication.
 *
 *      Please read the file sha1.cpp for more information.
 *
 */

#ifndef _SHA1_H_
#define _SHA1_H_

#include "pedigree/kernel/processor/types.h"

class SHA1
{
  public:
    SHA1();
    virtual ~SHA1();

    /*
     *  Re-initialize the class
     */
    void Reset();

    /*
     *  Returns the message digest
     */
    bool Result(unsigned *message_digest_array);

    /*
     *  Provide input to SHA1
     */
    void Input(const unsigned char *message_array, unsigned length);
    void Input(const char *message_array, unsigned length);
    void Input(unsigned char message_element);
    void Input(char message_element);
    SHA1 &operator<<(const char *message_array);
    SHA1 &operator<<(const unsigned char *message_array);
    SHA1 &operator<<(const char message_element);
    SHA1 &operator<<(const unsigned char message_element);

  private:
    /*
     *  Process the next 512 bits of the message
     */
    void ProcessMessageBlock();

    /*
     *  Pads the current message block to 512 bits
     */
    void PadMessage();

    /*
     *  Performs a circular left shift operation
     */
    inline unsigned CircularShift(int bits, unsigned word);

    uint32_t H[5];  // Message digest buffers

    size_t Length_Low;   // Message length in bits
    size_t Length_High;  // Message length in bits

    unsigned char Message_Block[64];  // 512-bit message blocks
    int Message_Block_Index;          // Index into message block array

    bool Computed;   // Is the digest computed?
    bool Corrupted;  // Is the message digest corruped?
};

#endif
