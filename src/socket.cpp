#include "firelink/socket.hpp"

// Platform-specific implementation headers
#ifdef _WIN32
    #include <firelink/platform/windows/win_socket.hpp>
#elif defined(__linux__)
    #include <firelink/platform/linux/lin_socket.hpp>
#else
    #error "Firelink: Unsupported platform"
#endif

firelink::Socket::Socket(std::shared_ptr<firelink::IOCore> io_core) : io_core_(io_core)
{
  
}

std::expected<std::shared_ptr<firelink::Socket>, firelink::ErrorCode>
firelink::Socket::create(std::shared_ptr<firelink::IOCore> io_core)
{
  if (!io_core)
  {
    return std::unexpected(ErrorCode::InvalidArgument);
  }
  
#if defined(_WIN32)
  auto sock = std::make_shared<platform::WinSocket>(io_core);
#elif defined(__linux__)
  auto sock = std::make_shared<platform::LinSocket>(io_core);
#else
  return std::unexpected(ErrorCode::PlatformNotSupported);
#endif

  sock->io_core_ = io_core;
  return sock;
}
