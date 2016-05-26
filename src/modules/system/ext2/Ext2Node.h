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

#ifndef EXT2_NODE_H
#define EXT2_NODE_H

#include "ext2.h"
#include <utilities/Vector.h>
#include "Ext2Filesystem.h"

/** A node in an ext2 filesystem. */
class Ext2Node
{
    friend class Ext2Filesystem;

private:
    /** Copy constructors are hidden - unused! */
    Ext2Node(const Ext2Node &file);
    Ext2Node& operator =(const Ext2Node&);
public:
    /** Constructor, should be called only by a Filesystem. */
    Ext2Node(uintptr_t inode_num, Inode *pInode, class Ext2Filesystem *pFs);
    /** Destructor */
    virtual ~Ext2Node();

    Inode *getInode()
    {return m_pInode;}

    uint32_t getInodeNumber()
    {return m_InodeNumber;}

    /** Updates inode attributes. */
    void fileAttributeChanged(size_t size, size_t atime, size_t mtime, size_t ctime);

    /** Updates inode metadata. */
    void updateMetadata(uint16_t uid, uint16_t gid, uint32_t perms);

    /** Wipes the node of data - frees all blocks. */
    void wipe();

    void extend(size_t newSize);

    uintptr_t readBlock(uint64_t location);
    void writeBlock(uint64_t location);

    void trackBlock(uint32_t block);

    void pinBlock(uint64_t location);
    void unpinBlock(uint64_t location);

    void sync(size_t offset, bool async);

protected:
    /** Ensures the inode is at least 'size' big. */
    bool ensureLargeEnough(size_t size);

    bool addBlock(uint32_t blockValue);

    bool ensureBlockLoaded(size_t nBlock);
    bool getBlockNumber(size_t nBlock);
    bool getBlockNumberIndirect(uint32_t inode_block, size_t nBlocks, size_t nBlock);
    bool getBlockNumberBiindirect(uint32_t inode_block, size_t nBlocks, size_t nBlock);
    bool getBlockNumberTriindirect(uint32_t inode_block, size_t nBlocks, size_t nBlock);

    bool setBlockNumber(size_t blockNum, uint32_t blockValue);

    uint32_t modeToPermissions(uint32_t mode) const;
    uint32_t permissionsToMode(uint32_t permissions) const;

    Inode *m_pInode;
    uint32_t m_InodeNumber;
    class Ext2Filesystem *m_pExt2Fs;

    uint32_t *m_pBlocks;
    uint32_t m_nBlocks;
    uint32_t m_nMetadataBlocks;

    size_t m_nSize;
};

#endif
