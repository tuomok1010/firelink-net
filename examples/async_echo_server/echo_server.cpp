#include "echo_server.hpp"

EchoServer::EchoServer(std::shared_ptr<firelink::IOCore> io_core)
    : io_core_(io_core), listener_({}), clients_({})
{
}

EchoServer::~EchoServer()
{
}

int EchoServer::start(const firelink::Endpoint& endpoint, int backlog = 5)
{
  auto listener_pending = firelink::Socket::create(io_core_);
  if (!listener_pending.has_value())
  {
    return -1;
  }

  listener_ = std::move(listener_pending.value());

  if (listener_->socket(firelink::AddressFamily::IPv4, firelink::SocketType::Stream,
                        firelink::Protocol::Tcp) != firelink::ErrorCode::Success)
  {
    return -1;
  }

  if (listener_->bind(endpoint) != firelink::ErrorCode::Success)
  {
    listener_->close();
    return -1;
  }

  if (listener_->listen(backlog) != firelink::ErrorCode::Success)
  {
    listener_->close();
    return -1;
  }

  return initialize_client_sockets();
}

void EchoServer::stop()
{
  close_all_clients();
  listener_->close();
}

void EchoServer::on_accept_complete(std::shared_ptr<firelink::Socket> listener,
                                    std::shared_ptr<firelink::Socket> accepted,
                                    const firelink::Endpoint& local, const firelink::Endpoint& peer,
                                    std::shared_ptr<void> user_data, firelink::ErrorCode error,
                                    firelink::AcceptTag tag)
{
  /*
   * Close erronous socket, attempt to make it available for accept again
   */
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "socket error " << std::to_string(static_cast<int>(error)) << std::endl;
    if (reset_client(std::static_pointer_cast<ClientContext>(user_op_data)) != 0)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
    }
    return;
  }

  auto accepted = std::shared_ptr<firelink::Socket>(std::move(accepted_socket));
  auto context = std::static_pointer_cast<ClientContext>(user_op_data);

  std::cout << firelink::inet_ntop(listener->get_addr_family(), local_endpoint)
            << " accepted connection from "
            << firelink::inet_ntop(accepted->get_addr_family(), peer_endpoint) << std::endl;

  if (accepted->start_recv(std::span<std::byte>(context->read_buffer_), user_op_data,
                           on_recv_complete) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_recv() error!" << std::endl;
    if (reset_client(std::static_pointer_cast<ClientContext>(user_op_data)) != 0)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
    }
    return;
  }
}

void EchoServer::on_recv_complete(std::shared_ptr<firelink::Socket> socket,
                                  std::span<std::byte> buffer, std::shared_ptr<void> user_data,
                                  firelink::ErrorCode error, std::int32_t bytes_transferred,
                                  firelink::ReadTag tag)
{
}

void EchoServer::on_send_complete(std::shared_ptr<firelink::Socket> socket,
                                  std::span<std::byte> buffer, std::shared_ptr<void> user_data,
                                  firelink::ErrorCode error, std::int32_t bytes_transferred,
                                  firelink::WriteTag tag)
{
}

int EchoServer::initialize_client_sockets()
{
  for (unsigned int i = 0; i < MAX_N_CLIENTS; ++i)
  {
    auto accept_sock_pending = firelink::Socket::create(io_core_);
    if (!accept_sock_pending.has_value())
    {
      std::cerr << "firelink::Socket::create error "
                << static_cast<int>(accept_sock_pending.error()) << std::endl;
      close_client_sockets(clients_);
      return -1;
    }

    clients_[i]->socket_ = std::move(accept_sock_pending.value());
    if (reset_client(clients[i]) != 0)
    {
      std::cerr << "reset_client_socket() error!" << std::endl;
      close_client_sockets(clients);
      return -1;
    }
  }

  return 0;
}

int EchoServer::reset_client(std::shared_ptr<ClientContext> client)
{
  // Close the socket in case it has been initialized already
  if (client->socket_->close() != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::close() error!" << std::endl;
    return -1;
  }

  // Reopen socket
  if (client->socket_->socket(firelink::AddressFamily::IPv4, firelink::SocketType::Stream,
                              firelink::Protocol::Tcp) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::socket() error!" << std::endl;
    return -2;
  }

  // Ready to accept a new connection in the socket
  if (listener_->start_accept(client->socket_, std::static_pointer_cast<void>(client),
                              on_accept_complete) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_accept() error!" << std::endl return -3;
  }

  return 0;
}

int EchoServer::close_all_clients()
{
  int ret = 0;
  for (unsigned int i = 0; i < MAX_N_CLIENTS; ++i)
  {
    if (clients[i]->socket_->close() != firelink::ErrorCode::Success)
    {
      std::cerr << "firelink::Socket::close() error!" << std::endl;
      ret = -1;
    }
  }

  return ret;
}
