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

#ifndef KERNEL_UTILITIES_LIB_H
#define KERNEL_UTILITIES_LIB_H

#include "pedigree/kernel/compiler.h"
#include <stdarg.h>

// Condense X86-ish systems into one define for utilities.
/// \note this will break testsuite/hosted builds on non-x86 hosts.
#if defined(X86_COMMON) || defined(HOSTED_X86_COMMON) || defined(UTILITY_LINUX)
#define TARGET_IS_X86
#endif

#ifdef __cplusplus
extern "C" {
#endif

// String functions.
#define StringLength(x) \
    (IS_CONSTANT(x) ? __builtin_strlen((x)) : _StringLength(x))
size_t _StringLength(const char *buf) PURE;
char *StringCopy(char *dest, const char *src);
char *StringCopyN(char *dest, const char *src, size_t len);
int StringCompare(const char *p1, const char *p2) PURE;
int StringCompareN(const char *p1, const char *p2, size_t n) PURE;
char *StringConcat(char *dest, const char *src);
char *StringConcatN(char *dest, const char *src, size_t n);
const char *StringFind(const char *str, int target) PURE;
const char *StringReverseFind(const char *str, int target) PURE;
int VStringFormat(char *buf, const char *fmt, va_list arg);
int StringFormat(char *buf, const char *fmt, ...) FORMAT(printf, 2, 3);
unsigned long
StringToUnsignedLong(const char *nptr, char const **endptr, int base);

// Compares the two strings with optional case-sensitivity. The offset out
// parameter holds the offset of a failed match in the case of a non-zero
// return, or the length of the string otherwise.
int StringCompareCase(
    const char *s1, const char *s2, int sensitive, size_t length,
    size_t *offset);

// Memory functions.
void *ByteSet(void *buf, int c, size_t len);
void *WordSet(void *buf, int c, size_t len);
void *DoubleWordSet(void *buf, unsigned int c, size_t len);
void *QuadWordSet(void *buf, unsigned long long c, size_t len);
void *ForwardMemoryCopy(void *s1, const void *s2, size_t n);
void *MemoryCopy(void *s1, const void *s2, size_t n);
int MemoryCompare(const void *p1, const void *p2, size_t len) PURE;

// Misc utilities for paths etc
const char *DirectoryName(const char *path) PURE;
const char *BaseName(const char *path) PURE;

// Character checks.
#ifndef UTILITY_LINUX
int isspace(int c) PURE;
int isupper(int c) PURE;
int islower(int c) PURE;
int isdigit(int c) PURE;
int isalpha(int c) PURE;
#endif

// Built-in PRNG.
void random_seed(uint64_t seed);
uint64_t random_next(void);

inline char toUpper(char c) PURE;
inline char toUpper(char c)
{
    if (c < 'a' || c > 'z')
        return c;  // special chars
    c += ('A' - 'a');
    return c;
}

inline char toLower(char c) PURE;
inline char toLower(char c)
{
    if (c < 'A' || c > 'Z')
        return c;  // special chars
    c -= ('A' - 'a');
    return c;
}

inline int max(size_t a, size_t b) PURE;
inline int max(size_t a, size_t b)
{
    return a > b ? a : b;
}

inline int min(size_t a, size_t b) PURE;
inline int min(size_t a, size_t b)
{
    return a > b ? b : a;
}

// Memory allocation for C code
#ifndef UTILITY_LINUX
void *malloc(size_t);
void *realloc(void *, size_t);
void free(void *);
#endif

/// Basic 8-bit checksum check (returns 1 if checksum is correct).
uint8_t checksum(const uint8_t *pMemory, size_t sMemory);

/// Fletcher 16-bit checksum.
uint16_t checksum16(const uint8_t *pMemory, size_t sMemory);

/// Fletcher 32-bit checksum.
uint32_t checksum32(const uint8_t *pMemory, size_t sMemory);

/// Fletcher 32-bit checksum.
uint32_t checksum32_naive(const uint8_t *pMemory, size_t sMemory);

/// Checksum a page of memory.
uint32_t checksumPage(uintptr_t address);

/// ELF-style hash.
uint32_t elfHash(const char *buffer, size_t length);

/// Jenkins hash.
uint32_t jenkinsHash(const char *buffer, size_t length);

#ifdef __cplusplus
}

// Export C++ support library header.
#include "pedigree/kernel/utilities/cpp.h"
#endif

#endif  // KERNEL_UTILITIES_LIB_H
