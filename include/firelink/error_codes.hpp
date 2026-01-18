#ifndef SOCKET_ERROR_CODES_H
#define SOCKET_ERROR_CODES_H

#ifdef _WIN32
#include <winerror.h>
#elif defined(__linux__)
#include <sys/socket.h>
#endif

namespace firelink
{
  enum class ErrorCode : int
  {
    // Library-specific
    SystemError = -1,
    PlatformNotSupported = -2,

#ifdef _WIN32
    // Windows specific
    Success                    = NO_ERROR,
    NotInitialized             = WSANOTINITIALISED,
    NotSupported               = WSAVERNOTSUPPORTED,
    ProcLimitReached           = WSAEPROCLIM,
    
    WouldBlock                 = WSAEWOULDBLOCK,
    InProgress                 = WSAEINPROGRESS,
    AlreadyInProgress          = WSAEALREADY,

    NotASocket                 = WSAENOTSOCK, 
    DestinationAddressRequired = WSAEDESTADDRREQ,
    MessageTooLong             = WSAEMSGSIZE,
    WrongProtocol              = WSAEPROTOTYPE,
    ProtocolOptionUnavailable  = WSAENOPROTOOPT,
    ProtocolNotSupported       = WSAEPROTONOSUPPORT,
    SocketTypeNotSupported     = WSAESOCKTNOSUPPORT,
    OperationNotSupported      = WSAEOPNOTSUPP,
    AddressFamilyNotSupported  = WSAEAFNOSUPPORT,

    AddressInUse               = WSAEADDRINUSE,
    AddressNotAvailable        = WSAEADDRNOTAVAIL,

    NetworkDown                = WSAENETDOWN,
    NetworkUnreachable         = WSAENETUNREACH,
    NetworkReset               = WSAENETRESET,
    ConnectionAborted          = WSAECONNABORTED,
    ConnectionReset            = WSAECONNRESET,
    NoBufferSpace              = WSAENOBUFS,

    AlreadyConnected           = WSAEISCONN,
    NotConnected               = WSAENOTCONN,
    SocketShutdown             = WSAESHUTDOWN,

    TimedOut                   = WSAETIMEDOUT,
    ConnectionRefused          = WSAECONNREFUSED,

    HostNotFound               = WSAHOST_NOT_FOUND,
    HostDown                   = WSAEHOSTDOWN,
    HostUnreachable            = WSAEHOSTUNREACH,

    InvalidArgument            = WSAEINVAL
    
#elif defined(__linux__)
    // Linux specific
    // add here...
#endif
  };
}  

#endif /* SOCKET_ERROR_CODES_H */
