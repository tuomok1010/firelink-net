#ifndef ENDPOINT_H
#define ENDPOINT_H

#include "export.hpp"
#include "firelink/types.hpp"
#include "firelink/error_codes.hpp"

#include <array>
#include <string>

namespace firelink
{
struct FIRELINK_CLASS_API IPv4Address
{
  std::array<std::uint8_t, 4> bytes{};

  constexpr IPv4Address() = default;

  constexpr IPv4Address(std::array<std::uint8_t, 4> b) : bytes(b)
  {
  }

  static constexpr IPv4Address any() noexcept
  {
    return IPv4Address{{0, 0, 0, 0}};
  }

  static constexpr IPv4Address loopback() noexcept
  {
    return IPv4Address{{127, 0, 0, 1}};
  }
};

struct FIRELINK_CLASS_API IPv6Address
{
  std::array<std::uint8_t, 16> bytes{};

  constexpr IPv6Address() = default;

  constexpr IPv6Address(std::array<std::uint8_t, 16> b) : bytes(b)
  {
  }

  static constexpr IPv6Address any() noexcept
  {
    return IPv6Address{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
  }

  static constexpr IPv6Address loopback() noexcept
  {
    return IPv6Address{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
  }
};

class FIRELINK_CLASS_API Endpoint
{
  public:
  Endpoint()
  {
  }
  
  Endpoint(IPv4Address addr, std::uint16_t port) : ipv4_(addr), family_(AddressFamily::IPv4), port_(port)
  {
  }
  
  Endpoint(IPv6Address addr, std::uint16_t port) : ipv6_(addr), family_(AddressFamily::IPv6), port_(port)
  {
  }

  inline const IPv4Address& ipv4() const
  {
    return ipv4_;
  }
  
  inline const IPv6Address& ipv6() const
  {
    return ipv6_;
  }

  inline AddressFamily family() const
  {
    return family_;
  }

  inline std::uint16_t port() const
  {
    return port_;
  }

  private:
  union
  {
    IPv4Address ipv4_;
    IPv6Address ipv6_;
  };

  AddressFamily family_;
  std::uint16_t port_;
};

// Utility functions
FIRELINK_API std::string inet_ntop(const IPv4Address& addr);
FIRELINK_API std::string inet_ntop(const IPv6Address& addr);
FIRELINK_API std::string inet_ntop(const Endpoint& endpoint);

FIRELINK_API ErrorCode inet_pton(std::string_view str, IPv4Address& out);
FIRELINK_API ErrorCode inet_pton(std::string_view str, IPv6Address& out);
FIRELINK_API ErrorCode inet_pton(AddressFamily family, std::string_view str, Endpoint& out);
} // namespace firelink

#endif /* ENDPOINT_H */
