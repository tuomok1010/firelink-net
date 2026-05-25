#include "async_tcp_server.hpp"
#include <cstddef>
#include <iostream>

AsyncTCPServer::AsyncTCPServer(int max_clients) : max_clients_(max_clients), listener_({}), clients_({})
{
}

AsyncTCPServer::~AsyncTCPServer()
{
}

firelink::ErrorCode AsyncTCPServer::run(std::shared_ptr<firelink::IOCore> io_core,
                                    firelink::Endpoint listener_ep, int backlog)
{
  if (clients_.size() > 0)
    clients_.clear();
  else
    clients_.reserve(static_cast<std::size_t>(max_clients_));

  for (int i = 0; i < max_clients_; ++i)
    clients_.push_back(std::make_shared<ClientContext>());

  auto listener_pending = firelink::Socket::create(io_core);
  if (!listener_pending.has_value())
  {
    return listener_pending.error();
  }

  listener_ = std::move(listener_pending.value());
  firelink::ErrorCode error = listener_->socket(
    firelink::AddressFamily::IPv4, firelink::SocketType::Stream, firelink::Protocol::Tcp);
  if (error != firelink::ErrorCode::Success)
  {
    return error;
  }

  error = listener_->bind(listener_ep);
  if (error != firelink::ErrorCode::Success)
  {
    listener_->close();
    return error;
  }

  error = listener_->listen(backlog);
  if (error != firelink::ErrorCode::Success)
  {
    listener_->close();
    return error;
  }

  error = initialize_clients(io_core);
  if (error != firelink::ErrorCode::Success)
  {
    listener_->close();
    return error;
  }

  return firelink::ErrorCode::Success;
}

firelink::ErrorCode AsyncTCPServer::close()
{
  firelink::ErrorCode error = close_all_clients();
  if (error != firelink::ErrorCode::Success)
  {
    listener_->close();
    return error;
  }

  error = listener_->close();
  if (error != firelink::ErrorCode::Success)
  {
    return error;
  }

  return firelink::ErrorCode::Success;
}

void AsyncTCPServer::on_accept_complete(std::shared_ptr<firelink::Socket> listener,
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
    if (reset_client(std::static_pointer_cast<ClientContext>(user_op_data)) != firelink::ErrorCode::Success)
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
    if (reset_client(std::static_pointer_cast<ClientContext>(user_op_data)) != firelink::ErrorCode::Success)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
      listener->stop_io_context();
    }
    return;
  }
}

void AsyncTCPServer::on_recv_complete(std::shared_ptr<firelink::Socket> socket,
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
    if (reset_client(std::static_pointer_cast<ClientContext>(user_op_data)) != firelink::ErrorCode::Success)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
      socket->stop_io_context();
    }
    return;
  }

  if (bytes_transferred < 0)
  {
    std::cerr << "firelink::Socket::recv() error!" << std::endl;
    if (reset_client(std::static_pointer_cast<ClientContext>(user_op_data)) != firelink::ErrorCode::Success)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
      socket->stop_io_context();
    }
    return;
  }
  else if (bytes_transferred == 0)
  {
    std::cout << "client disconnected." << std::endl;
    if (reset_client(std::static_pointer_cast<ClientContext>(user_op_data)) != firelink::ErrorCode::Success)
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
    if (reset_client(std::static_pointer_cast<ClientContext>(user_op_data)) != firelink::ErrorCode::Success)
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
    if (reset_client(std::static_pointer_cast<ClientContext>(user_op_data)) != firelink::ErrorCode::Success)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
      socket->stop_io_context();
    }
    return;
  }
}

void AsyncTCPServer::on_send_complete(std::shared_ptr<firelink::Socket> socket,
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
    if (reset_client(std::static_pointer_cast<ClientContext>(user_op_data)) != firelink::ErrorCode::Success)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
      socket->stop_io_context();
    }
    return;
  }

  std::cout << "sent " << bytes_transferred << " bytes" << std::endl;
}

firelink::ErrorCode AsyncTCPServer::initialize_clients(std::shared_ptr<firelink::IOCore> io_core)
{
  for (unsigned int i = 0; i < max_clients_; ++i)
  {
    auto accept_sock_pending = firelink::Socket::create(io_core);
    if (!accept_sock_pending.has_value())
    {
      close_all_clients();
      return accept_sock_pending.error();
    }

    clients_[i]->socket_ = std::move(accept_sock_pending.value());
    clients_[i]->listener_socket_ = listener_;

    firelink::ErrorCode error = reset_client(clients_[i]);
    if (error != firelink::ErrorCode::Success)
    {
      close_all_clients();
      return error;
    }
  }

  return firelink::ErrorCode::Success;
}

firelink::ErrorCode AsyncTCPServer::close_all_clients()
{
  firelink::ErrorCode ret = firelink::ErrorCode::Success;
  for (int i = 0; i < max_clients_; ++i)
  {
    firelink::ErrorCode error = clients_[i]->socket_->close();
    if (error != firelink::ErrorCode::Success)
    {
      ret = error;
    }
  }

  return ret;
}

firelink::ErrorCode AsyncTCPServer::reset_client(std::shared_ptr<ClientContext> context)
{
  firelink::ErrorCode error = context->socket_->close();
  if (error != firelink::ErrorCode::Success)
  {
    return error;
  }

  error = context->socket_->socket(firelink::AddressFamily::Unspecified,
                                   firelink::SocketType::Stream, firelink::Protocol::Tcp);
  if (error != firelink::ErrorCode::Success)
  {
    return error;
  }

  if (std::shared_ptr<firelink::Socket> listener_ptr = context->listener_socket_.lock())
  {
    error = listener_ptr->start_accept(context->socket_, std::static_pointer_cast<void>(context),
                                       on_accept_complete);

    if (error != firelink::ErrorCode::Success)
    {
      return error;
    }

    return firelink::ErrorCode::Success;
  }

  return firelink::ErrorCode::SystemError;
}
