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

#ifndef _SYS_RESOURCE_H_
#define _SYS_RESOURCE_H_

#include <sys/time.h>

typedef unsigned int rlim_t;

#define PRIO_PROCESS        0
#define PRIO_PGRP           1
#define PRIO_USER           2

#define	RUSAGE_SELF         0		/* calling process */
#define	RUSAGE_CHILDREN     -1		/* terminated child processes */

#define RLIM_INFINITY       (-1)
#define RLIM_SAVED_MAX      ((rlim_t) 0)
#define RLIM_SAVED_CUR      ((rlim_t) 1)

#define RLIMIT_CORE         0
#define RLIMIT_CPU          1
#define RLIMIT_DATA         2
#define RLIMIT_FSIZE        3
#define RLIMIT_NOFILE       4
#define RLIMIT_STACK        5
#define RLIMIT_AS           6

#define RLIM_NLIMITS        (RLIMIT_AS + 1)

struct rusage {
  	struct timeval ru_utime;	/* user time used */
	struct timeval ru_stime;	/* system time used */
};

typedef struct rlimit {
    rlim_t rlim_curr;
    rlim_t rlim_max;
} rlimit_t;
#define rlim_cur rlim_curr

_BEGIN_STD_C

int _EXFUN(getrlimit, (int resource, struct rlimit *rlp));
int _EXFUN(setrlimit, (int resource, const struct rlimit *rlp));

_END_STD_C

#endif

