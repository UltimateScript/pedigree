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
FUNCTION
<<fpurge>>---discard pending file I/O

INDEX
        fpurge
INDEX
        _fpurge_r

ANSI_SYNOPSIS
        #include <stdio.h>
        int fpurge(FILE *<[fp]>);

        int _fpurge_r(struct _reent *<[reent]>, FILE *<[fp]>);

DESCRIPTION
Use <<fpurge>> to clear all buffers of the given stream.  For output
streams, this discards data not yet written to disk.  For input streams,
this discards any data from <<ungetc>> and any data retrieved from disk
but not yet read via <<getc>>.  This is more severe than <<fflush>>,
and generally is only needed when manually altering the underlying file
descriptor of a stream.

The alternate function <<_fpurge_r>> is a reentrant version, where the
extra argument <[reent]> is a pointer to a reentrancy structure, and
<[fp]> must not be NULL.

RETURNS
<<fpurge>> returns <<0>> unless <[fp]> is not valid, in which case it
returns <<EOF>> and sets <<errno>>.

PORTABILITY
These functions are not portable to any standard.

No supporting OS subroutines are required.
*/

#define _COMPILING_NEWLIB

#include <newlib.h>

#include "local.h"
#include <_ansi.h>
#include <errno.h>
#include <stdio.h>

/* Discard I/O from a single file.  */

int _fpurge_r(struct _reent *ptr, register FILE *fp)
{
    int t;

    CHECK_INIT(ptr, fp);

    _flockfile(fp);

    t = fp->_flags;
    if (!t)
    {
        ptr->_errno = EBADF;
        _funlockfile(fp);
        return EOF;
    }
    fp->_p = fp->_bf._base;
    if ((t & __SWR) == 0)
    {
        fp->_r = 0;
        if (HASUB(fp))
            FREEUB(ptr, fp);
    }
    else
        fp->_w = t & (__SLBF | __SNBF) ? 0 : fp->_bf._size;
    _funlockfile(fp);
    return 0;
}

int fpurge(register FILE *fp)
{
    return _fpurge_r(_REENT, fp);
}
