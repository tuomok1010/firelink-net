#include "echo_server.hpp"
#include <iostream>

EchoServer::EchoServer(firelink::IOCoreConfig config, int max_clients)
    : config_(config), max_clients_(max_clients), listener_({}), clients_({})
{
}

EchoServer::~EchoServer()
{
}

int EchoServer::init()
{
  if (clients_.size() > 0)
    clients_.clear();
  else
    clients_.reserve(max_clients_);

  for (int i = 0; i < max_clients_; ++i)
    clients_.push_back(std::make_shared<ClientContext>());

  auto io_core_pending = firelink::IOCore::create(config_);
  if (!io_core_pending.has_value())
  {
    std::cerr << "firelink::IOCore::create error " << static_cast<int>(io_core_pending.error())
              << std::endl;
    return -1;
  }

  io_core_ = std::move(io_core_pending.value());

  firelink::ErrorCode err = io_core_->initialize();
  if (err != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::IOCore::initialize error " << static_cast<int>(err) << std::endl;
    return -1;
  }

  auto listener_pending = firelink::Socket::create(io_core_);
  if (!listener_pending.has_value())
  {
    std::cerr << "firelink::Socket::create error " << static_cast<int>(listener_pending.error())
              << std::endl;
    io_core_->release();
    return -1;
  }

  listener_ = std::move(listener_pending.value());
  return 0;
}

int EchoServer::release()
{
  io_core_->release();
}

int EchoServer::start(const firelink::Endpoint& endpoint, int backlog)
{
  if (listener_->socket(firelink::AddressFamily::IPv4, firelink::SocketType::Stream,
                        firelink::Protocol::Tcp) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::socket() error!" << std::endl;
    return -1;
  }

  if (listener_->bind(endpoint) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::bind() error " << std::endl;
    listener_->close();
    return -1;
  }

  if (listener_->listen(backlog) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::listen() error!" << std::endl;
    listener_->close();
    return -1;
  }

  std::cout << "listening for connections to  "
            << firelink::inet_ntop(firelink::AddressFamily::IPv4, endpoint) << std::endl;

  if (initialize_clients() != 0)
  {
    std::cerr << "initialize_client_sockets() error!" << std::endl;
    listener_->close();
    return -1;
  }

  io_core_->run();
  return 0;
}

int EchoServer::stop()
{
  if (close_all_clients() != 0)
  {
    std::cerr << "async_echo_server close_client_sockets() error!" << std::endl;
    listener_->close();
    return -1;
  }

  if (listener_->close() != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::close() error!" << std::endl;
    return -1;
  }

  io_core_->stop();
  std::cout << "server stopped." << std::endl;
  return 0;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
void EchoServer::on_accept_complete(std::shared_ptr<firelink::Socket> listener,
                                    std::shared_ptr<firelink::Socket> accepted_socket,
                                    const firelink::Endpoint& local_endpoint,
                                    const firelink::Endpoint& peer_endpoint,
                                    std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
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
      listener->stop_io_context();
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
      listener->stop_io_context();
    }
    return;
  }
}

void EchoServer::on_recv_complete(std::shared_ptr<firelink::Socket> socket,
                                  std::span<std::byte> buffer, std::shared_ptr<void> user_op_data,
                                  firelink::ErrorCode error, std::int32_t bytes_transferred,
                                  firelink::ReadTag tag)
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
      socket->stop_io_context();
    }
    return;
  }

  if (bytes_transferred < 0)
  {
    std::cerr << "firelink::Socket::recv() error!" << std::endl;
    if (reset_client(std::static_pointer_cast<ClientContext>(user_op_data)) != 0)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
      socket->stop_io_context();
    }
    return;
  }
  else if (bytes_transferred == 0)
  {
    std::cout << "client disconnected." << std::endl;
    if (reset_client(std::static_pointer_cast<ClientContext>(user_op_data)) != 0)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
      socket->stop_io_context();
    }
    return;
  }

  std::cout << "received " << bytes_transferred << " bytes" << std::endl;

  auto context = std::static_pointer_cast<ClientContext>(user_op_data);
  auto write_span =
    std::span<std::byte>(context->write_buffer_).first(static_cast<std::size_t>(bytes_transferred));

  std::memcpy(write_span.data(), buffer.data(), static_cast<uint32_t>(bytes_transferred));

  if (socket->start_send(write_span, user_op_data, on_send_complete) !=
      firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_send() error!" << std::endl;
    if (reset_client(std::static_pointer_cast<ClientContext>(user_op_data)) != 0)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
      socket->stop_io_context();
    }
    return;
  }

  context->read_buffer_.fill(std::byte{0});
  if (socket->start_recv(std::span<std::byte>(context->read_buffer_), user_op_data,
                         on_recv_complete) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_recv() error!" << std::endl;
    if (reset_client(std::static_pointer_cast<ClientContext>(user_op_data)) != 0)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
      socket->stop_io_context();
    }
    return;
  }
}

void EchoServer::on_send_complete(std::shared_ptr<firelink::Socket> socket,
                                  std::span<std::byte> buffer, std::shared_ptr<void> user_op_data,
                                  firelink::ErrorCode error, std::int32_t bytes_transferred,
                                  firelink::WriteTag tag)
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
      socket->stop_io_context();
    }
    return;
  }

  std::cout << "sent " << bytes_transferred << " bytes" << std::endl;
}
#pragma clang diagnostic pop

int EchoServer::initialize_clients()
{
  for (unsigned int i = 0; i < max_clients_; ++i)
  {
    auto accept_sock_pending = firelink::Socket::create(io_core_);
    if (!accept_sock_pending.has_value())
    {
      std::cerr << "firelink::Socket::create error "
                << static_cast<int>(accept_sock_pending.error()) << std::endl;
      close_all_clients();
      return -1;
    }

    clients_[i]->socket_ = std::move(accept_sock_pending.value());
    clients_[i]->listener_socket_ = listener_;

    if (reset_client(clients_[i]) != 0)
    {
      std::cerr << "reset_client_socket() error!" << std::endl;
      close_all_clients();
      return -1;
    }
  }

  return 0;
}

int EchoServer::close_all_clients()
{
  int ret = 0;
  for (unsigned int i = 0; i < max_clients_; ++i)
  {
    if (clients_[i]->socket_->close() != firelink::ErrorCode::Success)
    {
      std::cerr << "firelink::Socket::close() error!" << std::endl;
      ret = -1;
    }
  }

  return ret;
}

int EchoServer::reset_client(std::shared_ptr<ClientContext> context)
{
  // Close the socket in case it has been initialized already
  if (context->socket_->close() != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::close() error!" << std::endl;
    return -1;
  }

  // Reopen socket
  if (context->socket_->socket(firelink::AddressFamily::IPv4, firelink::SocketType::Stream,
                               firelink::Protocol::Tcp) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::socket() error!" << std::endl;
    return -2;
  }

  if (std::shared_ptr<firelink::Socket> listener_ptr = context->listener_socket_.lock())
  {
    // Ready to accept a new connection in the socket
    if (listener_ptr->start_accept(context->socket_, std::static_pointer_cast<void>(context),
                                   on_accept_complete) != firelink::ErrorCode::Success)
    {
      std::cerr << "firelink::Socket::start_accept() error!" << std::endl;
      return -3;
    }
    return 0;
  }

  return -4;
}
