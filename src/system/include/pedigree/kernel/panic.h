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

#ifndef KERNEL_PANIC_H
#define KERNEL_PANIC_H

#include "pedigree/kernel/compiler.h"

/** @addtogroup kernel
 * @{ */

#ifdef __cplusplus
extern "C" {
#endif

/** Prints out a message to the screen and the most recent log entries and halts
 *the processor. This function should only be called in unrecoverable emergency
 *cases, e.g. when the kernel is unable to successfully complete its
 *initialisation. \note This function and all functions called from this
 *function may not allocate any resources, e.g. no I/O port & memory-region
 *allocations and even no 'normal' memory allocations. \param[in] msg the
 *message to print to the screen */
void panic(const char *msg) NORETURN;

#ifdef __cplusplus
};
#endif
/** @} */

#endif
