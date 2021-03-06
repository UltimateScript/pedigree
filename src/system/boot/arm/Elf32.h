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

#ifndef ELF32_H
#define ELF32_H

#include "support.h"

#define SHT_PROGBITS 0x1  // The data is contained in the program file.
#define SHT_SYMTAB 0x2    // Symbol table
#define SHT_STRTAB 0x3    // String table
#define SHT_RELA 0x4
#define SHT_HASH 0x5     // Symbol hash table
#define SHT_DYNAMIC 0x6  // Dynamic linking information
#define SHT_NOTE 0x7
#define SHT_NOBITS 0x8  // The data is not contained in the program file.
#define SHT_REL 0x9
#define SHT_DYNSYM 0xb
#define SHT_INIT_ARRAY 0xe
#define SHT_FINI_ARRAY 0xf
#define SHT_PREINIT_ARRAY 0x10

#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4
#define SHF_MASKPROC 0xf0000000

#define ELF32_R_SYM(val) ((val) >> 8)
#define ELF32_R_TYPE(val) ((val) &0xff)

#define ELF32_ST_BIND(i) ((i) >> 4)
#define ELF32_ST_TYPE(i) ((i) &0xf)
#define ELF32_ST_INFO(b, t) (((b) << 4) + ((t) &0xf))
#define PACKED __attribute__((packed))

class BootstrapInfo;

/**
 * Provides an implementation of a 32-bit Executable and Linker format file
 * parser. The ELF data can be loaded either by supplying an entire ELF file in
 * a buffer, or by supplying details of each section seperately. \note This
 * class does not copy any section data. The supplied buffer pointers must
 *       remain valid throughout the lifespan of this class.
 */
class Elf32
{
  public:
    /**
     * Default constructor - loads no data.
     * \param name An identifier for this ELF file. This is copied into the
     * class.
     */
    Elf32(const char *name);

    /**
     * Destructor. Doesn't do much.
     */
    ~Elf32();

    /**
     * Constructs an Elf32 object, and assumes the given pointer to be to a
     * contiguous region of memory containing an ELF object.
     */
    bool load(uint8_t *pBuffer, unsigned int nBufferLength);

    /**
     * Extracts the symbol and string tables from the given BootstrapInfo class.
     */
    bool load(BootstrapInfo *pBootstrap);

    /**
     * Writes all writeable sections to their virtual addresses.
     * \return True on success.
     */
    bool writeSections();

    /**
     * Returns the virtual address of the last byte to be written. Used to
     * calculate the sbrk memory breakpoint.
     */
    unsigned int getLastAddress();

    /**
     * Returns the name of the symbol which contains 'addr', and also the
     * starting address of that symbol in 'startAddr' if startAddr != 0.
     * \param[in] addr The address to look up.
     * \param[out] startAddr The starting address of the found symbol
     * (optional). \return The symbol name, as a C string.
     */
    const char *lookupSymbol(unsigned int addr, unsigned int *startAddr = 0);

    /**
     * Returns the address of the symbol with offset 'off' in the dynamic
     * relocation table.
     */
    uint32_t lookupDynamicSymbolAddress(uint32_t off);

    /**
     * Returns a NULL terminated string specifying the name of
     * the symbol at offset 'off' in the relocation symbol table.
     */
    char *lookupDynamicSymbolName(uint32_t off);

    /**
     * Gets the address of the global offset table.
     * \return Address of the GOT, or 0 if none was found.
     */
    uint32_t getGlobalOffsetTable();

    /**
     * Returns the entry point of the file.
     */
    uint32_t getEntryPoint();

    // private:
    /** The copy-constructor
     *\note currently not implemented */
    Elf32(const Elf32 &);
    /** The assignment operator
     *\note currently not implemented */
    Elf32 &operator=(const Elf32 &);

    struct Elf32Header_t
    {
        uint8_t ident[16];
        uint16_t type;
        uint16_t machine;
        uint32_t version;
        uint32_t entry;
        uint32_t phoff;
        uint32_t shoff;
        uint32_t flags;
        uint16_t ehsize;
        uint16_t phentsize;
        uint16_t phnum;
        uint16_t shentsize;
        uint16_t shnum;
        uint16_t shstrndx;
    } PACKED;

    struct Elf32ProcessHeader_t
    {
        uint32_t type;
        uint32_t offset;
        uint32_t vaddr;
        uint32_t paddr;
        uint32_t filesz;
        uint32_t memsz;
        uint32_t flags;
        uint32_t align;
    } PACKED;

    struct Elf32SectionHeader_t
    {
        uint32_t name;
        uint32_t type;
        uint32_t flags;
        uint32_t addr;
        uint32_t offset;
        uint32_t size;
        uint32_t link;
        uint32_t info;
        uint32_t addralign;
        uint32_t entsize;
    } PACKED;

    struct Elf32Symbol_t
    {
        uint32_t name;
        uint32_t value;
        uint32_t size;
        uint8_t info;
        uint8_t other;
        uint16_t shndx;
    } PACKED;

    struct Elf32Dyn_t
    {
        int32_t tag;
        union
        {
            int32_t val;
            uint32_t ptr;
        } un;
    } PACKED;

    struct Elf32Rel_t
    {
        uint32_t offset;
        uint32_t info;
    } PACKED;

    Elf32Header_t *m_pHeader;
    Elf32SectionHeader_t *m_pSymbolTable;
    Elf32SectionHeader_t *m_pStringTable;
    Elf32SectionHeader_t *m_pShstrtab;
    Elf32SectionHeader_t *m_pGotTable;  // Global offset table.
    Elf32SectionHeader_t *m_pRelTable;
    Elf32SectionHeader_t *m_pSectionHeaders;
    char m_pId[128];
    uint8_t *m_pBuffer;  ///< Offset of the file in memory.
};

#endif
