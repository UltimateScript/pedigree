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

#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS  // don't need them here

#include "modules/system/vfs/File.h"
#include "modules/system/vfs/VFS.h"
#include "pedigree/kernel/process/Process.h"
#include "pedigree/kernel/processor/Processor.h"
#include "pedigree/kernel/processor/types.h"
#include "pedigree/kernel/process/Scheduler.h"
#include "pedigree/kernel/syscallError.h"
#include "pedigree/kernel/utilities/Tree.h"

#include "modules/system/lwip/include/lwip/api.h"
#include "modules/system/lwip/include/lwip/ip_addr.h"
#include "modules/system/lwip/include/lwip/tcp.h"

#include "pedigree/kernel/Subsystem.h"
#include <PosixSubsystem.h>
#include <FileDescriptor.h>
#include <UnixFilesystem.h>

#include "file-syscalls.h"
#include "net-syscalls.h"

#include <fcntl.h>

#ifndef UTILITY_LINUX
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#endif

#include <sys/un.h>
#include <netinet/in.h>

Tree<struct netconn *, LwipSocketSyscalls*> LwipSocketSyscalls::m_SyscallObjects;

extern UnixFilesystem *g_pUnixFilesystem;

netconnMetadata::netconnMetadata() : recv(0), send(0), error(false), lock(false), semaphores(), offset(0), pb(nullptr), buf(nullptr)
{
}

#ifdef UTILITY_LINUX
#include <vector>

std::vector<FileDescriptor *> g_Descriptors;

static FileDescriptor *getDescriptor(int socket)
{
    if (socket >= g_Descriptors.size())
    {
        return nullptr;
    }

    return g_Descriptors[socket];
}

static void addDescriptor(int socket, FileDescriptor *f)
{
    FileDescriptor *old = getDescriptor(socket);
    if (old)
    {
        delete old;
    }

    if (socket > g_Descriptors.capacity())
    {
        g_Descriptors.reserve(socket + 1);
    }

    g_Descriptors.insert(g_Descriptors.begin() + socket, f);
}

static size_t getAvailableDescriptor()
{
    return g_Descriptors.size();
}
#else
/// \todo move these into a common area, this code is duplicated EVERYWHERE
static PosixSubsystem *getSubsystem()
{
    Process *pProcess =
        Processor::information().getCurrentThread()->getParent();
    PosixSubsystem *pSubsystem =
        reinterpret_cast<PosixSubsystem *>(pProcess->getSubsystem());
    if (!pSubsystem)
    {
        ERROR("No subsystem for one or both of the processes!");
        return nullptr;
    }

    return pSubsystem;
}

static FileDescriptor *getDescriptor(int socket)
{
    PosixSubsystem *pSubsystem = getSubsystem();
    return pSubsystem->getFileDescriptor(socket);
}

static void addDescriptor(int socket, FileDescriptor *f)
{
    PosixSubsystem *pSubsystem = getSubsystem();
    pSubsystem->addFileDescriptor(socket, f);
}

static size_t getAvailableDescriptor()
{
    PosixSubsystem *pSubsystem = getSubsystem();
    return pSubsystem->getFd();
}
#endif

/// Pass is_create = true to indicate that the operation is permitted to
// operate if the socket does not yet have valid members (i.e. before a bind).
static bool isSaneSocket(FileDescriptor *f, bool is_create = false)
{
    if (!f)
    {
        N_NOTICE(" -> isSaneSocket: descriptor is null");
        SYSCALL_ERROR(BadFileDescriptor);
        return false;
    }

    if (is_create)
    {
        return true;
    }

    if (!f->networkImpl)
    {
        N_NOTICE(" -> isSaneSocket: no network implementation found");
        SYSCALL_ERROR(BadFileDescriptor);
        return false;
    }

    return true;
}

static err_t sockaddrToIpaddr(const struct sockaddr *saddr, uint16_t &port, ip_addr_t *result, bool isbind = true)
{
    ByteSet(result, 0, sizeof(*result));

    if (saddr->sa_family == AF_INET)
    {
        const struct sockaddr_in *sin = reinterpret_cast<const struct sockaddr_in *>(saddr);
        result->u_addr.ip4.addr = sin->sin_addr.s_addr;
        result->type = IPADDR_TYPE_V4;

        if (!isbind)
        {
            // do some extra sanity checks for client connections
            if (!sin->sin_addr.s_addr)
            {
                // rebind to 127.0.0.1 (localhost)
                result->u_addr.ip4.addr = HOST_TO_BIG32(INADDR_LOOPBACK);
            }
        }

        port = BIG_TO_HOST16(sin->sin_port);

        return ERR_OK;
    }
    else
    {
        ERROR("sockaddrToIpaddr: only AF_INET is supported at the moment.");
    }

    return ERR_VAL;
}



int posix_socket(int domain, int type, int protocol)
{
    N_NOTICE("socket(" << domain << ", " << type << ", " << protocol << ")");

    size_t fd = getAvailableDescriptor();

    netconn_type connType = NETCONN_INVALID;

    File *file = nullptr;
    struct netconn *conn = nullptr;
    bool valid = true;

    NetworkSyscalls *syscalls;

    if (domain == AF_UNIX)
    {
        syscalls = new UnixSocketSyscalls(domain, type, protocol);
    }
    else
    {
        /// \todo handle non-lwIP domains
        syscalls = new LwipSocketSyscalls(domain, type, protocol);
    }

    if (!syscalls->create())
    {
        return -1;
    }

    FileDescriptor *f = new FileDescriptor;
    f->networkImpl = syscalls;
    f->fd = fd;
    addDescriptor(fd, f);
    syscalls->associate(f);

    N_NOTICE("  -> " << Dec << fd << Hex);
    return static_cast<int>(fd);
}

int posix_socketpair(int domain, int type, int protocol, int sv[2])
{
    N_NOTICE("socketpair");

    if (!PosixSubsystem::checkAddress(
            reinterpret_cast<uintptr_t>(sv), sizeof(int) * 2,
            PosixSubsystem::SafeWrite))
    {
        N_NOTICE("socketpair -> invalid address");
        SYSCALL_ERROR(BadAddress);
        return -1;
    }

    if (domain != AF_UNIX)
    {
        /// \todo syscall error for EAFNOSUPPORT
        return -1;
    }

    UnixSocketSyscalls *syscallsA = new UnixSocketSyscalls(domain, type, protocol);
    if (!syscallsA->create())
    {
        delete syscallsA;
        return -1;
    }

    UnixSocketSyscalls *syscallsB = new UnixSocketSyscalls(domain, type, protocol);
    if (!syscallsB->create())
    {
        delete syscallsA;
        delete syscallsB;
        return -1;
    }

    if (!syscallsA->pairWith(syscallsB))
    {
        delete syscallsA;
        delete syscallsB;
        return -1;
    }

    FileDescriptor *fA = new FileDescriptor;
    FileDescriptor *fB = new FileDescriptor;

    size_t fdA = getAvailableDescriptor();
    size_t fdB = getAvailableDescriptor();

    fA->networkImpl = syscallsA;
    fA->fd = fdA;
    fB->networkImpl = syscallsB;
    fB->fd = fdB;

    addDescriptor(fdA, fA);
    addDescriptor(fdB, fB);

    syscallsA->associate(fA);
    syscallsB->associate(fB);

    sv[0] = static_cast<int>(fdA);
    sv[1] = static_cast<int>(fdB);

    return 0;
}

int posix_connect(int sock, const struct sockaddr *address, socklen_t addrlen)
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
        "connect(" << sock << ", " << reinterpret_cast<uintptr_t>(address)
                         << ", " << addrlen << ")");

    FileDescriptor *f = getDescriptor(sock);
    if (!isSaneSocket(f, true))
    {
        return -1;
    }

    if (address->sa_family != f->networkImpl->getDomain())
    {
        // EAFNOSUPPORT
        N_NOTICE(" -> incorrect address family passed to connect()");
        return -1;
    }

    return f->networkImpl->connect(address, addrlen);
}

ssize_t posix_send(int sock, const void *buff, size_t bufflen, int flags)
{
    N_NOTICE("send");

    if (!PosixSubsystem::checkAddress(
            reinterpret_cast<uintptr_t>(buff), bufflen,
            PosixSubsystem::SafeRead))
    {
        N_NOTICE("send -> invalid address");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    N_NOTICE(
        "send(" << sock << ", " << buff << ", " << bufflen << ", "
                      << flags << ")");

    if (buff && bufflen)
    {
        String debug;
        debug.assign(reinterpret_cast<const char *>(buff), bufflen, true);
        N_NOTICE(" -> sending: '" << debug << "'");
    }

    FileDescriptor *f = getDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    return f->networkImpl->sendto(buff, bufflen, flags, nullptr, 0);
}

ssize_t posix_sendto(
    int sock, const void *buff, size_t bufflen, int flags, struct sockaddr *address,
    socklen_t addrlen)
{
    N_NOTICE("sendto");

    if (!PosixSubsystem::checkAddress(
            reinterpret_cast<uintptr_t>(buff), bufflen,
            PosixSubsystem::SafeRead))
    {
        N_NOTICE("sendto -> invalid address for transmission buffer");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    N_NOTICE(
        "sendto(" << sock << ", " << buff << ", " << bufflen << ", "
                        << flags << ", " << address << ", " << addrlen << ")");

    if (buff && bufflen)
    {
        String debug;
        debug.assign(reinterpret_cast<const char *>(buff), bufflen, true);
        N_NOTICE(" -> sending: '" << debug << "'");
    }

    FileDescriptor *f = getDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    return f->networkImpl->sendto(buff, bufflen, flags, address, addrlen);
}

ssize_t posix_recv(int sock, void *buff, size_t bufflen, int flags)
{
    N_NOTICE("recv");

    if (!PosixSubsystem::checkAddress(
            reinterpret_cast<uintptr_t>(buff), bufflen,
            PosixSubsystem::SafeWrite))
    {
        N_NOTICE("recv -> invalid address");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    N_NOTICE(
        "recv(" << sock << ", " << buff << ", " << bufflen << ", "
                      << flags << ")");

    FileDescriptor *f = getDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    ssize_t n = f->networkImpl->recvfrom(buff, bufflen, flags, nullptr, nullptr);

    if (buff && n > 0)
    {
        String debug;
        debug.assign(reinterpret_cast<const char *>(buff), n, true);
        N_NOTICE(" -> received: '" << debug << "'");
    }

    N_NOTICE(" -> " << n);
    return n;
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
        "recvfrom(" << sock << ", " << buff << ", " << bufflen << ", "
                          << flags << ", " << address << ", " << addrlen);

    FileDescriptor *f = getDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    ssize_t n = f->networkImpl->recvfrom(buff, bufflen, flags, address, addrlen);

    if (buff && n > 0)
    {
        String debug;
        debug.assign(reinterpret_cast<const char *>(buff), n, true);
        N_NOTICE(" -> received: '" << debug << "'");
    }

    N_NOTICE(" -> " << n);
    return n;
}

int posix_bind(int sock, const struct sockaddr *address, socklen_t addrlen)
{
    N_NOTICE("bind");

    if (!PosixSubsystem::checkAddress(
            reinterpret_cast<uintptr_t>(address), addrlen,
            PosixSubsystem::SafeRead))
    {
        N_NOTICE("bind -> invalid address");
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    N_NOTICE(
        "bind(" << sock << ", " << address << ", " << addrlen << ")");

    FileDescriptor *f = getDescriptor(sock);
    if (!isSaneSocket(f, true))
    {
        return -1;
    }

    if (f->networkImpl->getDomain() != address->sa_family)
    {
        // EAFNOSUPPORT
        return -1;
    }

    return f->networkImpl->bind(address, addrlen);
}

int posix_listen(int sock, int backlog)
{
    N_NOTICE("listen(" << sock << ", " << backlog << ")");

    FileDescriptor *f = getDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    if (f->networkImpl->getType() != SOCK_STREAM)
    {
        SYSCALL_ERROR(InvalidArgument);
        return -1;
    }

    return f->networkImpl->listen(backlog);
}

int posix_accept(int sock, struct sockaddr *address, socklen_t *addrlen)
{
    N_NOTICE("accept");

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
        "accept(" << sock << ", " << address << ", " << addrlen << ")");

    FileDescriptor *f = getDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    if (f->networkImpl->getType() != SOCK_STREAM)
    {
        SYSCALL_ERROR(OperationNotSupported);
        return -1;
    }

    return f->networkImpl->accept(address, addrlen);
}

int posix_shutdown(int socket, int how)
{
    N_NOTICE("shutdown(" << socket << ", " << how << ")");

    FileDescriptor *f = getDescriptor(socket);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    return f->networkImpl->shutdown(how);
}

int posix_getpeername(
    int socket, struct sockaddr *address, socklen_t *address_len)
{
    N_NOTICE("getpeername");

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
        "getpeername(" << socket << ", " << address << ", " << address_len
                             << ")");

    FileDescriptor *f = getDescriptor(socket);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    return f->networkImpl->getpeername(address, address_len);
}

int posix_getsockname(
    int socket, struct sockaddr *address, socklen_t *address_len)
{
    N_NOTICE("getsockname");

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
        "getsockname(" << socket << ", " << address << ", " << address_len
                             << ")");

    FileDescriptor *f = getDescriptor(socket);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    return f->networkImpl->getsockname(address, address_len);
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

    FileDescriptor *f = getDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    return f->networkImpl->setsockopt(level, optname, optvalue, optlen);
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

    FileDescriptor *f = getDescriptor(sock);
    if (!isSaneSocket(f))
    {
        return -1;
    }

    return f->networkImpl->getsockopt(level, optname, optvalue, optlen);
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

NetworkSyscalls::NetworkSyscalls(int domain, int type, int protocol) : m_Domain(domain), m_Type(type), m_Protocol(protocol), m_Fd(nullptr) {}

NetworkSyscalls::~NetworkSyscalls() = default;

bool NetworkSyscalls::create()
{
    return true;
}

int NetworkSyscalls::shutdown(int how)
{
    return 0;
}

bool NetworkSyscalls::canPoll() const
{
    return false;
}

bool NetworkSyscalls::poll(bool &read, bool &write, bool &error, Semaphore *waiter)
{
    return false;
}

void NetworkSyscalls::unPoll(Semaphore *waiter)
{
}

void NetworkSyscalls::associate(FileDescriptor *fd)
{
    m_Fd = fd;
}

bool NetworkSyscalls::isBlocking() const
{
    return !((m_Fd->flflags & O_NONBLOCK) == O_NONBLOCK);
}


LwipSocketSyscalls::LwipSocketSyscalls(int domain, int type, int protocol) : NetworkSyscalls(domain, type, protocol), m_Socket(nullptr), m_Metadata() {}

LwipSocketSyscalls::~LwipSocketSyscalls()
{
    if (m_Socket)
    {
        m_SyscallObjects.remove(m_Socket);

        netconn_delete(m_Socket);
        m_Socket = nullptr;
    }
}

bool LwipSocketSyscalls::create()
{
    netconn_type connType = NETCONN_INVALID;

    // fix up some defaults that make sense for inet[6] sockets
    if (!m_Protocol)
    {
        N_NOTICE("LwipSocketSyscalls: using default protocol for socket type");
        if (m_Type == SOCK_DGRAM)
        {
            m_Protocol = IPPROTO_UDP;
        }
        else if (m_Type == SOCK_STREAM)
        {
            m_Protocol = IPPROTO_TCP;
        }
    }

    if (m_Domain == AF_INET)
    {
        switch (m_Protocol)
        {
            case IPPROTO_TCP:
                connType = NETCONN_TCP;
                break;
            case IPPROTO_UDP:
                connType = NETCONN_UDP;
                break;
        }
    }
    else if (m_Domain == AF_INET6)
    {
        switch (m_Protocol)
        {
            case IPPROTO_TCP:
                connType = NETCONN_TCP_IPV6;
                break;
            case IPPROTO_UDP:
                connType = NETCONN_UDP_IPV6;
                break;
        }
    }
    else if (m_Domain == AF_PACKET)
    {
        connType = NETCONN_RAW;
    }
    else
    {
        WARNING("LwipSocketSyscalls: domain " << m_Domain << " is not known!");
        SYSCALL_ERROR(InvalidArgument);
        return false;
    }

    if (connType == NETCONN_INVALID)
    {
        N_NOTICE("LwipSocketSyscalls: invalid socket creation parameters");
        SYSCALL_ERROR(InvalidArgument);
        return false;
    }

    // Socket already exists? No need to do the rest.
    if (m_Socket)
    {
        return true;
    }

    m_Socket = netconn_new_with_callback(connType, netconnCallback);
    if (!m_Socket)
    {
        /// \todo need an error here...
        return false;
    }

    m_SyscallObjects.insert(m_Socket, this);

    return true;
}

int LwipSocketSyscalls::connect(const struct sockaddr *address, socklen_t addrlen)
{
    /// \todo need to track if we've already done a bind() and not bind if so
    ip_addr_t ipaddr;
    ByteSet(&ipaddr, 0, sizeof(ipaddr));
    err_t err = netconn_bind(m_Socket, &ipaddr, 0);  // bind to any address
    if (err != ERR_OK)
    {
        N_NOTICE(" -> lwip error when binding before connect");
        lwipToSyscallError(err);
        return -1;
    }

    uint16_t port = 0;
    if ((err = sockaddrToIpaddr(address, port, &ipaddr, false)) != ERR_OK)
    {
        N_NOTICE("failed to convert sockaddr");
        lwipToSyscallError(err);
        return -1;
    }

    // set blocking status if needed
    bool blocking = !((getFileDescriptor()->flflags & O_NONBLOCK) == O_NONBLOCK);
    netconn_set_nonblocking(m_Socket, blocking ? 0 : 1);

    N_NOTICE("using socket " << m_Socket << "!");
    N_NOTICE(" -> connecting to remote " << ipaddr_ntoa(&ipaddr) << " on port " << Dec << port);

    err = netconn_connect(m_Socket, &ipaddr, port);
    if (err != ERR_OK)
    {
        N_NOTICE(" -> lwip error");
        lwipToSyscallError(err);
        return -1;
    }

    return 0;
}

ssize_t LwipSocketSyscalls::sendto(const void *buffer, size_t bufferlen, int flags, const struct sockaddr *address, socklen_t addrlen)
{
    err_t err;

    if (address)
    {
        /// \todo need to build this - but netconn_sendto() requires a netbuf
        SYSCALL_ERROR(Unimplemented);
        return -1;
    }

    // Can we send without blocking?
    if (!isBlocking() && !m_Metadata.send)
    {
        N_NOTICE(" -> send queue full, would block");
        SYSCALL_ERROR(NoMoreProcesses);
        return -1;
    }

    // Copy buffer from userspace before passing it into lwIP.
    char *data = new char[bufferlen];
    MemoryCopy(data, buffer, bufferlen);

    size_t bytesWritten = 0;
    err = netconn_write_partly(m_Socket, data, bufferlen, NETCONN_COPY | NETCONN_MORE, &bytesWritten);
    if (err != ERR_OK)
    {
        delete [] data;
        lwipToSyscallError(err);
        return -1;
    }

    delete [] data;

    return bytesWritten;
}

ssize_t LwipSocketSyscalls::recvfrom(void *buffer, size_t bufferlen, int flags, struct sockaddr *address, socklen_t *addrlen)
{
    if (address)
    {
        /// \todo need to build this - extract from the pbuf
        SYSCALL_ERROR(Unimplemented);
        return -1;
    }

    // No data to read right now.
    if (!isBlocking())
    {
        if (!(m_Metadata.recv || m_Metadata.pb))
        {
            // If an app tightly calls recv() and keeps hitting here, it'll
            // burn a lot of cycles for no good reason. Instead, reschedule to
            // reduce that tight spin.
            Scheduler::instance().yield();

            N_NOTICE(" -> no more data available, would block");
            SYSCALL_ERROR(NoMoreProcesses);
            return -1;
        }
    }

    err_t err;
    if (!m_Metadata.pb)
    {
        struct pbuf *pb = nullptr;
        struct netbuf *buf = nullptr;

        // No partial data present from a previous read. Read new data from
        // the socket.
        if (NETCONNTYPE_GROUP(netconn_type(m_Socket)) == NETCONN_TCP)
        {
            err = netconn_recv_tcp_pbuf(m_Socket, &pb);
        }
        else
        {
            err = netconn_recv(m_Socket, &buf);
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

        m_Metadata.offset = 0;
        m_Metadata.pb = pb;
        m_Metadata.buf = buf;
    }

    // now we read some things.
    size_t len = m_Metadata.offset + bufferlen;
    if (len > m_Metadata.pb->tot_len)
    {
        len = m_Metadata.pb->tot_len - m_Metadata.offset;
    }

    pbuf_copy_partial(m_Metadata.pb, buffer, len, m_Metadata.offset);

    // partial read?
    if ((m_Metadata.offset + len) < m_Metadata.pb->tot_len)
    {
        m_Metadata.offset += len;
    }
    else
    {
        if (m_Metadata.buf == nullptr)
        {
            pbuf_free(m_Metadata.pb);
        }
        else
        {
            // will indirectly clean up m_Metadata.pb as it's a member of the netbuf
            netbuf_free(m_Metadata.buf);
        }

        m_Metadata.pb = nullptr;
        m_Metadata.buf = nullptr;
        m_Metadata.offset = 0;
    }

    N_NOTICE(" -> " << len);
    return len;
}

int LwipSocketSyscalls::listen(int backlog)
{
    err_t err = netconn_listen_with_backlog(m_Socket, backlog);
    if (err != ERR_OK)
    {
        N_NOTICE(" -> lwIP error");
        lwipToSyscallError(err);
        return -1;
    }

    return 0;
}

int LwipSocketSyscalls::bind(const struct sockaddr *address, socklen_t addrlen)
{
    uint16_t port = 0;
    ip_addr_t ipaddr;
    sockaddrToIpaddr(address, port, &ipaddr);

    err_t err = netconn_bind(m_Socket, &ipaddr, port);
    if (err != ERR_OK)
    {
        N_NOTICE(" -> lwIP error");
        lwipToSyscallError(err);
        return -1;
    }

    return 0;
}

int LwipSocketSyscalls::accept(struct sockaddr *address, socklen_t *addrlen)
{
    struct netconn *new_conn;
    err_t err = netconn_accept(m_Socket, &new_conn);
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

    LwipSocketSyscalls *obj = new LwipSocketSyscalls(m_Domain, m_Type, m_Protocol);
    obj->m_Socket = new_conn;
    obj->create();
    m_SyscallObjects.insert(new_conn, obj);

    size_t fd = getAvailableDescriptor();
    FileDescriptor *desc = new FileDescriptor;
    desc->networkImpl = obj;
    desc->fd = fd;

    addDescriptor(fd, desc);
    obj->associate(desc);

    return static_cast<int>(fd);
}

int LwipSocketSyscalls::shutdown(int how)
{
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

    err_t err = netconn_shutdown(m_Socket, rx, tx);
    if (err != ERR_OK)
    {
        lwipToSyscallError(err);
        return -1;
    }

    return 0;
}

int LwipSocketSyscalls::getpeername(struct sockaddr *address, socklen_t *address_len)
{
    ip_addr_t peer;
    uint16_t port;
    err_t err = netconn_peer(m_Socket, &peer, &port);
    if (err != ERR_OK)
    {
        N_NOTICE(" -> getpeername failed");
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

int LwipSocketSyscalls::getsockname(struct sockaddr *address, socklen_t *address_len)
{
    ip_addr_t self;
    uint16_t port;
    err_t err = netconn_addr(m_Socket, &self, &port);
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

int LwipSocketSyscalls::setsockopt(int level, int optname, const void *optvalue, socklen_t optlen)
{
    if (m_Protocol == IPPROTO_TCP && level == IPPROTO_TCP)
    {
#ifdef TCP_NODELAY
        if (optname == TCP_NODELAY)
        {
            N_NOTICE(" -> TCP_NODELAY");
            const uint32_t *val = reinterpret_cast<const uint32_t *>(optvalue);
            N_NOTICE("  --> val=" << *val);

            // TCP_NODELAY controls Nagle's algorithm usage
            if (*val)
            {
                tcp_nagle_disable(m_Socket->pcb.tcp);
            }
            else
            {
                tcp_nagle_enable(m_Socket->pcb.tcp);
            }

            return 0;
        }
#endif
    }

    /// \todo implement with lwIP functionality
    return -1;
}

int LwipSocketSyscalls::getsockopt(int level, int optname, void *optvalue, socklen_t *optlen)
{
    // SO_ERROR etc
    /// \todo implement with lwIP functionality
    return -1;
}

bool LwipSocketSyscalls::canPoll() const
{
    return true;
}

bool LwipSocketSyscalls::poll(bool &read, bool &write, bool &error, Semaphore *waiter)
{
    bool ok = false;

    if (!(read || write || error))
    {
        // not actually polling for anything
        return true;
    }

#ifdef THREADS
    m_Metadata.lock.acquire();
#endif

    if (write)
    {
        write = m_Metadata.send != 0;
        ok = ok || write;
    }

    if (read)
    {
        read = m_Metadata.recv || m_Metadata.pb;
        ok = ok || read;
    }

    if (error)
    {
        error = m_Metadata.error;
        ok = ok || error;
    }

    if (waiter && !ok)
    {
        // Need to wait for socket data.
        /// \todo this is buggy as it'll return for the wrong events!
        m_Metadata.semaphores.pushBack(waiter);
    }

#ifdef THREADS
    m_Metadata.lock.release();
#endif

    return ok;
}

void LwipSocketSyscalls::unPoll(Semaphore *waiter)
{
#ifdef THREADS
    m_Metadata.lock.acquire();
    for (auto it = m_Metadata.semaphores.begin(); it != m_Metadata.semaphores.end();)
    {
        if ((*it) == waiter)
        {
            it = m_Metadata.semaphores.erase(it);
        }
        else
        {
            ++it;
        }
    }
    m_Metadata.lock.release();
#endif
}

void LwipSocketSyscalls::netconnCallback(struct netconn *conn, enum netconn_evt evt, u16_t len)
{
    LwipSocketSyscalls *obj = m_SyscallObjects.lookup(conn);
    if (!obj)
    {
        return;
    }

#ifdef THREADS
    obj->m_Metadata.lock.acquire();
#endif

    switch(evt)
    {
        case NETCONN_EVT_RCVPLUS:
            N_NOTICE("RCV+");
            ++(obj->m_Metadata.recv);
            break;
        case NETCONN_EVT_RCVMINUS:
            N_NOTICE("RCV-");
            if (obj->m_Metadata.recv)
            {
                --(obj->m_Metadata.recv);
            }
            break;
        case NETCONN_EVT_SENDPLUS:
            N_NOTICE("SND+");
            obj->m_Metadata.send = 1;
            break;
        case NETCONN_EVT_SENDMINUS:
            N_NOTICE("SND-");
            obj->m_Metadata.send = 0;
            break;
        case NETCONN_EVT_ERROR:
            N_NOTICE("ERR");
            obj->m_Metadata.error = true;  /// \todo figure out how to bubble errors
            break;
        default:
            N_NOTICE("Unknown netconn callback error.");
    }

    /// \todo need a way to do this with lwip when threads are off
#ifdef THREADS
    for (auto &it : obj->m_Metadata.semaphores)
    {
        it->release();
    }

    obj->m_Metadata.lock.release();
#endif
}

void LwipSocketSyscalls::lwipToSyscallError(err_t err)
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

LwipSocketSyscalls::LwipMetadata::LwipMetadata() : recv(0), send(0), error(false), lock(false), semaphores(), offset(0), pb(nullptr), buf(nullptr)
{
}

UnixSocketSyscalls::UnixSocketSyscalls(int domain, int type, int protocol) : NetworkSyscalls(domain, type, protocol), m_Socket(nullptr), m_Remote(nullptr), m_LocalPath(), m_RemotePath() {}

UnixSocketSyscalls::~UnixSocketSyscalls()
{
    delete m_Socket;
}

bool UnixSocketSyscalls::create()
{
    if (m_Socket)
    {
        return true;
    }

    // Create an unnamed unix socket by default.
    m_Socket = new UnixSocket(String(), g_pUnixFilesystem, nullptr);

    return true;
}

int UnixSocketSyscalls::connect(const struct sockaddr *address, socklen_t addrlen)
{
    // Find the remote socket
    const struct sockaddr_un *un =
        reinterpret_cast<const struct sockaddr_un *>(address);
    String pathname;
    normalisePath(pathname, un->sun_path);

    N_NOTICE(" -> unix connect: '" << pathname << "'");

    File *file = VFS::instance().find(pathname);
    if (!file)
    {
        SYSCALL_ERROR(DoesNotExist);
        N_NOTICE(" -> unix socket '" << pathname << "' doesn't exist");
        return -1;
    }

    if (!file->isSocket())
    {
        /// \todo wrong error
        SYSCALL_ERROR(DoesNotExist);
        N_NOTICE(
            " -> target '" << pathname << "' is not a unix socket");
        return -1;
    }

    m_Remote = static_cast<UnixSocket *>(file);

    if (getType() == SOCK_STREAM)
    {
        // Create the remote for accept() on the server side.
        UnixSocket *remote = new UnixSocket(String(), g_pUnixFilesystem, nullptr);
        m_Remote->addSocket(remote);

        // Bind our local socket to the remote side
        m_Socket->bind(remote);
    }

    m_RemotePath = pathname;

    return 0;
}

ssize_t UnixSocketSyscalls::sendto(const void *buffer, size_t bufferlen, int flags, const struct sockaddr *address, socklen_t addrlen)
{
    UnixSocket *remote = m_Remote;
    if (getType() == SOCK_STREAM)
    {
        /// \todo this doesn't handle a connection going away - only a connection
        /// not being made in the first place (I think it's a different errno)
        if (!m_Socket->getOther())
        {
            SYSCALL_ERROR(NotConnected);
            return -1;
        }

        // streaming sockets already do the right thing if they're connected
        remote = m_Socket;
    }

    if (!m_Remote)
    {
        if (getType() == SOCK_STREAM)
        {
            // sendto() can't be used for streaming sockets
            /// \todo errno
            return -1;
        }
        else if (!address)
        {
            /// \todo needs some sort of errno here
            return -1;
        }

        // Find the remote socket
        const struct sockaddr_un *un =
            reinterpret_cast<const struct sockaddr_un *>(address);
        String pathname;
        normalisePath(pathname, un->sun_path);

        N_NOTICE(" -> unix connect: '" << pathname << "'");

        File *file = VFS::instance().find(pathname);
        if (!file)
        {
            SYSCALL_ERROR(DoesNotExist);
            N_NOTICE(" -> unix socket '" << pathname << "' doesn't exist");
            return -1;
        }

        if (!file->isSocket())
        {
            /// \todo wrong error
            SYSCALL_ERROR(DoesNotExist);
            N_NOTICE(
                " -> target '" << pathname << "' is not a unix socket");
            return -1;
        }

        remote = static_cast<UnixSocket *>(file);
    }

    /// \todo this is incorrect for streaming sockets (m_Remote is the server peer not the accepted socket)
    return remote->write(
        reinterpret_cast<uintptr_t>(
            static_cast<const char *>(m_LocalPath)),
        bufferlen, reinterpret_cast<uintptr_t>(buffer),
        isBlocking());
}

ssize_t UnixSocketSyscalls::recvfrom(void *buffer, size_t bufferlen, int flags, struct sockaddr *address, socklen_t *addrlen)
{
    /// \todo drop in peer if we're a streaming socket
    String remote;
    uint64_t numRead = m_Socket->recvfrom(
        bufferlen, reinterpret_cast<uintptr_t>(buffer),
        isBlocking(), remote);

    if (numRead && address)
    {
        struct sockaddr_un *un =
            reinterpret_cast<struct sockaddr_un *>(address);
        un->sun_family = AF_UNIX;
        StringCopy(un->sun_path, remote);
        *addrlen = sizeof(sa_family_t) + remote.length();
    }

    return numRead;
}

int UnixSocketSyscalls::listen(int backlog)
{
    /// \todo track that we're now listening
    /// \todo maybe don't allow listening on an unnamed socket
    return 0;
}

int UnixSocketSyscalls::bind(const struct sockaddr *address, socklen_t addrlen)
{
    /// \todo unbind existing socket if one exists.

    // Valid state. But no socket, so do the magic here.
    const struct sockaddr_un *un =
        reinterpret_cast<const struct sockaddr_un *>(address);

    if (SUN_LEN(un) == sizeof(sa_family_t))
    {
        /// \todo re-bind an unnamed address if we are bound already
        return 0;
    }

    String adjusted_pathname;
    normalisePath(adjusted_pathname, un->sun_path);

    N_NOTICE(" -> unix bind: '" << adjusted_pathname << "'");

    File *cwd = VFS::instance().find(String("."));
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

    /// \todo does this actually create a findable file?
    UnixSocket *socket = new UnixSocket(
        basename, parentDirectory->getFilesystem(), parentDirectory);
    if (!pDir->addEphemeralFile(socket))
    {
        /// \todo errno?
        delete socket;
        return false;
    }

    N_NOTICE(" -> basename=" << basename);

    // bind() then connect().
    if (!m_LocalPath.length())
    {
        // just an unnamed socket, safe to delete.
        delete m_Socket;
    }

    m_Socket = socket;
    m_LocalPath = adjusted_pathname;

    return 0;
}

int UnixSocketSyscalls::accept(struct sockaddr *address, socklen_t *addrlen)
{
    UnixSocket *remote = m_Socket->getSocket(isBlocking());
    if (!remote)
    {
        SYSCALL_ERROR(NoMoreProcesses);
        return -1;
    }

    if (remote)
    {
        struct sockaddr_un *sun =
            reinterpret_cast<struct sockaddr_un *>(address);

        if (remote->getName().length())
        {
            // Named.
            String name = remote->getFullPath();

            StringCopy(sun->sun_path, name);
            *addrlen = sizeof(sa_family_t) + name.length();
        }
        else
        {
            *addrlen = sizeof(sa_family_t);
        }

        sun->sun_family = AF_UNIX;

        UnixSocketSyscalls *obj = new UnixSocketSyscalls(m_Domain, m_Type, m_Protocol);
        obj->m_Socket = remote;
        obj->m_Remote = remote->getOther();
        obj->m_LocalPath = String();
        obj->m_RemotePath = m_Socket->getFullPath();
        obj->create();

        size_t fd = getAvailableDescriptor();
        FileDescriptor *desc = new FileDescriptor;
        desc->networkImpl = obj;
        desc->fd = fd;

        addDescriptor(fd, desc);
        obj->associate(desc);

        return static_cast<int>(fd);
    }

    return -1;
}

int UnixSocketSyscalls::shutdown(int how)
{
    /// \todo
    return 0;
}

int UnixSocketSyscalls::getpeername(struct sockaddr *address, socklen_t *address_len)
{
    struct sockaddr_un *sun =
        reinterpret_cast<struct sockaddr_un *>(address);
    StringCopy(sun->sun_path, m_RemotePath);
    *address_len = sizeof(sa_family_t) + m_RemotePath.length();

    return 0;
}

int UnixSocketSyscalls::getsockname(struct sockaddr *address, socklen_t *address_len)
{
    struct sockaddr_un *sun =
        reinterpret_cast<struct sockaddr_un *>(address);
    StringCopy(sun->sun_path, m_LocalPath);
    *address_len = sizeof(sa_family_t) + m_LocalPath.length();

    return 0;
}

int UnixSocketSyscalls::setsockopt(int level, int optname, const void *optvalue, socklen_t optlen)
{
    // nothing to do here
    return -1;
}

int UnixSocketSyscalls::getsockopt(int level, int optname, void *optvalue, socklen_t *optlen)
{
    // nothing to do here
    return -1;
}
