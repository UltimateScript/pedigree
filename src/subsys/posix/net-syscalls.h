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

#ifndef NET_SYSCALLS_H
#define NET_SYSCALLS_H

#include <vfs/File.h>
#include "logging.h"

#include <sys/types.h>
#include <sys/socket.h>

struct sockaddr;

int posix_socket(int domain, int type, int protocol);
int posix_connect(int sock, struct sockaddr* address, socklen_t addrlen);

ssize_t posix_send(int sock, const void* buff, size_t bufflen, int flags);
ssize_t posix_sendto(int sock, void *buff, size_t bufflen, int flags, struct sockaddr *address, socklen_t addrlen);
ssize_t posix_recv(int sock, void* buff, size_t bufflen, int flags);
ssize_t posix_recvfrom(int sock, void *buff, size_t bufflen, int flags, struct sockaddr *address, socklen_t *addrlen);

int posix_listen(int sock, int backlog);
int posix_bind(int sock, const struct sockaddr *address, socklen_t addrlen);
int posix_accept(int sock, struct sockaddr* address, socklen_t* addrlen);

int posix_shutdown(int socket, int how);

int posix_getpeername(int socket, struct sockaddr *address, socklen_t *address_len);
int posix_getsockname(int socket, struct sockaddr *address, socklen_t *address_len);

int posix_getsockopt(int sock, int level, int optname, void* optvalue, socklen_t *optlen);

int posix_sethostname(const char *name, size_t len);

#endif
