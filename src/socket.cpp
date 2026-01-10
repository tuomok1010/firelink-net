#include "firelink/socket.hpp"

// Platform-specific implementation headers (in detail/)
#ifdef _WIN32
    #include <firelink/platform/windows/win_socket.hpp>
    using PlatformSocket = firelink::platform::WinSocket;
#elif defined(__linux__)
    #include <firelink/platform/linux/lin_socket.hpp>
    using PlatformSocket = firelink::platform::LinSocket;
#else
    #error "Firelink: Unsupported platform"
#endif


std::shared_ptr<firelink::Socket> firelink::Socket::create()
{
  return std::make_shared<PlatformSocket>();
}

firelink::ErrorCode firelink::Socket::initialize()
{
  return PlatformSocket::initialize();
}

firelink::ErrorCode firelink::Socket::release()
{
  return PlatformSocket::release();
}
