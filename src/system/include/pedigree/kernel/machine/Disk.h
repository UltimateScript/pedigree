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

#ifndef MACHINE_DISK_H
#define MACHINE_DISK_H

#include "pedigree/kernel/compiler.h"
#include "pedigree/kernel/machine/Device.h"
#include "pedigree/kernel/processor/types.h"

class String;

/**
 * A disk is a random access fixed size block device.
 */
class EXPORTED_PUBLIC Disk : public Device
{
  public:
    enum SubType
    {
        ATA = 0,
        ATAPI
    };

    Disk();
    Disk(Device *p);
    virtual ~Disk();

    virtual Type getType();

    virtual SubType getSubType();

    virtual void getName(String &str);

    virtual void dump(String &str);

    /**
     * Read from \p location on disk and return a pointer to it. \p location
     * must be 512 byte aligned. The pointer returned is within a page of
     * cache that maps to 4096 bytes of disk area.
     * \note If the Disk is backed by a Cache, the buffer returned should have
     *       its refcount incremented by one by a successful run of read(). Use
     *       unpin() to indicate you no longer care about the buffer.
     * \param location The offset from the start of the device, in bytes,
     *        to start the read, must be multiple of 512.
     * \return Pointer to writable area of memory containing the data. If the
     *         data is written, the page is marked as dirty and may be written
     *         back to disk at any time (or forced with \c write()
     *         or \c flush() ).
     */
    virtual uintptr_t read(uint64_t location);

    /**
     * This function schedules a cache writeback of the given location.
     * The data to be written back is fetched from the cache (pointer returned
     * by \c read() ).
     * \param location The offset from the start of the device, in bytes, to
     *                 start the write. Must be 512byte aligned.
     */
    virtual void write(uint64_t location);

    /**
     * \brief Sets the page boundary alignment after a specific location on the
     * disk.
     *
     * For example, if one has a partition starting on byte 512, one will
     * probably want 4096-byte reads to be aligned with this (so reading 4096
     * bytes from byte 0 on the partition will create one page of cache and not
     * span two). Without an align point a read of the first sector of a
     * partition starting at byte 512 will have to have a location of 512 rather
     * than 0.
     *
     * Use this function to allow reads to fit into the 4096 byte buffers
     * manipulated in \c read() or \c write() even when location isn't aligned
     * on a 4096 byte boundary.
     */
    virtual void align(uint64_t location);

    /**
     * \brief Gets the size of the disk.
     *
     * This is the size in bytes of the disk. Reads or writes beyond this size
     * will fail.
     */
    virtual size_t getSize() const;

    /**
     * \brief Gets the block size of the disk.
     *
     * This is the native block size with which all reads and writes are
     * performed, regardless of how much data is available to be read/written.
     */
    virtual size_t getBlockSize() const;

    /**
     * \brief Pins a cache page.
     *
     * This allows an upstream user of Disk pages to 'pin' cache pages, causing
     * them to only be freed once all consumers have done an 'unpin'. The pin
     * and unpin semantics allow for memory mappings to be made in a reasonably
     * safe manner, as it can be assumed that the physical page for a particular
     * cache block will not be freed.
     */
    virtual void pin(uint64_t location);

    /**
     * Unpins a cache page (see \c pin() for more information and rationale).
     */
    virtual void unpin(uint64_t location);

    /**
     * \brief Whether or not the cache is critical and cannot be flushed or
     * deleted.
     *
     * Some implementations of this class may provide a Disk that does not
     * actually back onto a writable media, or perhaps sit only in RAM and have
     * no correlation to physical hardware. If cache pages are deleted for these
     * implementations, data may be lost.
     *
     * Note that cache should only be marked "critical" if it is possible to
     * write via an implementation. There is no need to worry about cache pages
     * being deleted on a read-only disk as they will be re-created on the next
     * read (and no written data is lost).
     *
     * This function allows callers that want to delete cache pages to verify
     * that the cache is not critical to the performance of the implementation.
     *
     * \return True if the cache is critical and must not be removed or flushed.
     * False otherwise.
     */
    virtual bool cacheIsCritical();

    /**
     * \brief Flush a cached page to disk.
     *
     * Essentially a no-op if the given location is not actually in
     * cache. Called either by filesystem drivers (on removable disks) or from a
     * central cache manager which handles flushing caches back to the disk on a
     * regular basis.
     *
     * Will not remove the page from cache, that must be done by the caller.
     */
    virtual void flush(uint64_t location);
};

#endif
