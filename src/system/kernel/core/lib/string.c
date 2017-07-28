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

#include "pedigree/kernel/processor/types.h"
#include "pedigree/kernel/utilities/utility.h"
#include <stdarg.h>
#include <stddef.h>

size_t strlen(const char *s);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t len);
int strcmp(const char *p1, const char *p2);
int strncmp(const char *p1, const char *p2, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
const char *strchr(const char *str, int target);
const char *strrchr(const char *str, int target);
int vsprintf(char *buf, const char *fmt, va_list arg);
unsigned long strtoul(const char *nptr, char const **endptr, int base);

#define ULONG_MAX -1

size_t _StringLength(const char *src)
{
    if (!src)
    {
        return 0;
    }

    // Unrolled loop that still avoids reading past the end of src (instead of
    // e.g. doing bitmasks with 64-bit views of src).
    const char *orig = src;
    size_t result = 0;
    while (1)
    {
#define UNROLL(n)    \
    if (!*(src + n)) \
        return (src + n) - orig;
        UNROLL(0);
        UNROLL(1);
        UNROLL(2);
        UNROLL(3);
        UNROLL(4);
        UNROLL(5);
        UNROLL(6);
        UNROLL(7);
#undef UNROLL
        src += 8;
    }
}

char *StringCopy(char *dest, const char *src)
{
    char *orig_dest = dest;
    while (*src)
    {
        *dest = *src;
        ++dest;
        ++src;
    }
    *dest = '\0';

    return orig_dest;
}

char *StringCopyN(char *dest, const char *src, size_t len)
{
    char *orig_dest = dest;
    for (size_t i = 0; i < len; ++i)
    {
        if (*src)
            *dest++ = *src++;
        else
            break;
    }
    *dest = '\0';

    return orig_dest;
}

int StringFormat(char *buf, const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i = VStringFormat(buf, fmt, args);
    va_end(args);

    return i;
}

int StringCompare(const char *p1, const char *p2)
{
    if (p1 == p2)
        return 0;

    while (*p1 && *p2)
    {
        char c = *p1 - *p2;
        if (c)
            return c;
        ++p1;
        ++p2;
    }

    return *p1 - *p2;
}

int StringCompareN(const char *p1, const char *p2, size_t n)
{
    if (!n)
        return 0;
    if (p1 == p2)
        return 0;

    while (*p1 && *p2)
    {
        char c = *p1 - *p2;
        if (c)
            return c;
        else if (!--n)
            return *p1 - *p2;

        ++p1;
        ++p2;
    }

    return *p1 - *p2;
}

char *StringConcat(char *dest, const char *src)
{
    size_t di = StringLength(dest);
    size_t si = 0;
    while (src[si])
        dest[di++] = src[si++];

    dest[di] = '\0';

    return dest;
}

char *StringConcatN(char *dest, const char *src, size_t n)
{
    size_t di = StringLength(dest);
    size_t si = 0;
    while (src[si] && n)
    {
        dest[di++] = src[si++];
        n--;
    }

    dest[di] = '\0';

    return dest;
}

int isspace(int c)
{
    return (c == ' ' || c == '\n' || c == '\r' || c == '\t');
}

int isupper(int c)
{
    return (c >= 'A' && c <= 'Z');
}

int islower(int c)
{
    return (c >= 'a' && c <= 'z');
}

int isdigit(int c)
{
    return (c >= '0' && c <= '9');
}

int isalpha(int c)
{
    return isupper(c) || islower(c) || isdigit(c);
}

unsigned long
StringToUnsignedLong(const char *nptr, char const **endptr, int base)
{
    register const char *s = nptr;
    register unsigned long acc;
    register int c;
    register unsigned long cutoff;
    register int neg = 0, any, cutlim;

    /*
     * See strtol for comments as to the logic used.
     */
    do
    {
        c = *s++;
    } while (isspace(c));
    if (c == '-')
    {
        neg = 1;
        c = *s++;
    }
    else if (c == '+')
        c = *s++;
    if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X'))
    {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0)
        base = c == '0' ? 8 : 10;
    cutoff = (unsigned long) ULONG_MAX / (unsigned long) base;
    cutlim = (unsigned long) ULONG_MAX % (unsigned long) base;
    for (acc = 0, any = 0;; c = *s++)
    {
        if (isdigit(c))
            c -= '0';
        else if (isalpha(c))
            c -= isupper(c) ? 'A' - 10 : 'a' - 10;
        else
            break;
        if (c >= base)
            break;
        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
            any = -1;
        else
        {
            any = 1;
            acc *= base;
            acc += c;
        }
    }
    if (any < 0)
    {
        acc = ULONG_MAX;
    }
    else if (neg)
        acc = -acc;
    if (endptr != 0)
        *endptr = (const char *) (any ? s - 1 : nptr);

    return (acc);
}

const char *StringFind(const char *str, int target)
{
    const char *s;
    char ch;
    while (1)
    {
#define UNROLL(n)     \
    s = str + n;      \
    ch = *s;          \
    if (!ch)          \
        return NULL;  \
    if (ch == target) \
        return s;

        UNROLL(0);
        UNROLL(1);
        UNROLL(2);
        UNROLL(3);
        UNROLL(4);
        UNROLL(5);
        UNROLL(6);
        UNROLL(7);
#undef UNROLL
        str += 8;
    }
}

const char *StringReverseFind(const char *str, int target)
{
    // StringLength must traverse the entire string once to find the length,
    // so rather than finding the length and then traversing in reverse, we just
    // traverse the string once. This gives a small performance boost.
    const char *s;
    const char *result = NULL;
    char ch;
    while (1)
    {
#define UNROLL(n)      \
    s = str + n;       \
    ch = *s;           \
    if (!ch)           \
        return result; \
    if (ch == target)  \
        result = s;

        UNROLL(0);
        UNROLL(1);
        UNROLL(2);
        UNROLL(3);
        UNROLL(4);
        UNROLL(5);
        UNROLL(6);
        UNROLL(7);
#undef UNROLL
        str += 8;
    }
}

int StringCompareCase(
    const char *s1, const char *s2, int sensitive, size_t length,
    size_t *offset)
{
    if (!length)
    {
        return 0;
    }
    else if (s1 == s2)
    {
        *offset = StringLength(s1);
        return 0;
    }
    else if (!s1)
    {
        return -1;
    }
    else if (!s2)
    {
        return 1;
    }

    static size_t local = 0;
    if (UNLIKELY(!offset))
    {
        offset = &local;
    }

    if (LIKELY(sensitive))
    {
        size_t i = 0;
        while (*s1 && *s2)
        {
            char c = *s1 - *s2;
            if (c)
            {
                break;
            }
            else if (!--length)
            {
                break;
            }

            ++s1;
            ++s2;
            ++i;
        }

        *offset = i;
        return *s1 - *s2;
    }
    else
    {
        size_t i = 0;
        while (*s1 && *s2)
        {
            char c = toLower(*s1) - toLower(*s2);
            if (c)
            {
                break;
            }
            else if (!--length)
            {
                break;
            }

            ++s1;
            ++s2;
            ++i;
        }

        *offset = i;
        return toLower(*s1) - toLower(*s2);
    }
}

#ifndef UTILITY_LINUX
// Provide forwarding functions to handle GCC optimising things.
size_t strlen(const char *s)
{
    return StringLength(s);
}

char *strcpy(char *dest, const char *src)
{
    return StringCopy(dest, src);
}

char *strncpy(char *dest, const char *src, size_t len)
{
    return StringCopyN(dest, src, len);
}

int strcmp(const char *p1, const char *p2)
{
    return StringCompare(p1, p2);
}

int strncmp(const char *p1, const char *p2, size_t n)
{
    return StringCompareN(p1, p2, n);
}

char *strcat(char *dest, const char *src)
{
    return StringConcat(dest, src);
}

char *strncat(char *dest, const char *src, size_t n)
{
    return StringConcatN(dest, src, n);
}

const char *strchr(const char *str, int target)
{
    return StringFind(str, target);
}

const char *strrchr(const char *str, int target)
{
    return StringReverseFind(str, target);
}

int vsprintf(char *buf, const char *fmt, va_list arg)
{
    return VStringFormat(buf, fmt, arg);
}

unsigned long strtoul(const char *nptr, char const **endptr, int base)
{
    return StringToUnsignedLong(nptr, endptr, base);
}
#endif
