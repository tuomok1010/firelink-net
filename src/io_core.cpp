#include "firelink/io_core.hpp"

// Platform-specific implementation headers
#ifdef _WIN32
    #include <firelink/platform/windows/win_io_core.hpp>
#elif defined(__linux__)
    #include <firelink/platform/linux/lin_io_core.hpp>
#else
    #error "Firelink: Unsupported platform"
#endif

std::expected<std::unique_ptr<firelink::IOCore>, firelink::ErrorCode>
firelink::IOCore::create(const IOCoreConfig& config)
{
#if defined(_WIN32)
  auto core = std::make_unique<platform::WinIOCore>(config);
#elif defined(__linux__)
  auto core = std::make_unique<platform::LinuxIOCore>(config);
#else
  return std::unexpected(ErrorCode::PlatformNotSupported);
#endif

  // Try to initialize
  ErrorCode ec = core->initialize();
  if (ec != ErrorCode::Success)
  {
    return std::unexpected(ec);
  }

  return core;
}
