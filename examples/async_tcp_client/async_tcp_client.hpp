#ifndef ASYNC_TCP_CLIENT_H
#define ASYNC_TCP_CLIENT_H

#include "firelink/io_core.hpp"
#include "firelink/socket.hpp"
#include <memory>

static constexpr int READ_BUFFER_LEN = 512;
static constexpr int WRITE_BUFFER_LEN = 512;

struct SocketBuffer
{
  std::array<std::byte, READ_BUFFER_LEN> read_buffer_;
  std::array<std::byte, WRITE_BUFFER_LEN> write_buffer_;
};

class AsyncTCPClient
{
  public:
  explicit AsyncTCPClient();
  ~AsyncTCPClient();

  // Non-copyable / non-movable
  AsyncTCPClient(const AsyncTCPClient&) = delete;
  AsyncTCPClient& operator=(const AsyncTCPClient&) = delete;

  firelink::ErrorCode run(std::shared_ptr<firelink::IOCore> io_core, firelink::Endpoint target_ep);
  firelink::ErrorCode close();

  private:
  // Callbacks
  static void on_connect_complete(std::shared_ptr<firelink::Socket> caller,
                           std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                           firelink::ConnectTag tag);

  static void on_disconnect_complete(std::shared_ptr<firelink::Socket> caller,
                              std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                              firelink::DisconnectTag tag);

  static void on_recv_complete(std::shared_ptr<firelink::Socket> caller, std::span<std::byte> user_buffer,
                        std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                        std::int32_t bytes_transferred, firelink::ReadTag tag);

  static void on_send_complete(std::shared_ptr<firelink::Socket> caller, std::span<std::byte> user_buffer,
                        std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                        std::int32_t bytes_transferred, firelink::WriteTag tag);

  std::shared_ptr<firelink::Socket> socket_;
};
#endif /* ASYNC_TCP_CLIENT_H */
