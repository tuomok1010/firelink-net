#ifndef ASYNC_TCP_SERVER_H
#define ASYNC_TCP_SERVER_H

#include "firelink/io_core.hpp"
#include "firelink/socket.hpp"
#include <memory>

static constexpr int READ_BUFFER_LEN = 512;
static constexpr int WRITE_BUFFER_LEN = 512;

struct ClientContext
{
  std::weak_ptr<firelink::Socket> listener_socket_;
  std::shared_ptr<firelink::Socket> socket_;
  std::array<std::byte, READ_BUFFER_LEN> read_buffer_;
  std::array<std::byte, WRITE_BUFFER_LEN> write_buffer_;
};

class AsyncTCPServer
{
  public:
  explicit AsyncTCPServer(int max_clients);
  ~AsyncTCPServer();

  // Non-copyable / non-movable
  AsyncTCPServer(const AsyncTCPServer&) = delete;
  AsyncTCPServer& operator=(const AsyncTCPServer&) = delete;

  firelink::ErrorCode run(std::shared_ptr<firelink::IOCore> io_core,
                          firelink::Endpoint listener_ep, int backlog);
  firelink::ErrorCode close();

  private:
  // Callbacks
  static void on_accept_complete(std::shared_ptr<firelink::Socket> listener,
                                 std::shared_ptr<firelink::Socket> accepted_socket,
                                 const firelink::Endpoint& local_endpoint,
                                 const firelink::Endpoint& peer_endpoint,
                                 std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                                 firelink::AcceptTag tag);

  static void on_recv_complete(std::shared_ptr<firelink::Socket> socket,
                               std::span<std::byte> buffer, std::shared_ptr<void> user_op_data,
                               firelink::ErrorCode error, std::int32_t bytes_transferred,
                               firelink::ReadTag tag);

  static void on_send_complete(std::shared_ptr<firelink::Socket> socket,
                               std::span<std::byte> buffer, std::shared_ptr<void> user_op_data,
                               firelink::ErrorCode error, std::int32_t bytes_transferred,
                               firelink::WriteTag tag);

  // Helpers
  firelink::ErrorCode initialize_clients(std::shared_ptr<firelink::IOCore> io_core);
  firelink::ErrorCode close_all_clients();
  static firelink::ErrorCode reset_client(std::shared_ptr<ClientContext> context);

  private:
  int max_clients_;
  std::shared_ptr<firelink::Socket> listener_;
  std::vector<std::shared_ptr<ClientContext>> clients_;
};

#endif /* ASYNC_TCP_SERVER_H */
