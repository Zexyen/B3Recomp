/* Linux build only: Winsock names over BSD sockets. WSAGetLastError is errno, so the WSAE* codes are the errno values. */
#ifndef RECOMP_POSIX_WINSOCK2_H
#define RECOMP_POSIX_WINSOCK2_H
#include "plat.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

typedef int SOCKET;
typedef unsigned long u_long;
typedef unsigned short u_short;
typedef struct linger LINGER;
typedef struct pollfd WSAPOLLFD;
typedef struct { WORD wVersion, wHighVersion; char szDescription[257], szSystemStatus[129]; } WSADATA;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define WSAStartup(v, d) 0
#define WSACleanup() 0
#define WSAGetLastError() errno
#define closesocket close
#define ioctlsocket(s, cmd, arg) ioctl((s), (cmd), (arg))
#define WSAPoll(fds, n, ms) poll((fds), (nfds_t)(n), (ms))
#define InetPtonA inet_pton
#define WSAEINTR EINTR
#define WSAEBADF EBADF
#define WSAEACCES EACCES
#define WSAEFAULT EFAULT
#define WSAEINVAL EINVAL
#define WSAEMFILE EMFILE
#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAEINPROGRESS EINPROGRESS
#define WSAEALREADY EALREADY
#define WSAENOTSOCK ENOTSOCK
#define WSAEDESTADDRREQ EDESTADDRREQ
#define WSAEMSGSIZE EMSGSIZE
#define WSAEPROTOTYPE EPROTOTYPE
#define WSAENOPROTOOPT ENOPROTOOPT
#define WSAEPROTONOSUPPORT EPROTONOSUPPORT
#define WSAESOCKTNOSUPPORT ESOCKTNOSUPPORT
#define WSAEOPNOTSUPP EOPNOTSUPP
#define WSAEPFNOSUPPORT EPFNOSUPPORT
#define WSAEAFNOSUPPORT EAFNOSUPPORT
#define WSAEADDRINUSE EADDRINUSE
#define WSAEADDRNOTAVAIL EADDRNOTAVAIL
#define WSAENETDOWN ENETDOWN
#define WSAENETUNREACH ENETUNREACH
#define WSAENETRESET ENETRESET
#define WSAECONNABORTED ECONNABORTED
#define WSAECONNRESET ECONNRESET
#define WSAENOBUFS ENOBUFS
#define WSAEISCONN EISCONN
#define WSAENOTCONN ENOTCONN
#define WSAESHUTDOWN ESHUTDOWN
#define WSAETIMEDOUT ETIMEDOUT
#define WSAECONNREFUSED ECONNREFUSED
#define WSAEHOSTDOWN EHOSTDOWN
#define WSAEHOSTUNREACH EHOSTUNREACH
/* resolver failures (getaddrinfo's EAI_* codes, which the code compares the result of getaddrinfo with) */
#define WSAHOST_NOT_FOUND EAI_NONAME
#define WSATRY_AGAIN EAI_AGAIN
#define WSANO_RECOVERY EAI_FAIL
#define WSATYPE_NOT_FOUND EAI_SERVICE
#define WSA_NOT_ENOUGH_MEMORY EAI_MEMORY
#endif
