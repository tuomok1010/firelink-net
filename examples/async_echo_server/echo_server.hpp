#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

#include "firelink/io_core.hpp"
#include "firelink/socket.hpp"

static constexpr int MAX_CLIENTS = 10000;
static constexpr int READ_BUFFER_SIZE = 512;
static constexpr int WRITE_BUFFER_SIZE = 512;

struct ClientContext
{
  std::shared_ptr<firelink::Socket> socket_;
  std::array<std::byte, READ_BUFFER_SIZE> read_buffer_;
  std::array<std::byte, WRITE_BUFFER_SIZE> write_buffer_;
};

class EchoServer
{
  public:
  explicit EchoServer(std::shared_ptr<firelink::IOCore> io_core);
  ~EchoServer();

  // Non-copyable / non-movable
  EchoServer(const EchoServer&) = delete;
  EchoServer& operator=(const EchoServer&) = delete;

  int start(const firelink::Endpoint& endpoint, int backlog = 5);
  void stop();

  private:
  // Handlers
  void on_accept_complete(std::shared_ptr<firelink::Socket> listener,
                        std::shared_ptr<firelink::Socket> accepted, const firelink::Endpoint& local,
                        const firelink::Endpoint& peer, std::shared_ptr<void> user_data,
                        firelink::ErrorCode error, firelink::AcceptTag tag);

  void on_recv_complete(std::shared_ptr<firelink::Socket> socket, std::span<std::byte> buffer,
                      std::shared_ptr<void> user_data, firelink::ErrorCode error,
                      std::int32_t bytes_transferred, firelink::ReadTag tag);

  void on_send_complete(std::shared_ptr<firelink::Socket> socket, std::span<std::byte> buffer,
                      std::shared_ptr<void> user_data, firelink::ErrorCode error,
                      std::int32_t bytes_transferred, firelink::WriteTag tag);

  // Helpers
  int initialize_client_sockets();
  int reset_client(std::shared_ptr<firelink::Socket> client_socket);
  int close_all_clients();

  private:
  std::shared_ptr<firelink::IOCore> io_core_;
  std::shared_ptr<firelink::Socket> listener_;
  std::array<std::shared_ptr<ClientContext>, MAX_CLIENTS> clients_;
};

#endif /* ECHO_SERVER_H */
