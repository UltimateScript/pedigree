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

#include "modules/system/network-stack/NetManager.h"
#include "modules/system/network-stack/NetworkStack.h"
#include "modules/system/network-stack/RoutingTable.h"
#include "modules/system/network-stack/Tcp.h"
#include "modules/system/network-stack/UdpManager.h"
#include "modules/system/vfs/File.h"
#include "modules/system/vfs/VFS.h"
#include "pedigree/kernel/machine/Network.h"
#include "pedigree/kernel/process/Process.h"
#include "pedigree/kernel/processor/Processor.h"
#include "pedigree/kernel/processor/types.h"
#include "pedigree/kernel/syscallError.h"
#include "pedigree/kernel/utilities/Tree.h"

#include "modules/system/lwip/include/lwip/api.h"
#include "modules/system/lwip/include/lwip/ip_addr.h"
#include "modules/system/lwip/include/lwip/tcp.h"

#include "pedigree/kernel/Subsystem.h"
#include <PosixSubsystem.h>
#include <UnixFilesystem.h>

#include "file-syscalls.h"
#include "net-syscalls.h"

#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/tcp.h>

static Tree<struct netconn *, struct netconnMetadata *> g_NetconnMetadata;

netconnMetadata::netconnMetadata() : recv(0), send(0), error(false), lock(false), semaphores(), offset(0), pb(nullptr), buf(nullptr)
{
}

static void netconnCallback(struct netconn *conn, enum netconn_evt evt, u16_t len)
{
    struct netconnMetadata *meta = getNetconnMetadata(conn);

    meta->lock.acquire();

    switch(evt)
    {
        case NETCONN_EVT_RCVPLUS:
            NOTICE("RCV+");
            ++(meta->recv);
            break;
        case NETCONN_EVT_RCVMINUS:
            NOTICE("RCV-");
            if (meta->recv)
            {
                --(meta->recv);
            }
            break;
        case NETCONN_EVT_SENDPLUS:
            NOTICE("SND+");
            meta->send = 1;
            break;
        case NETCONN_EVT_SENDMINUS:
            NOTICE("SND-");
            meta->send = 0;
            break;
        case NETCONN_EVT_ERROR:
            NOTICE("ERR");
            meta->error = true;  /// \todo figure out how to bubble errors
            break;
        default:
            NOTICE("Unknown error.");
    }

    for (auto &it : meta->semaphores)
    {
        it->release();
    }

    meta->lock.release();
}

static void lwipToSyscallError(err_t err)
{
    if (err != ERR_OK)
    {
        N_NOTICE(" -> lwip strerror gives '" << lwip_strerr(err) << "'");
    }
    // Based on lwIP's err_to_errno_table.
    switch(err)
    {
        case ERR_OK:
            break;
        case ERR_MEM:
            SYSCALL_ERROR(OutOfMemory);
            break;
        case ERR_BUF:
            SYSCALL_ERROR(NoMoreBuffers);
            break;
        case ERR_TIMEOUT:
            SYSCALL_ERROR(TimedOut);
            break;
        case ERR_RTE:
            SYSCALL_ERROR(HostUnreachable);
            break;
        case ERR_INPROGRESS:
            SYSCALL_ERROR(InProgress);
            break;
        case ERR_VAL:
            SYSCALL_ERROR(InvalidArgument);
            break;
        case ERR_WOULDBLOCK:
            SYSCALL_ERROR(NoMoreProcesses);
            break;
        case ERR_USE:
            // address in use
            SYSCALL_ERROR(InvalidArgument);
            break;
        case ERR_ALREADY:
            SYSCALL_ERROR(Already);
            break;
        case ERR_ISCONN:
            SYSCALL_ERROR(IsConnected);
            break;
        case ERR_CONN:
            SYSCALL_ERROR(NotConnected);
            break;
        case ERR_IF:
            // no error
            break;
        case ERR_ABRT:
            SYSCALL_ERROR(ConnectionAborted);
            break;
        case ERR_RST:
            SYSCALL_ERROR(ConnectionReset);
            break;
        case ERR_CLSD:
            SYSCALL_ERROR(NotConnected);
            break;
        case ERR_ARG:
            SYSCALL_ERROR(IoError);
            break;
    }
}

static bool isSaneSocket(FileDescriptor *f)
{
    if (!f)
    {
        N_NOTICE(" -> isSaneSocket: descriptor is null");
        SYSCALL_ERROR(BadFileDescriptor);
        return false;
    }

    if (!(f->file || f->socket))
    {
        N_NOTICE(" -> isSaneSocket: both file and socket members are null");
        SYSCALL_ERROR(BadFileDescriptor);
        return false;
    }

    if (f->file && f->socket)
    {
        N_NOTICE(" -> isSaneSocket: both file and socket members are non-null");
        SYSCALL_ERROR(BadFileDescriptor);
        return false;
    }

    return true;
}

static ip_addr_t sockaddrToIpaddr(const struct sockaddr *saddr, uint16_t &port)
{
    ip_addr_t result;
    ByteSet(&result, 0, sizeof(result));

    if (saddr->sa_family == AF_INET)
    {
        const struct sockaddr_in *sin = reinterpret_cast<const struct sockaddr_in *>(saddr);
        result.u_addr.ip4.addr = sin->sin_addr.s_addr;
        result.type = IPADDR_TYPE_V4;

        port = BIG_TO_HOST16(sin->sin_port);
    }
    else
    {
        ERROR("sockaddrToIpaddr: only AF_INET is supported at the moment.");
    }

    return result;
}

struct netconnMetadata *getNetconnMetadata(struct netconn *conn)
{
    struct netconnMetadata *result = g_NetconnMetadata.lookup(conn);
    if (!result)
    {
        result = new struct netconnMetadata;
        g_NetconnMetadata.insert(conn, result);
    }

    return result;
}

int posix_socket(int domain, int type, int protocol)
{
    N_NOTICE("socket(" << domain << ", " << type << ", " << protocol << ")");

    // Lookup this process.
    Process *pProcess =
        Processor::information().getCurrentThread()->getParent();
    PosixSubsystem *pSubsystem =
        reinterpret_cast<PosixSubsystem *>(pProcess->getSubsystem());
    if (!pSubsystem)
    {
        ERROR("No subsystem for one or both of the processes!");
        return -1;
    }

    size_t fd = pSubsystem->getFd();

    netconn_type connType = NETCONN_INVALID;

    File *file = 0;
    struct netconn *conn = 0;
    bool valid = true;

    /// \todo need to verify type parameter (e.g. SOCK_DGRAM is no good for TCP)
    if (domain == AF_INET)
    {
        switch (protocol)
        {
            case IPPROTO_TCP:
                connType = NETCONN_TCP;
                break;
            case IPPROTO_UDP:
                connType = NETCONN_UDP;
                break;
            default:
                valid = false;
        }
    }
    else if (domain == AF_INET6)
    {
        switch (protocol)
        {
            case IPPROTO_TCP:
                connType = NETCONN_TCP_IPV6;
                break;
            case IPPROTO_UDP:
                connType = NETCONN_UDP_IPV6;
                break;
            default:
                valid = false;
        }
    }
    else if (domain == AF_PACKET)
    {
        connType = NETCONN_RAW;
    }
    else if (domain == AF_UNIX)
    {
        // OK
    }
    else
    {
        WARNING("domain = " << domain << " - not known!");
        SYSCALL_ERROR(InvalidArgument);
        valid = false;
    }

    if (!valid)
    {
        N_NOTICE(" -> invalid socket parameters");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    if (domain != AF_UNIX)
    {
        NOTICE("creating new connection!");
        conn = netconn_new_with_callback(connType, netconnCallback);
        if (!conn)
        {
            /// \todo get the real error
            SYSCALL_ERROR(InvalidArgument);
            return -1;
        }

        NOTICE("connection is " << conn);
    }

    FileDescriptor *f = new FileDescriptor;
    f->socket = conn;
    f->file = file;
    f->offset = 0;
    f->fd = fd;
    f->so_domain = domain;
    f->so_type = type;
    pSubsystem->addFileDescriptor(fd, f);

    N_NOTICE("  -> " << Dec << fd << Hex);
    return static_cast<int>(fd);
}

int posix_connect(int sock, struct sockaddr *address, socklen_t addrlen)
{
    N_NOTICE("connect");

    if (!PosixSubsystem::checkAddress(
            reinterpret_cast<uintptr_t>(address), addrlen,
            PosixSubsystem::SafeRead))
    {
        N_NOTICE("connect -> invalid address");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    N_NOTICE(
        "posix_connect(" << sock << ", " << reinterpret_cast<uintptr_t>(address)
                         << ", " << addrlen << ")");

    Process *pProcess =
        Processor::information().getCurrentThread()->getParent();
    PosixSubsystem *pSubsystem =
        reinterpret_cast<PosixSubsystem *>(pProcess->getSubsystem());
    if (!pSubsystem)
    {
        ERROR("No subsystem for one or both of the processes!");
        return -1;
    }

    FileDescriptor *f = pSubsystem->getFileDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    if (f->so_domain == AF_UNIX)
    {
        if (address->sa_family != AF_UNIX)
        {
            // EAFNOSUPPORT
            N_NOTICE(" -> address family unsupported");
            return -1;
        }

        // Valid state. But no socket, so do the magic here.
        struct sockaddr_un *un =
            reinterpret_cast<struct sockaddr_un *>(address);
        String pathname;
        normalisePath(pathname, un->sun_path);

        N_NOTICE(" -> unix connect: '" << pathname << "'");

        f->file = VFS::instance().find(pathname);
        if (!f->file)
        {
            SYSCALL_ERROR(DoesNotExist);
            N_NOTICE(" -> unix socket '" << pathname << "' doesn't exist");
            return -1;
        }

        if (!f->file->isSocket())
        {
            /// \todo wrong error
            SYSCALL_ERROR(DoesNotExist);
            N_NOTICE(
                " -> target '" << pathname << "' is not a unix socket");
            return -1;
        }

        f->so_remotePath = pathname;

        return 0;
    }
    else if (f->so_domain != address->sa_family)
    {
        // EAFNOSUPPORT
        N_NOTICE(" -> incorrect address family passed to connect()");
        return -1;
    }

    struct netconn *conn = f->socket;

    /// \todo need to figure out if we've already done a bind()
    ip_addr_t ipaddr;
    ByteSet(&ipaddr, 0, sizeof(ipaddr));
    err_t err = netconn_bind(conn, &ipaddr, 0);  // bind to any address
    if (err != ERR_OK)
    {
        N_NOTICE(" -> lwip error when binding before connect");
        lwipToSyscallError(err);
        return -1;
    }

    uint16_t port = 0;
    ipaddr = sockaddrToIpaddr(address, port);

    // set blocking status if needed
    bool blocking = !((f->flflags & O_NONBLOCK) == O_NONBLOCK);
    netconn_set_nonblocking(conn, blocking ? 0 : 1);

    N_NOTICE("using socket " << conn << "!");
    N_NOTICE(" -> connecting to remote " << ipaddr_ntoa(&ipaddr) << " on port " << Dec << port);

    err = netconn_connect(conn, &ipaddr, port);
    if (err != ERR_OK)
    {
        N_NOTICE(" -> lwip error");
        lwipToSyscallError(err);
        return -1;
    }

    return 0;
}

ssize_t posix_send(int sock, const void *buff, size_t bufflen, int flags)
{
    N_NOTICE("posix_send");

    if (!PosixSubsystem::checkAddress(
            reinterpret_cast<uintptr_t>(buff), bufflen,
            PosixSubsystem::SafeRead))
    {
        N_NOTICE("send -> invalid address");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    N_NOTICE(
        "posix_send(" << sock << ", " << buff << ", " << bufflen << ", "
                      << flags << ")");

    Process *pProcess =
        Processor::information().getCurrentThread()->getParent();
    PosixSubsystem *pSubsystem =
        reinterpret_cast<PosixSubsystem *>(pProcess->getSubsystem());
    if (!pSubsystem)
    {
        ERROR("No subsystem for one or both of the processes!");
        return -1;
    }

    FileDescriptor *f = pSubsystem->getFileDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    if (f->so_domain == AF_UNIX)
    {
        return f->file->write(
            reinterpret_cast<uintptr_t>(
                static_cast<const char *>(f->so_localPath)),
            bufflen, reinterpret_cast<uintptr_t>(buff),
            (f->flflags & O_NONBLOCK) == 0);
    }
    else if (f->socket)
    {
        struct netconnMetadata *meta = getNetconnMetadata(f->socket);

        // Handle non-blocking sockets that would block to send.
        bool blocking = !((f->flflags & O_NONBLOCK) == O_NONBLOCK);
        if (!blocking && !meta->send)
        {
            // Can't safely send.
            SYSCALL_ERROR(NoMoreProcesses);
            return -1;
        }

        /// \todo flags
        size_t bytesWritten = 0;
        err_t err = netconn_write_partly(f->socket, buff, bufflen, 0, &bytesWritten);
        if (err != ERR_OK)
        {
            N_NOTICE(" -> lwip failed in netconn_write");
            lwipToSyscallError(err);
            return -1;
        }

        return bytesWritten;
    }
    else
    {
        // invalid somehow
        return -1;
    }
}

ssize_t posix_sendto(
    int sock, void *buff, size_t bufflen, int flags, struct sockaddr *address,
    socklen_t addrlen)
{
    N_NOTICE("posix_sendto");

    if (!PosixSubsystem::checkAddress(
            reinterpret_cast<uintptr_t>(buff), bufflen,
            PosixSubsystem::SafeRead))
    {
        N_NOTICE("sendto -> invalid address for transmission buffer");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    N_NOTICE(
        "posix_sendto(" << sock << ", " << buff << ", " << bufflen << ", "
                        << flags << ", " << address << ", " << addrlen << ")");

    Process *pProcess =
        Processor::information().getCurrentThread()->getParent();
    PosixSubsystem *pSubsystem =
        reinterpret_cast<PosixSubsystem *>(pProcess->getSubsystem());
    if (!pSubsystem)
    {
        ERROR("No subsystem for one or both of the processes!");
        return -1;
    }

    FileDescriptor *f = pSubsystem->getFileDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    if (address && (f->so_domain != address->sa_family))
    {
        // EAFNOSUPPORT
        return -1;
    }

    if (f->so_domain == AF_UNIX)
    {
        File *pFile = 0;
        if (address)
        {
            if (!PosixSubsystem::checkAddress(
                    reinterpret_cast<uintptr_t>(address),
                    sizeof(struct sockaddr_un), PosixSubsystem::SafeRead))
            {
                N_NOTICE(
                    "sendto -> invalid address for AF_UNIX struct sockaddr_un");
                SYSCALL_ERROR(InvalidArgument);
                return -1;
            }

            const struct sockaddr_un *un =
                reinterpret_cast<const struct sockaddr_un *>(address);
            String pathname;
            normalisePath(pathname, un->sun_path);

            pFile = VFS::instance().find(pathname);
            if (!pFile)
            {
                SYSCALL_ERROR(DoesNotExist);
                N_NOTICE(" -> sendto path '" << pathname << "' does not exist");
                return -1;
            }
        }
        else
        {
            // Assume we're connect()'d, write to socket.
            pFile = f->file;
        }

        /// \todo sanity check pFile?
        String s(reinterpret_cast<const char *>(buff), bufflen);
        N_NOTICE(" -> send '" << s << "'");
        ssize_t result = pFile->write(
            reinterpret_cast<uintptr_t>(
                static_cast<const char *>(f->so_localPath)),
            bufflen, reinterpret_cast<uintptr_t>(buff),
            (f->flflags & O_NONBLOCK) == 0);
        N_NOTICE(" -> " << result);

        return result;
    }

    uint16_t port = 0;
    ip_addr_t ipaddr = sockaddrToIpaddr(address, port);

    /// \todo netconn_sendto

    N_NOTICE(" -> sendto() not implemented with lwIP");

    SYSCALL_ERROR(Unimplemented);
    return -1;
}

ssize_t posix_recv(int sock, void *buff, size_t bufflen, int flags)
{
    N_NOTICE("posix_recv");

    if (!PosixSubsystem::checkAddress(
            reinterpret_cast<uintptr_t>(buff), bufflen,
            PosixSubsystem::SafeWrite))
    {
        N_NOTICE("recv -> invalid address");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    N_NOTICE(
        "posix_recv(" << sock << ", " << buff << ", " << bufflen << ", "
                      << flags << ")");

    Process *pProcess =
        Processor::information().getCurrentThread()->getParent();
    PosixSubsystem *pSubsystem =
        reinterpret_cast<PosixSubsystem *>(pProcess->getSubsystem());
    if (!pSubsystem)
    {
        ERROR("No subsystem for one or both of the processes!");
        return -1;
    }

    FileDescriptor *f = pSubsystem->getFileDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    if (f->so_domain == AF_UNIX)
    {
        return f->so_local->read(
            0, bufflen, reinterpret_cast<uintptr_t>(buff),
            (f->flflags & O_NONBLOCK) == 0);
    }

    err_t err = ERR_OK;

    struct netconnMetadata *meta = getNetconnMetadata(f->socket);

    // Handle non-blocking sockets having nothing pending on the socket.
    bool blocking = !((f->flflags & O_NONBLOCK) == O_NONBLOCK);
    if (!blocking && !meta->recv && !meta->pb)
    {
        // No pending data to receive.
        SYSCALL_ERROR(NoMoreProcesses);
        return -1;
    }

    if (meta->pb == nullptr)
    {
        struct pbuf *pb = nullptr;
        struct netbuf *buf = nullptr;

        // No partial data present from a previous read. Read new data from
        // the socket.
        if (NETCONNTYPE_GROUP(netconn_type(f->socket)) == NETCONN_TCP)
        {
            err = netconn_recv_tcp_pbuf(f->socket, &pb);
        }
        else
        {
            err = netconn_recv(f->socket, &buf);
        }

        if (err != ERR_OK)
        {
            N_NOTICE(" -> lwIP error");
            lwipToSyscallError(err);
            return -1;
        }

        if (pb == nullptr && buf != nullptr)
        {
            pb = buf->p;
        }

        meta->pb = pb;
        meta->buf = buf;

        N_NOTICE(" -> recv is pulling new data from the netconn");
    }
    else
    {
        N_NOTICE(" -> recv is referencing unconsumed data from a previous call");
    }

    // now we read some things.
    size_t len = meta->offset + bufflen;
    if (len > meta->pb->tot_len)
    {
        len = meta->pb->tot_len - meta->offset;
    }

    pbuf_copy_partial(meta->pb, buff, len, meta->offset);

    // partial read?
    if ((meta->offset + len) < meta->pb->tot_len)
    {
        meta->offset += len;
    }
    else
    {
        N_NOTICE("offset=" << meta->offset << " / len=" << len << " / tot_len = " << meta->pb->tot_len << " / total read now = " << (meta->offset + len));
        N_NOTICE("  [partial reads completed, all pending data is consumed]");

        if (meta->buf == nullptr)
        {
            pbuf_free(meta->pb);
        }
        else
        {
            // will indirectly clean up meta->pb as it's a member of the netbuf
            netbuf_free(meta->buf);
        }

        meta->pb = nullptr;
        meta->buf = nullptr;
        meta->offset = 0;
    }

    N_NOTICE(" -> " << len);
    return len;
}

ssize_t posix_recvfrom(
    int sock, void *buff, size_t bufflen, int flags, struct sockaddr *address,
    socklen_t *addrlen)
{
    N_NOTICE("recvfrom");

    if (!(PosixSubsystem::checkAddress(
              reinterpret_cast<uintptr_t>(buff), bufflen,
              PosixSubsystem::SafeWrite) &&
          ((!address) || PosixSubsystem::checkAddress(
                             reinterpret_cast<uintptr_t>(addrlen),
                             sizeof(socklen_t), PosixSubsystem::SafeWrite))))
    {
        N_NOTICE("recvfrom -> invalid address for receive buffer or addrlen "
                 "parameter");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    N_NOTICE(
        "posix_recvfrom(" << sock << ", " << buff << ", " << bufflen << ", "
                          << flags << ", " << address << ", " << addrlen);

    Process *pProcess =
        Processor::information().getCurrentThread()->getParent();
    PosixSubsystem *pSubsystem =
        reinterpret_cast<PosixSubsystem *>(pProcess->getSubsystem());
    if (!pSubsystem)
    {
        ERROR("No subsystem for one or both of the processes!");
        return -1;
    }

    FileDescriptor *f = pSubsystem->getFileDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    if (f->so_domain == AF_UNIX)
    {
        File *pFile = f->so_local;
        struct sockaddr_un un_temp;
        struct sockaddr_un *un = 0;
        if (address)
        {
            un = reinterpret_cast<struct sockaddr_un *>(address);
        }
        else
        {
            un = &un_temp;
        }

        // this will load sun_path into sun_path automatically
        un->sun_family = AF_UNIX;
        ssize_t r = pFile->read(
            reinterpret_cast<uintptr_t>(un->sun_path), bufflen,
            reinterpret_cast<uintptr_t>(buff), (f->flflags & O_NONBLOCK) == 0);

        if ((r > 0) && address && addrlen)
        {
            *addrlen = sizeof(struct sockaddr_un);
        }

        N_NOTICE(" -> " << r);
        return r;
    }

    SYSCALL_ERROR(Unimplemented);
    return -1;
}

int posix_bind(int sock, const struct sockaddr *address, socklen_t addrlen)
{
    N_NOTICE("posix_bind");

    if (!PosixSubsystem::checkAddress(
            reinterpret_cast<uintptr_t>(address), addrlen,
            PosixSubsystem::SafeRead))
    {
        N_NOTICE("bind -> invalid address");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    N_NOTICE(
        "posix_bind(" << sock << ", " << address << ", " << addrlen << ")");

    Process *pProcess =
        Processor::information().getCurrentThread()->getParent();
    PosixSubsystem *pSubsystem =
        reinterpret_cast<PosixSubsystem *>(pProcess->getSubsystem());
    if (!pSubsystem)
    {
        ERROR("No subsystem for one or both of the processes!");
        return -1;
    }

    FileDescriptor *f = pSubsystem->getFileDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    if (address->sa_family != f->so_domain)
    {
        // EAFNOSUPPORT
        return -1;
    }

    if (f->so_domain == AF_UNIX)
    {

        // Valid state. But no socket, so do the magic here.
        const struct sockaddr_un *un =
            reinterpret_cast<const struct sockaddr_un *>(address);

        String adjusted_pathname;
        normalisePath(adjusted_pathname, un->sun_path);

        N_NOTICE(" -> unix bind: '" << adjusted_pathname << "'");

        File *cwd = Processor::information()
                        .getCurrentThread()
                        ->getParent()
                        ->getCwd();
        if (adjusted_pathname.endswith('/'))
        {
            // uh, that's a directory
            SYSCALL_ERROR(IsADirectory);
            return -1;
        }

        File *parentDirectory = cwd;

        const char *pDirname =
            DirectoryName(static_cast<const char *>(adjusted_pathname));
        const char *pBasename =
            BaseName(static_cast<const char *>(adjusted_pathname));

        String basename(pBasename);
        delete[] pBasename;

        if (pDirname)
        {
            // Reorder rfind result to be from beginning of string.
            String dirname(pDirname);
            delete[] pDirname;

            N_NOTICE(" -> dirname=" << dirname);

            parentDirectory = VFS::instance().find(dirname);
            if (!parentDirectory)
            {
                N_NOTICE(
                    " -> parent directory '" << dirname
                                             << "' doesn't exist");
                SYSCALL_ERROR(DoesNotExist);
                return -1;
            }
        }

        if (!parentDirectory->isDirectory())
        {
            SYSCALL_ERROR(NotADirectory);
            return -1;
        }

        Directory *pDir = Directory::fromFile(parentDirectory);

        UnixSocket *socket = new UnixSocket(
            basename, parentDirectory->getFilesystem(), parentDirectory);
        if (!pDir->addEphemeralFile(socket))
        {
            /// \todo errno?
            delete socket;
            return false;
        }

        N_NOTICE(" -> basename=" << basename);

        // HERE: set UnixSocket mode (dgram, stream) for correct operation

        // bind() then connect().
        f->so_local = f->file = socket;
        f->so_localPath = adjusted_pathname;

        return 0;
    }

    uint16_t port = 0;
    ip_addr_t ipaddr = sockaddrToIpaddr(address, port);

    err_t err = netconn_bind(f->socket, &ipaddr, port);
    if (err != ERR_OK)
    {
        N_NOTICE(" -> lwIP error");
        lwipToSyscallError(err);
        return -1;
    }

    return 0;
}

int posix_listen(int sock, int backlog)
{
    N_NOTICE("posix_listen(" << sock << ", " << backlog << ")");

    Process *pProcess =
        Processor::information().getCurrentThread()->getParent();
    PosixSubsystem *pSubsystem =
        reinterpret_cast<PosixSubsystem *>(pProcess->getSubsystem());
    if (!pSubsystem)
    {
        ERROR("No subsystem for one or both of the processes!");
        return -1;
    }

    FileDescriptor *f = pSubsystem->getFileDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    if (f->so_type != SOCK_STREAM)
    {
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    if (f->so_domain == AF_UNIX)
    {
        SYSCALL_ERROR(OperationNotSupported);
        N_NOTICE(" -> listen not supported for unix sockets yet");
        return -1;
    }

    err_t err = netconn_listen_with_backlog(f->socket, backlog);
    if (err != ERR_OK)
    {
        N_NOTICE(" -> lwIP error");
        lwipToSyscallError(err);
        return -1;
    }

    return 0;
}

int posix_accept(int sock, struct sockaddr *address, socklen_t *addrlen)
{
    N_NOTICE("posix_accept");

    if (!(PosixSubsystem::checkAddress(
              reinterpret_cast<uintptr_t>(address),
              sizeof(struct sockaddr_storage), PosixSubsystem::SafeWrite) &&
          PosixSubsystem::checkAddress(
              reinterpret_cast<uintptr_t>(addrlen), sizeof(socklen_t),
              PosixSubsystem::SafeWrite)))
    {
        N_NOTICE("accept -> invalid address");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    N_NOTICE(
        "posix_accept(" << sock << ", " << address << ", " << addrlen << ")");

    Process *pProcess =
        Processor::information().getCurrentThread()->getParent();
    PosixSubsystem *pSubsystem =
        reinterpret_cast<PosixSubsystem *>(pProcess->getSubsystem());
    if (!pSubsystem)
    {
        ERROR("No subsystem for one or both of the processes!");
        return -1;
    }

    FileDescriptor *f = pSubsystem->getFileDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    /// \todo check family

    struct netconn *new_conn = nullptr;

    err_t err = netconn_accept(f->socket, &new_conn);
    if (err != ERR_OK)
    {
        N_NOTICE(" -> lwIP error");
        lwipToSyscallError(err);
        return -1;
    }

    // get the new peer
    ip_addr_t peer;
    uint16_t port;
    err = netconn_peer(new_conn, &peer, &port);
    if (err != ERR_OK)
    {
        netconn_delete(new_conn);
        lwipToSyscallError(err);
        return -1;
    }

    /// \todo handle other families
    struct sockaddr_in *sin =
        reinterpret_cast<struct sockaddr_in *>(address);
    sin->sin_family = AF_INET;
    sin->sin_port = HOST_TO_BIG16(port);
    sin->sin_addr.s_addr = peer.u_addr.ip4.addr;
    *addrlen = sizeof(sockaddr_in);

    size_t fd = pSubsystem->getFd();

    FileDescriptor *desc = new FileDescriptor;
    desc->socket = new_conn;
    desc->file = 0;
    desc->offset = 0;
    desc->fd = fd;
    desc->so_domain = f->so_domain;
    desc->so_type = f->so_type;
    pSubsystem->addFileDescriptor(fd, desc);

    return static_cast<int>(fd);
}

int posix_shutdown(int socket, int how)
{
    N_NOTICE("posix_shutdown(" << socket << ", " << how << ")");

    Process *pProcess =
        Processor::information().getCurrentThread()->getParent();
    PosixSubsystem *pSubsystem =
        reinterpret_cast<PosixSubsystem *>(pProcess->getSubsystem());
    if (!pSubsystem)
    {
        ERROR("No subsystem for one or both of the processes!");
        return -1;
    }

    FileDescriptor *f = pSubsystem->getFileDescriptor(socket);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    /// \todo unix sockets don't do this

    int rx = 0;
    int tx = 0;
    if (how == SHUT_RDWR)
    {
        rx = tx = 1;
    }
    else if (how == SHUT_RD)
    {
        rx = 1;
    }
    else
    {
        tx = 1;
    }

    err_t err = netconn_shutdown(f->socket, rx, tx);
    if (err != ERR_OK)
    {
        lwipToSyscallError(err);
        return -1;
    }

    return 0;
}

int posix_getpeername(
    int socket, struct sockaddr *address, socklen_t *address_len)
{
    N_NOTICE("posix_getpeername");

    if (!(PosixSubsystem::checkAddress(
              reinterpret_cast<uintptr_t>(address),
              sizeof(struct sockaddr_storage), PosixSubsystem::SafeWrite) &&
          PosixSubsystem::checkAddress(
              reinterpret_cast<uintptr_t>(address_len), sizeof(socklen_t),
              PosixSubsystem::SafeWrite)))
    {
        N_NOTICE("getpeername -> invalid address");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    N_NOTICE(
        "posix_getpeername(" << socket << ", " << address << ", " << address_len
                             << ")");

    Process *pProcess =
        Processor::information().getCurrentThread()->getParent();
    PosixSubsystem *pSubsystem =
        reinterpret_cast<PosixSubsystem *>(pProcess->getSubsystem());
    if (!pSubsystem)
    {
        ERROR("No subsystem for one or both of the processes!");
        return -1;
    }

    FileDescriptor *f = pSubsystem->getFileDescriptor(socket);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    ip_addr_t peer;
    uint16_t port;
    err_t err = netconn_peer(f->socket, &peer, &port);
    if (err != ERR_OK)
    {
        lwipToSyscallError(err);
        return -1;
    }

    /// \todo handle other families
    struct sockaddr_in *sin =
        reinterpret_cast<struct sockaddr_in *>(address);
    sin->sin_family = AF_INET;
    sin->sin_port = HOST_TO_BIG16(port);
    sin->sin_addr.s_addr = peer.u_addr.ip4.addr;
    *address_len = sizeof(sockaddr_in);

    return 0;
}

int posix_getsockname(
    int socket, struct sockaddr *address, socklen_t *address_len)
{
    N_NOTICE("posix_getsockname");

    if (!(PosixSubsystem::checkAddress(
              reinterpret_cast<uintptr_t>(address),
              sizeof(struct sockaddr_storage), PosixSubsystem::SafeWrite) &&
          PosixSubsystem::checkAddress(
              reinterpret_cast<uintptr_t>(address_len), sizeof(socklen_t),
              PosixSubsystem::SafeWrite)))
    {
        N_NOTICE("getsockname -> invalid address");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    N_NOTICE(
        "posix_getsockname(" << socket << ", " << address << ", " << address_len
                             << ")");

    Process *pProcess =
        Processor::information().getCurrentThread()->getParent();
    PosixSubsystem *pSubsystem =
        reinterpret_cast<PosixSubsystem *>(pProcess->getSubsystem());
    if (!pSubsystem)
    {
        ERROR("No subsystem for one or both of the processes!");
        return -1;
    }

    FileDescriptor *f = pSubsystem->getFileDescriptor(socket);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    ip_addr_t self;
    uint16_t port;
    err_t err = netconn_addr(f->socket, &self, &port);
    if (err != ERR_OK)
    {
        lwipToSyscallError(err);
        return -1;
    }

    /// \todo handle other families
    struct sockaddr_in *sin =
        reinterpret_cast<struct sockaddr_in *>(address);
    sin->sin_family = AF_INET;
    sin->sin_port = HOST_TO_BIG16(port);
    sin->sin_addr.s_addr = self.u_addr.ip4.addr;
    *address_len = sizeof(sockaddr_in);

    return 0;
}

int posix_setsockopt(
    int sock, int level, int optname, const void *optvalue, socklen_t optlen)
{
    N_NOTICE("setsockopt(" << sock << ", " << level << ", " << optname << ", " << optvalue << ", " << optlen << ")");

    if (!(PosixSubsystem::checkAddress(
            reinterpret_cast<uintptr_t>(optvalue), optlen,
            PosixSubsystem::SafeWrite)))
    {
        N_NOTICE("getsockopt -> invalid address");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    // Valid socket?
    Process *pProcess =
        Processor::information().getCurrentThread()->getParent();
    PosixSubsystem *pSubsystem =
        reinterpret_cast<PosixSubsystem *>(pProcess->getSubsystem());
    if (!pSubsystem)
    {
        ERROR("No subsystem for one or both of the processes!");
        return -1;
    }

    FileDescriptor *f = pSubsystem->getFileDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    if (level == IPPROTO_TCP)
    {
        if (optname == TCP_NODELAY)
        {
            N_NOTICE(" -> TCP_NODELAY");
            const uint32_t *val = reinterpret_cast<const uint32_t *>(optvalue);
            N_NOTICE("  --> val=" << *val);

            // TCP_NODELAY controls Nagle's algorithm usage
            if (*val)
            {
                tcp_nagle_disable(f->socket->pcb.tcp);
            }
            else
            {
                tcp_nagle_enable(f->socket->pcb.tcp);
            }
        }
    }

    /// \todo this will have actually useful things to do

    // SO_ERROR etc
    /// \todo implement with lwIP functionality
    return -1;
}

int posix_getsockopt(
    int sock, int level, int optname, void *optvalue, socklen_t *optlen)
{
    N_NOTICE("getsockopt");

    // Check optlen first, then use it to check optvalue.
    if (!(PosixSubsystem::checkAddress(
              reinterpret_cast<uintptr_t>(optlen), sizeof(socklen_t),
              PosixSubsystem::SafeRead) &&
          PosixSubsystem::checkAddress(
              reinterpret_cast<uintptr_t>(optlen), sizeof(socklen_t),
              PosixSubsystem::SafeWrite)))
    {
        N_NOTICE("getsockopt -> invalid address");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }
    if (!(PosixSubsystem::checkAddress(
            reinterpret_cast<uintptr_t>(optvalue), *optlen,
            PosixSubsystem::SafeWrite)))
    {
        N_NOTICE("getsockopt -> invalid address");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    // Valid socket?
    Process *pProcess =
        Processor::information().getCurrentThread()->getParent();
    PosixSubsystem *pSubsystem =
        reinterpret_cast<PosixSubsystem *>(pProcess->getSubsystem());
    if (!pSubsystem)
    {
        ERROR("No subsystem for one or both of the processes!");
        return -1;
    }

    FileDescriptor *f = pSubsystem->getFileDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    // SO_ERROR etc
    /// \todo implement with lwIP functionality
    return -1;
}

int posix_sethostname(const char *name, size_t len)
{
    N_NOTICE("sethostname");

    if (!(PosixSubsystem::checkAddress(
            reinterpret_cast<uintptr_t>(name), len, PosixSubsystem::SafeRead)))
    {
        N_NOTICE(" -> invalid address");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    N_NOTICE("sethostname(" << String(name, len) << ")");

    /// \todo integrate this

    return 0;
}
