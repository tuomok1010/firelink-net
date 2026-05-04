#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

#include "firelink/io_core.hpp"
#include "firelink/socket.hpp"
#include <memory>

static constexpr int READ_BUFFER_SIZE = 512;
static constexpr int WRITE_BUFFER_SIZE = 512;

struct ClientContext
{
  std::weak_ptr<firelink::Socket> listener_socket_;
  std::shared_ptr<firelink::Socket> socket_;
  std::array<std::byte, READ_BUFFER_SIZE> read_buffer_;
  std::array<std::byte, WRITE_BUFFER_SIZE> write_buffer_;
};

class EchoServer
{
  public:
  explicit EchoServer(firelink::IOCoreConfig config, int max_clients);
  ~EchoServer();

  // Non-copyable / non-movable
  EchoServer(const EchoServer&) = delete;
  EchoServer& operator=(const EchoServer&) = delete;

  int init();
  int release(); // TODO implement
  int start(const firelink::Endpoint& endpoint, int backlog = 5);
  int stop();

  private:
  // Handlers
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
  int initialize_clients();
  int close_all_clients();
  static int reset_client(std::shared_ptr<ClientContext> context);

  private:
  firelink::IOCoreConfig config_;
  int max_clients_;
  
  std::shared_ptr<firelink::IOCore> io_core_;
  std::shared_ptr<firelink::Socket> listener_;
  std::vector<std::shared_ptr<ClientContext>> clients_;
};

#endif /* ECHO_SERVER_H */
