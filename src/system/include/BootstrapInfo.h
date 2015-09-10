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

#ifndef KERNEL_BOOTSTRAPINFO_H
#define KERNEL_BOOTSTRAPINFO_H

#include <compiler.h>
#include <processor/types.h>

/** @addtogroup kernel
 * @{ */

// Again, if we're passed via grub these multiboot #defines will be valid, otherwise they won't.
#if defined(KERNEL_STANDALONE) || defined(MIPS_COMMON) || defined(ARM_COMMON)
#define MULTIBOOT_FLAG_MEM     0x001
#define MULTIBOOT_FLAG_DEVICE  0x002
#define MULTIBOOT_FLAG_CMDLINE 0x004
#define MULTIBOOT_FLAG_MODS    0x008
#define MULTIBOOT_FLAG_AOUT    0x010
#define MULTIBOOT_FLAG_ELF     0x020
#define MULTIBOOT_FLAG_MMAP    0x040
#define MULTIBOOT_FLAG_CONFIG  0x080
#define MULTIBOOT_FLAG_LOADER  0x100
#define MULTIBOOT_FLAG_APM     0x200
#define MULTIBOOT_FLAG_VBE     0x400
#endif

class BaseBootstrapStruct
{
public:
    virtual bool isInitrdLoaded() const
    {
        return false;
    }

    virtual uint8_t *getInitrdAddress() const
    {
        return 0;
    }

    virtual size_t getInitrdSize() const
    {
        return 0;
    }

    virtual bool isDatabaseLoaded() const
    {
        return false;
    }

    virtual uint8_t *getDatabaseAddress() const
    {
        return 0;
    }

    virtual size_t getDatabaseSize() const
    {
        return 0;
    }

    virtual char *getCommandLine() const
    {
        return 0;
    }

    virtual size_t getSectionHeaderCount() const
    {
        return 0;
    }

    virtual size_t getSectionHeaderEntrySize() const
    {
        return 0;
    }

    virtual size_t getSectionHeaderStringTableIndex() const
    {
        return 0;
    }

    virtual uintptr_t getSectionHeaders() const
    {
        return 0;
    }

    virtual void *getMemoryMap() const
    {
        return 0;
    }

    virtual uint64_t getMemoryMapEntryAddress(void *opaque) const
    {
        return 0;
    }

    virtual uint64_t getMemoryMapEntryLength(void *opaque) const
    {
        return 0;
    }

    virtual uint32_t getMemoryMapEntryType(void *opaque) const
    {
        return 0;
    }

    virtual void *nextMemoryMapEntry(void *opaque) const
    {
        return 0;
    }

    virtual size_t getModuleCount() const
    {
        return 0;
    }

    virtual void *getModuleBase() const
    {
        return 0;
    }
};

#ifndef PPC_COMMON

class BootstrapStruct_t : public BaseBootstrapStruct
{
public:
    virtual bool isInitrdLoaded() const;
    virtual uint8_t *getInitrdAddress() const;
    virtual size_t getInitrdSize() const;

    virtual bool isDatabaseLoaded() const;
    virtual uint8_t *getDatabaseAddress() const;
    virtual size_t getDatabaseSize() const;

    virtual char *getCommandLine() const;

    virtual size_t getSectionHeaderCount() const;
    virtual size_t getSectionHeaderEntrySize() const;
    virtual size_t getSectionHeaderStringTableIndex() const;
    virtual uintptr_t getSectionHeaders() const;

    virtual void *getMemoryMap() const;
    virtual uint64_t getMemoryMapEntryAddress(void *opaque) const;
    virtual uint64_t getMemoryMapEntryLength(void *opaque) const;
    virtual uint32_t getMemoryMapEntryType(void *opaque) const;
    virtual void *nextMemoryMapEntry(void *opaque) const;

    virtual size_t getModuleCount() const;
    virtual void *getModuleBase() const;

private:
    // If we are passed via grub, this information will be completely different to
    // via the bootstrapper.
    uint32_t flags;

    uint32_t mem_lower;
    uint32_t mem_upper;

    uint32_t boot_device;

    uint32_t cmdline;

    uint32_t mods_count;
    uint32_t mods_addr;

    /* ELF information */
    uint32_t num;
    uint32_t size;
    uint32_t addr;
    uint32_t shndx;

    uint32_t mmap_length;
    uint32_t mmap_addr;

    uint32_t drives_length;
    uint32_t drives_addr;

    uint32_t config_table;

    uint32_t boot_loader_name;

    uint32_t apm_table;

    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint32_t vbe_mode;
    uint32_t vbe_interface_seg;
    uint32_t vbe_interface_off;
    uint32_t vbe_interface_len;
} PACKED;

#else

class BootstrapStruct_t : public BaseBootstrapStruct
{
public:
    int (*prom)(struct anon*);
    uint32_t initrd_start;
    uint32_t initrd_end;

    /* ELF information */
    uint32_t num;
    uint32_t size;
    uint32_t addr;
    uint32_t shndx;

    virtual bool isInitrdLoaded() const
    {
        return true;
    }
    virtual uint8_t *getInitrdAddress() const
    {
        return reinterpret_cast<uint8_t*>(initrd_start);
    }
    virtual size_t getInitrdSize() const
    {
        return initrd_end - initrd_start;
    }

    virtual char *getCommandLine() const
    {
        static char buf[1] = {0};
        return buf;
    }
};

#endif

/** @} */

#endif
