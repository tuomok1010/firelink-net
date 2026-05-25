#ifndef ASYNC_TCP_SERVER_H
#define ASYNC_TCP_SERVER_H

#include "firelink/io_core.hpp"
#include "firelink/socket.hpp"
#include <cstddef>
#include <cstdint>

class AsyncTCPServer : public std::enable_shared_from_this<AsyncTCPServer>
{
  public:
  explicit AsyncTCPServer();
  ~AsyncTCPServer();

  // Non-copyable / non-movable
  AsyncTCPServer(const AsyncTCPServer&) = delete;
  AsyncTCPServer& operator=(const AsyncTCPServer&) = delete;

  // Initialization/Release
  ErrorCode init(std::shared_ptr<firelink::IOCore> io_core, std::size_t max_clients);
  ErrorCode close();
  void stop_io_context();

  // Handlers
  using RecvHandler = std::function<void(
    std::shared_ptr<AsyncTCPServer> server, std::shared_ptr<AsyncTCPClient> client,
    std::span<std::byte> buffer, std::int32_t bytes_transferred, ErrorCode error,
    std::shared_ptr<void> user_op_data)>;

  using SendHandler = std::function<void(
    std::shared_ptr<AsyncTCPServer> server, std::shared_ptr<AsyncTCPClient> client,
    std::span<std::byte> buffer, std::int32_t bytes_transferred, ErrorCode error,
    std::shared_ptr<void> user_op_data)>;

  // Operations
  ErrorCode listen(const firelink::Endpoint& endpoint, int backlog = 5,
                   std::shared_ptr<void> user_op_data = nullptr, AcceptHandler handler = {});
  
  ErrorCode recv(std::span<std::byte> buffer, std::shared_ptr<void> user_op_data = nullptr,
                 RecvHandler handler = {});

  ErrorCode send(std::span<std::byte> data, std::shared_ptr<void> user_op_data = nullptr,
                 SendHandler handler = SendHandler{});

  // Utils
  firelink::AddressFamily get_addr_family() const;
  Endpoint get_local_endpoint() const;
  Endpoint get_remote_endpoint() const;

  private:
  struct OpContext
  {
    std::weak_ptr<AsyncTCPServer> server_;
    std::weak_ptr<AsyncTCPClient> client_;
    std::shared_ptr<void> user_op_data_;
  };

  std::shared_ptr<firelink::Socket> listener_;
  std::vector<std::shared_ptr<AsyncTCPClient>> clients_; 
};

#endif /* ASYNC_TCP_SERVER_H */
