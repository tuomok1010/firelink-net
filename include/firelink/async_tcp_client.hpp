#ifndef ASYNC_TCP_CLIENT_H
#define ASYNC_TCP_CLIENT_H

#include "firelink/io_core.hpp"
#include "firelink/socket.hpp"
#include <cstddef>
#include <memory>

namespace firelink
{
class FIRELINK_CLASS_API AsyncTCPClient : public std::enable_shared_from_this<AsyncTCPClient>
{
  public:
  explicit AsyncTCPClient();
  ~AsyncTCPClient();

  // Non-copyable / non-movable
  AsyncTCPClient(const AsyncTCPClient&) = delete;
  AsyncTCPClient& operator=(const AsyncTCPClient&) = delete;

  // Initialization/Release
  ErrorCode init(std::shared_ptr<firelink::IOCore> io_core);
  ErrorCode close();

  // Handlers
  using ConnectHandler = std::function<void(std::shared_ptr<AsyncTCPClient> client, ErrorCode error,
                                            std::shared_ptr<void> user_op_data)>;

  using DisconnectHandler = std::function<void(
    std::shared_ptr<AsyncTCPClient> client, ErrorCode error, std::shared_ptr<void> user_op_data)>;

  using RecvHandler = std::function<void(
    std::shared_ptr<AsyncTCPClient> client, std::span<std::byte> buffer,
    std::int32_t bytes_transferred, ErrorCode error, std::shared_ptr<void> user_op_data)>;

  using SendHandler = std::function<void(
    std::shared_ptr<AsyncTCPClient> client, std::span<std::byte> buffer,
    std::int32_t bytes_transferred, ErrorCode error, std::shared_ptr<void> user_op_data)>;

  // Operations
  ErrorCode connect(const firelink::Endpoint& dst, std::shared_ptr<void> user_op_data = nullptr,
              ConnectHandler handler = {});

  ErrorCode disconnect(std::shared_ptr<void> user_op_data = nullptr, DisconnectHandler handler = {});

  ErrorCode recv(std::span<std::byte> buffer, std::shared_ptr<void> user_op_data = nullptr,
           RecvHandler handler = {});

  ErrorCode send(std::span<std::byte> data, std::shared_ptr<void> user_op_data = nullptr,
                 SendHandler handler = SendHandler{});

  // Utils
  bool is_connected() const noexcept;
  Endpoint get_local_endpoint() const;
  Endpoint get_remote_endpoint() const;

  private:
  struct OpContext
  {
    std::weak_ptr<AsyncTCPClient> client_;
    std::shared_ptr<void> user_op_data_;
  };

  std::shared_ptr<firelink::Socket> socket_;
  bool connected_;
};
} // namespace firelink
#endif /* ASYNC_TCP_CLIENT_H */
