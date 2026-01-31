#ifndef FIRELINK_SOCKET_H
#define FIRELINK_SOCKET_H

#include "firelink/export.hpp"
#include "firelink/types.hpp"
#include "firelink/error_codes.hpp"
#include "firelink/options.hpp"
#include "firelink/endpoint.hpp"
#include "firelink/io_core.hpp"


#include <memory>
#include <string_view>
#include <span>
#include <functional>
#include <string>

namespace firelink
{
  class Socket;

  struct AcceptTag {};
  struct ConnectTag {};
  struct ReadTag {};
  struct WriteTag {};
  struct DisconnectTag {};
  
  using AcceptHandler = std::function<void(std::shared_ptr<firelink::Socket> caller,
                                           std::shared_ptr<firelink::Socket> accepted_socket,
                                           const Endpoint& local_endpoint,
                                           const Endpoint& peer_endpoint,
                                           ErrorCode error, AcceptTag tag)>;
  
  using ConnectHandler = std::function<void(std::shared_ptr<firelink::Socket> caller,
                                            ErrorCode error, ConnectTag tag)>;
  
  using ReadHandler = std::function<void(std::shared_ptr<firelink::Socket> caller,
                                         ErrorCode error, std::int32_t bytes_transferred, ReadTag tag)>;
  
  using WriteHandler = std::function<void(std::shared_ptr<firelink::Socket> caller,
                                          ErrorCode error, std::int32_t bytes_transferred, WriteTag tag)>;
  
  using DisconnectHandler = std::function<void(std::shared_ptr<firelink::Socket> caller,
                                               ErrorCode error, DisconnectTag tag)>;
  
  class FIRELINK_CLASS_API Socket
  {
    public:
    virtual ~Socket() = default;
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Initialization / Creation
    static std::expected<std::shared_ptr<Socket>, ErrorCode> create(std::shared_ptr<IOCore> io_core);

    // Synchronous API
    virtual ErrorCode socket(AddressFamily addr_family, SocketType sock_type, Protocol protocol) = 0;
    virtual ErrorCode bind(const Endpoint& endpoint) = 0;
    virtual ErrorCode listen(std::int32_t backlog) = 0;
    virtual ErrorCode shutdown(ShutdownHow how) = 0;
    virtual ErrorCode close() = 0;

    virtual ErrorCode set_socket_option(SocketOptionLevel level, SocketOption option,
                                        std::span<const std::byte> value) = 0;

    virtual ErrorCode get_socket_option(SocketOptionLevel level, SocketOption option,
                                        std::span<std::byte> value,
                                        std::size_t& value_size_out) = 0;

    virtual ErrorCode get_sock_name(Endpoint& ep) = 0;
    virtual ErrorCode get_peer_name(Endpoint& ep) = 0;

    virtual bool is_valid() const = 0;

    inline NativeHandle get_native_handle() const { return socket_; }
    inline AddressFamily get_addr_family() const { return addr_family_; }
    inline SocketType get_sock_type() const { return sock_type_; }
    inline Protocol get_protocol() const { return protocol_; }
    inline bool is_bound() const { return is_bound_; }
    
    virtual ErrorCode accept(std::shared_ptr<firelink::Socket> accept_socket) = 0;
    virtual ErrorCode connect(const Endpoint& dst) = 0;
    virtual std::int32_t recv(std::span<std::byte> buffer) = 0;
    virtual std::int32_t recv_from(std::span<std::byte> buffer, Endpoint& dst) = 0;
    virtual std::int32_t send(std::span<std::byte> data) = 0;
    virtual std::int32_t send_to(std::span<std::byte> data, const Endpoint& dst) = 0;
    virtual ErrorCode disconnect(int timeout_ms) = 0;

    // Asynchronous API
    virtual ErrorCode start_accept(std::shared_ptr<firelink::Socket> accept_socket, AcceptHandler handler = AcceptHandler{}) = 0;
    virtual ErrorCode start_connect(const Endpoint& dst, ConnectHandler handler = ConnectHandler{}) = 0;
    virtual ErrorCode start_recv(std::span<std::byte> buffer, ReadHandler handler = ReadHandler{}) = 0;
    virtual ErrorCode start_recv_from(std::span<std::byte> buffer, ReadHandler handler = ReadHandler{}) = 0;
    virtual ErrorCode start_send(std::span<std::byte> data, WriteHandler handler = WriteHandler{}) = 0;
    virtual ErrorCode start_send_to(std::span<std::byte> data, const Endpoint& dst, WriteHandler handler = WriteHandler{}) = 0;
    virtual ErrorCode start_disconnect(bool reuse_socket, DisconnectHandler handler = DisconnectHandler{}) = 0;

  protected:
    Socket(std::shared_ptr<IOCore> io_core);
    std::weak_ptr<IOCore> io_core_;
    NativeHandle socket_;
    AddressFamily addr_family_;
    SocketType sock_type_;
    Protocol protocol_;
    bool is_bound_;
  };
}
#endif /* FIRELINK_SOCKET_H */
