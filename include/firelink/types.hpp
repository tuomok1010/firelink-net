#ifndef FIRELINK_TYPES_H
#define FIRELINK_TYPES_H

#ifdef _WIN32
#include <WinSock2.h>
#elif defined(__linux__)
#include <sys/socket.h>
#endif

namespace firelink
{
#ifdef _WIN32
  // Windows specific
  using NativeHandle = SOCKET;
#elif defined(__linux__)
  // Linux specific
  using NativeHandle = int;
#endif
  
  enum class Operation : int
  {
    Unknown,
    Accept,
    Connect,
    Recv,
    RecvFrom,
    Send,
    SendTo,
    Disconnect
  };

  enum class AddressFamily : int
  {
    // Supported by both platforms
    NotSupported = -1,
    Unspecified =  AF_UNSPEC,
    IPv4 =         AF_INET,
    IPv6 =         AF_INET6

#ifdef _WIN32
    // Windows specific
    // add here...
#elif defined(__linux__)
    // Linux specific
    // add here...
#endif
  };

  enum class SocketType : int
  {
    // Supported by both platforms
    NotSupported = -1,
    Stream =       SOCK_STREAM,
    Datagram =     SOCK_DGRAM

#ifdef _WIN32
    // Windows specific
    // add here...
#elif defined(__linux__)
    // Linux specific
    // add here...
#endif
  };

  enum class Protocol : int
  {
    // Supported by both platforms
    NotSupported = -1,
    Tcp =          IPPROTO_TCP,
    Udp =          IPPROTO_UDP

#ifdef _WIN32
    // Windows specific
    // add here...
#elif defined(__linux__)
    // Linux specific
    // add here...
#endif
  };

  enum class ShutdownHow : int
  {
    // Supported by both platforms
    NotSupported = -1,

#ifdef _WIN32
    // Windows specific
    Read =         SD_RECEIVE,
    Write =        SD_SEND,
    Both =         SD_BOTH
#elif defined(__linux__)
    // Linux specific
    Read =         SHUT_RD,
    Write =        SHUT_WR,
    Both =         SHUT_RDWR
#endif

  };
}

#endif /* FIRELINK_TYPES_H */
