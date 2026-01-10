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
    std::uint16_t port = 0;

    constexpr IPv4Address() = default;

    constexpr IPv4Address(std::array<std::uint8_t, 4> b, std::uint16_t p) : bytes(b), port(p)
    {
      
    }

    static constexpr IPv4Address any(std::uint16_t p = 0) noexcept
    {
      return IPv4Address{{0, 0, 0, 0}, p};
    }

    static constexpr IPv4Address loopback(std::uint16_t p = 0) noexcept
    {
      return IPv4Address{{127, 0, 0, 1}, p};
    }
  };

  struct FIRELINK_CLASS_API IPv6Address
  {
    std::array<std::uint8_t, 16> bytes{};
    std::uint16_t port = 0;

    constexpr IPv6Address() = default;

    constexpr IPv6Address(std::array<std::uint8_t, 16> b, std::uint16_t p)
      : bytes(b), port(p)
    {
      
    }

    static constexpr IPv6Address any(std::uint16_t p = 0) noexcept
    {
      return IPv6Address{{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, p};
    }

    static constexpr IPv6Address loopback(std::uint16_t p = 0) noexcept
    {
      return IPv6Address{{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, p};
    }
  };

  class FIRELINK_CLASS_API Endpoint
  {
    public:
    Endpoint(){}
    Endpoint(IPv4Address addr) : ipv4_(addr) {}
    Endpoint(IPv6Address addr) : ipv6_(addr) {}

    inline const IPv4Address& ipv4()    const { return ipv4_; }
    inline const IPv6Address& ipv6()    const { return ipv6_; }

    private:
    union
    {
      IPv4Address ipv4_;
      IPv6Address ipv6_;
    };
  };

  // Utility functions
  FIRELINK_API std::string inet_ntop(const IPv4Address& addr);
  FIRELINK_API std::string inet_ntop(const IPv6Address& addr);
  FIRELINK_API std::string inet_ntop(AddressFamily family, const Endpoint& endpoint);

  FIRELINK_API ErrorCode inet_pton(std::string_view str, IPv4Address& out);
  FIRELINK_API ErrorCode inet_pton(std::string_view str, IPv6Address& out);
  FIRELINK_API ErrorCode inet_pton(AddressFamily family, std::string_view str, Endpoint& out);
} // namespace firelink

#endif /* ENDPOINT_H */
