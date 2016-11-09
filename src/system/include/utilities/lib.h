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

#include <stdarg.h>
#include <compiler.h>

// Condense X86-ish systems into one define for utilities.
/// \note this will break testsuite/hosted builds on non-x86 hosts.
#if defined(X86_COMMON) || defined(HOSTED_X86_COMMON) || defined(UTILITY_LINUX)
#define TARGET_IS_X86
#endif

#ifdef __cplusplus
extern "C" {
#endif

// String functions.
#define StringLength(x) (IS_CONSTANT(x) \
    ? __builtin_strlen((x)) : _StringLength(x))
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
unsigned long StringToUnsignedLong(const char *nptr, char const **endptr, int base);

// Memory functions.
void *ByteSet(void *buf, int c, size_t len);
void *WordSet(void *buf, int c, size_t len);
void *DoubleWordSet(void *buf, unsigned int c, size_t len);
void *QuadWordSet(void *buf, unsigned long long c, size_t len);
void *ForwardMemoryCopy(void *s1, const void *s2, size_t n);
void *MemoryCopy(void *s1, const void *s2, size_t n);
int MemoryCompare(const void *p1, const void *p2, size_t len) PURE;

// Built-in PRNG.
void random_seed(uint64_t seed);
uint64_t random_next(void);

inline char toUpper(char c) PURE;
inline char toUpper(char c)
{
    if (c < 'a' || c > 'z')
        return c; // special chars
    c += ('A' - 'a');
    return c;
}

inline char toLower(char c) PURE;
inline char toLower(char c)
{
    if (c < 'A' || c > 'Z')
        return c; // special chars
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
void *malloc(size_t);
void *realloc(void *, size_t);
void free(void *);

#ifdef __cplusplus
}

// Export C++ support library header.
#include <utilities/cpp.h>
#endif

#endif  // KERNEL_UTILITIES_LIB_H
