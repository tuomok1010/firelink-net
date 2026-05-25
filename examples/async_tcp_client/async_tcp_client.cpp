#include "async_tcp_client.hpp"
#include <iostream>
#include <memory>

AsyncTCPClient::AsyncTCPClient() : socket_(nullptr)
{
}

AsyncTCPClient::~AsyncTCPClient()
{
}

firelink::ErrorCode AsyncTCPClient::run(std::shared_ptr<firelink::IOCore> io_core,
                                        firelink::Endpoint target_ep)
{
  auto sock_pending = firelink::Socket::create(io_core);
  if (!sock_pending.has_value())
  {
    return firelink::ErrorCode::SystemError;
  }

  socket_ = std::move(sock_pending.value());
  firelink::ErrorCode error = socket_->socket(
    firelink::AddressFamily::IPv4, firelink::SocketType::Stream, firelink::Protocol::Tcp);

  auto sock_buf = std::make_shared<SocketBuffer>();
  error = socket_->start_connect(target_ep, std::static_pointer_cast<void>(sock_buf),
                                 on_connect_complete);

  if (error != firelink::ErrorCode::Success)
  {
    socket_->close();
    return error;
  }

  return firelink::ErrorCode::Success;
}

firelink::ErrorCode AsyncTCPClient::close()
{
  return socket_->close();
}

void AsyncTCPClient::on_connect_complete(std::shared_ptr<firelink::Socket> caller,
                                         std::shared_ptr<void> user_op_data,
                                         firelink::ErrorCode error, firelink::ConnectTag tag)
{
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "socket error " << std::to_string(static_cast<int>(error)) << std::endl;
    caller->stop_io_context();
    return;
  }

  firelink::Endpoint local_endpoint{};
  firelink::Endpoint peer_endpoint{};
  caller->get_sock_name(local_endpoint);
  caller->get_peer_name(peer_endpoint);

  std::cout << firelink::inet_ntop(caller->get_addr_family(), local_endpoint) << " connected to "
            << firelink::inet_ntop(caller->get_addr_family(), peer_endpoint) << std::endl;

  // IMPORTANT NOTE: Should NOT use blocking function calls in socket callbacks!!
  std::string msg{};
  std::cin >> msg;

  auto sock_buffer = std::static_pointer_cast<SocketBuffer>(user_op_data);
  auto write_span =
    std::span<std::byte>(sock_buffer->write_buffer_).first(static_cast<std::size_t>(msg.length()));

  std::memcpy(write_span.data(), msg.data(), msg.length());
  if (caller->start_send(write_span, user_op_data, on_send_complete) !=
      firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_send() error!" << std::endl;
    caller->stop_io_context();
    return;
  }

  sock_buffer->read_buffer_.fill(std::byte{0});
  if (caller->start_recv(std::span<std::byte>(sock_buffer->read_buffer_), user_op_data,
                         on_recv_complete) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_recv() error!" << std::endl;
    caller->stop_io_context();
    return;
  }
}

void AsyncTCPClient::on_disconnect_complete(std::shared_ptr<firelink::Socket> caller,
                                            std::shared_ptr<void> user_op_data,
                                            firelink::ErrorCode error, firelink::DisconnectTag tag)
{
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "socket error " << std::to_string(static_cast<int>(error)) << std::endl;
    caller->stop_io_context();
    return;
  }

  firelink::Endpoint local_endpoint{};
  firelink::Endpoint peer_endpoint{};
  caller->get_sock_name(local_endpoint);
  caller->get_peer_name(peer_endpoint);

  std::cout << firelink::inet_ntop(caller->get_addr_family(), local_endpoint)
            << " disconnecting from "
            << firelink::inet_ntop(caller->get_addr_family(), peer_endpoint) << std::endl;
}

void AsyncTCPClient::on_recv_complete(std::shared_ptr<firelink::Socket> caller,
                                      std::span<std::byte> user_buffer,
                                      std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                                      std::int32_t bytes_transferred, firelink::ReadTag tag)
{
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "AsyncTCPClient::on_recv_complete() error "
              << std::to_string(static_cast<int>(error)) << std::endl;
    caller->stop_io_context();
    return;
  }

  if (bytes_transferred < 0)
  {
    std::cerr << "AsyncTCPClient::on_recv_complete() error!" << std::endl;
    caller->stop_io_context();
    return;
  }
  else if (bytes_transferred == 0)
  {
    std::cout << "disconnected." << std::endl;
    caller->stop_io_context();
    return;
  }

  std::string str(reinterpret_cast<const char*>(user_buffer.data()), user_buffer.size());

  std::cout << "received  " << bytes_transferred << " bytes"
            << ", data:\n"
            << str << std::endl;

  auto sock_buffer = std::static_pointer_cast<SocketBuffer>(user_op_data);
  sock_buffer->read_buffer_.fill(std::byte{0});

  if (caller->start_recv(std::span<std::byte>(sock_buffer->read_buffer_), user_op_data,
                         on_recv_complete) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_recv() error!" << std::endl;
    caller->stop_io_context();
    return;
  }
}

void AsyncTCPClient::on_send_complete(std::shared_ptr<firelink::Socket> caller,
                                      std::span<std::byte> user_buffer,
                                      std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                                      std::int32_t bytes_transferred, firelink::WriteTag tag)
{
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "socket error " << std::to_string(static_cast<int>(error)) << std::endl;
    caller->stop_io_context();
    return;
  }

  std::cout << "sent " << bytes_transferred << " bytes" << std::endl;

  // IMPORTANT NOTE: Should NOT use blocking function calls in socket callbacks!!
  std::string msg{};
  std::cin >> msg;

  auto sock_buffer = std::static_pointer_cast<SocketBuffer>(user_op_data);
  auto write_span =
    std::span<std::byte>(sock_buffer->write_buffer_).first(static_cast<std::size_t>(msg.length()));

  std::memcpy(write_span.data(), msg.data(), msg.length());
  if (caller->start_send(write_span, user_op_data, on_send_complete) !=
      firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_send() error!" << std::endl;
    caller->stop_io_context();
    return;
  }
}
