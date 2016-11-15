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

#ifndef CONSOLE_SYSCALLS_H
#define CONSOLE_SYSCALLS_H

#include <vfs/File.h>

#include <sys/types.h>

struct termios;
struct winsize;

int posix_tcgetattr(int fd, struct termios *p);
int posix_tcsetattr(int fd, int optional_actions, struct termios *p);
int console_getwinsize(File* file, struct winsize *buf);
int console_setwinsize(File *file, const struct winsize *buf);
int console_flush(File *file, void *what);

int console_ptsname(int fd, char *buf);
int console_ttyname(int fd, char *buf);

int console_setctty(int fd, bool steal);
int console_notty(int fd);

// get the terminal's number (e.g. ptyp0 returns 0)
unsigned int console_getptn(int fd);

pid_t posix_tcgetpgrp(int fd);
int posix_tcsetpgrp(int fd, pid_t pgid_id);

#endif
