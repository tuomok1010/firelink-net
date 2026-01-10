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
  
  if(::inet_ntop(AF_INET, addr.bytes.data(), buf, INET_ADDRSTRLEN) == nullptr)
    return {};
  
  return std::string(buf) + ":" + std::to_string(addr.port);
}
  
std::string firelink::inet_ntop(const IPv6Address& addr)
{ 
  char buf[INET6_ADDRSTRLEN]{};
  if (::inet_ntop(AF_INET6, addr.bytes.data(), buf, INET6_ADDRSTRLEN) == nullptr)
    return {};
    
  return "[" + std::string(buf) + "]" + ":" + std::to_string(addr.port);
}
  
std::string firelink::inet_ntop(AddressFamily family, const Endpoint& endpoint)
{
  if(family == AddressFamily::IPv4)
    return firelink::inet_ntop(endpoint.ipv4());
  else if(family == AddressFamily::IPv6)
    return firelink::inet_ntop(endpoint.ipv6());
  else
    return {};
}

firelink::ErrorCode firelink::inet_pton(std::string_view str, IPv4Address& out)
{
  std::string trimmed_addr{};
  std::string trimmed_port{};
  
  size_t pos = str.rfind(':');
  if (pos != std::string_view::npos && pos > str.find_last_of('.'))
  {    
    trimmed_addr = std::string(str.substr(0, pos));
    trimmed_port = std::string(str.substr(pos + 1));
    
    out.port = static_cast<std::uint16_t>(std::stoi(trimmed_port, &pos));
    // TODO check stoi errors?
  }
  else
  { 
    trimmed_addr = std::string(str);
  }

  int res = ::inet_pton(AF_INET, trimmed_addr.data(), out.bytes.data());
  if(res != 1)
  {
    return ErrorCode::SystemError;
  }

  return ErrorCode::Success;
}
  
firelink::ErrorCode firelink::inet_pton(std::string_view str, IPv6Address& out)
{
  std::string trimmed_addr{};
  std::string trimmed_port{};
  
  size_t pos = str.rfind(':');
  if (pos != std::string_view::npos && pos > str.rfind(']'))
  {    
    // trim port from string
    trimmed_addr = std::string(str.substr(0, pos));
    trimmed_port = std::string(str.substr(pos + 1));

    // trim brackets from around addr ("[addr]")
    trimmed_addr.erase(0, 1);
    trimmed_addr.pop_back();
    
    out.port = static_cast<std::uint16_t>(std::stoi(trimmed_port, &pos));
    // TODO check stoi errors?
  }
  else
  { 
    trimmed_addr = std::string(str);
  }
  
  sockaddr_in6 addr{};
  int res = ::inet_pton(AF_INET6, trimmed_addr.data(), &addr.sin6_addr);
  if(res != 1)
  {
    return ErrorCode::SystemError;
  }

  std::memcpy(out.bytes.data(), &addr.sin6_addr, 16);
  return ErrorCode::Success;    
}
  
firelink::ErrorCode firelink::inet_pton(AddressFamily family, std::string_view str, Endpoint& out)
{
  ErrorCode result = ErrorCode::Success;
  if(family == AddressFamily::IPv4)
  {
    firelink::IPv4Address addr{};
    result = firelink::inet_pton(str, addr);
    if(result == ErrorCode::Success)
      out = Endpoint(addr);

    return result;
  }
    
  else if(family == AddressFamily::IPv6)
  {
    firelink::IPv6Address addr{};
    result = firelink::inet_pton(str, addr);
    if(result == ErrorCode::Success)
      out = Endpoint(addr);

    return result;
  }

  return ErrorCode::AddressFamilyNotSupported;
}
