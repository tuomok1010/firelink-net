#include "firelink/endpoint.hpp"
#include <in6addr.h>
#include <ws2ipdef.h>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#elif defined(__linux__)
#include <arpa/inet.h>
#endif

std::string firelink::inet_ntop(const IPv4Address& addr)
{
  // Convert into a string
  char buf[INET_ADDRSTRLEN]{};

  if (::inet_ntop(AF_INET, addr.bytes.data(), buf, INET_ADDRSTRLEN) == nullptr)
    return {};

  return std::string(buf);
}

std::string firelink::inet_ntop(const IPv6Address& addr)
{
  char buf[INET6_ADDRSTRLEN]{};
  if (::inet_ntop(AF_INET6, addr.bytes.data(), buf, INET6_ADDRSTRLEN) == nullptr)
    return {};

  return std::string(buf);
}

std::string firelink::inet_ntop(const Endpoint& endpoint)
{
  if (endpoint.family() == AddressFamily::IPv4)
    return firelink::inet_ntop(endpoint.ipv4()) + ":" + std::to_string(endpoint.port());
  else if (endpoint.family() == AddressFamily::IPv6)
    return "[" + firelink::inet_ntop(endpoint.ipv6()) + "]" + ":" + std::to_string(endpoint.port());
  else
    return {};
}

firelink::ErrorCode firelink::inet_pton(std::string_view str, IPv4Address& out)
{
  std::string trimmed_addr{};
  // std::string trimmed_port{};

  size_t pos = str.rfind(':');
  if (pos != std::string_view::npos && pos > str.find_last_of('.'))
  {
    // trim addr and port from str
    trimmed_addr = std::string(str.substr(0, pos));
    // trimmed_port = std::string(str.substr(pos + 1));

    // In case we needed the port
    // std::uint16_t port = static_cast<std::uint16_t>(std::stoi(trimmed_port, &pos));
    // TODO check stoi errors?
  }
  else
  {
    trimmed_addr = std::string(str);
  }

  int res = ::inet_pton(AF_INET, trimmed_addr.data(), out.bytes.data());
  if (res != 1)
  {
    return ErrorCode::SystemError;
  }

  return ErrorCode::Success;
}

firelink::ErrorCode firelink::inet_pton(std::string_view str, IPv6Address& out)
{
  std::string trimmed_addr{};
  // std::string trimmed_port{};

  size_t pos = str.rfind(':');
  if (pos != std::string_view::npos && pos > str.rfind(']'))
  {
    // trim addr and port from str
    trimmed_addr = std::string(str.substr(0, pos));
    // trimmed_port = std::string(str.substr(pos + 1));

    // trim brackets from around addr ("[addr]")
    trimmed_addr.erase(0, 1);
    trimmed_addr.pop_back();

    // In case we needed the port
    // std::uint16_t port = static_cast<std::uint16_t>(std::stoi(trimmed_port, &pos));
    // TODO check stoi errors?
  }
  else
  {
    trimmed_addr = std::string(str);
  }

  sockaddr_in6 addr{};
  int res = ::inet_pton(AF_INET6, trimmed_addr.data(), &addr.sin6_addr);
  if (res != 1)
  {
    return ErrorCode::SystemError;
  }

  std::memcpy(out.bytes.data(), &addr.sin6_addr, 16);
  return ErrorCode::Success;
}

firelink::ErrorCode firelink::inet_pton(AddressFamily family, std::string_view str, Endpoint& out)
{
  std::string trimmed_port{};
  std::string trimmed_addr{};
  std::uint16_t port{};

  if (family == AddressFamily::IPv4)
  {
    size_t pos = str.rfind(':');
    if (pos != std::string_view::npos && pos > str.find_last_of('.'))
    {
      trimmed_addr = std::string(str.substr(0, pos));
      trimmed_port = std::string(str.substr(pos + 1));

      port = static_cast<std::uint16_t>(std::stoi(trimmed_port, &pos));
      // TODO check stoi errors?
    }
    else
    {
      trimmed_addr = std::string(str);
    }

    firelink::IPv4Address addr{};
    int res = ::inet_pton(AF_INET, trimmed_addr.data(), addr.bytes.data());
    if (res != 1)
    {
      return ErrorCode::SystemError;
    }

    out = Endpoint(addr, port);
    return ErrorCode::Success;
  }

  else if (family == AddressFamily::IPv6)
  {
    size_t pos = str.rfind(':');
    if (pos != std::string_view::npos && pos > str.rfind(']'))
    {
      // trim port from string
      trimmed_addr = std::string(str.substr(0, pos));
      trimmed_port = std::string(str.substr(pos + 1));

      // trim brackets from around addr ("[addr]")
      trimmed_addr.erase(0, 1);
      trimmed_addr.pop_back();

      port = static_cast<std::uint16_t>(std::stoi(trimmed_port, &pos));
      // TODO check stoi errors?
    }
    else
    {
      trimmed_addr = std::string(str);
    }

    firelink::IPv6Address addr{};
    int res = ::inet_pton(AF_INET6, trimmed_addr.data(), addr.bytes.data());
    if (res != 1)
    {
      return ErrorCode::SystemError;
    }

    out = Endpoint(addr, port);
    return ErrorCode::Success;
  }

  return ErrorCode::AddressFamilyNotSupported;
}

// NOTE: Only supports IPv6 and IPv4 checks! Crude! TODO: make better
firelink::AddressFamily firelink::str_to_family(std::string_view str)
{
  // Check if addr contains >1 ":" symbols --> IPv6, else --> IPv4
  int count = std::count(str.begin(), str.end(), ':');
  if (count > 1)
    return AddressFamily::IPv6;
  else
    return AddressFamily::IPv4;

  // return AddressFamily::NotSupported;
}
