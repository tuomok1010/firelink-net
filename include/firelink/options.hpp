#ifndef SOCKET_OPTIONS_H
#define SOCKET_OPTIONS_H

#ifdef _WIN32
#include <WinSock2.h>
#include <MSWSock.h>
#elif defined(__linux__)
#include <sys/socket.h>
#include <netinet/in.h>
#endif

namespace firelink
{
  enum class SocketOptionLevel : int
  {
    // Supported by both platforms
    Socket = SOL_SOCKET, 
    Ip     = IPPROTO_IP,
    Ipv6   = IPPROTO_IPV6,
    Tcp    = IPPROTO_TCP,
    Udp    = IPPROTO_UDP

#ifdef _WIN32
    // Windows specific
    // add here...
#elif defined(__linux__)
    // Linux specific
    // add here...
#endif
  };

  enum class SocketOption : int
  {
    // Supported by both platforms 
    ReuseAddress         = SO_REUSEADDR,
    KeepAlive            = SO_KEEPALIVE,
    ReceiveBuffer        = SO_RCVBUF,
    SendBuffer           = SO_SNDBUF,
    Broadcast            = SO_BROADCAST,
    Linger               = SO_LINGER,

#ifdef _WIN32
    // Windows specific
    NoDelay              = TCP_NODELAY,
    DontLinger           = SO_DONTLINGER,
    UpdateAcceptContext  = SO_UPDATE_ACCEPT_CONTEXT,
    UpdateConnectContext = SO_UPDATE_CONNECT_CONTEXT
#elif defined(__linux__)
    // Linux specific
    // add here...
#endif
  };
}
#endif /* SOCKET_OPTIONS_H */
