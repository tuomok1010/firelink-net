#include "firelink/socket.hpp"
#include <cstring>
#include <iostream>
#include <array>

static constexpr int READ_BUFFER_LEN = 512;
static constexpr int WRITE_BUFFER_LEN = 512;

struct SocketBuffer
{
  std::array<std::byte, READ_BUFFER_LEN> read_buffer_;
  std::array<std::byte, WRITE_BUFFER_LEN> write_buffer_;
};

static void on_connect_complete(std::shared_ptr<firelink::Socket> caller,
                                std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                                firelink::ConnectTag tag);

static void on_send_complete(std::shared_ptr<firelink::Socket> caller,
                             std::span<std::byte> user_buffer, std::shared_ptr<void> user_op_data,
                             firelink::ErrorCode error, std::int32_t bytes_transferred,
                             firelink::WriteTag tag);

static void on_recv_complete(std::shared_ptr<firelink::Socket> caller,
                             std::span<std::byte> user_buffer, std::shared_ptr<void> user_op_data,
                             firelink::ErrorCode error, std::int32_t bytes_transferred,
                             firelink::ReadTag tag);

static void on_disconnect_complete(std::shared_ptr<firelink::Socket> caller,
                                   std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                                   firelink::DisconnectTag tag);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
static void on_connect_complete(std::shared_ptr<firelink::Socket> caller,
                                std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                                firelink::ConnectTag tag)
#pragma clang diagnostic pop
{
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "socket error " << std::to_string(static_cast<int>(error)) << std::endl;
    caller->stop_io_context();
    return;
  }

  std::cout << "connect complete" << std::endl;

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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
static void on_send_complete(std::shared_ptr<firelink::Socket> caller,
                             std::span<std::byte> user_buffer, std::shared_ptr<void> user_op_data,
                             firelink::ErrorCode error, std::int32_t bytes_transferred,
                             firelink::WriteTag tag)
#pragma clang diagnostic pop
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
static void on_recv_complete(std::shared_ptr<firelink::Socket> caller,
                             std::span<std::byte> user_buffer, std::shared_ptr<void> user_op_data,
                             firelink::ErrorCode error, std::int32_t bytes_transferred,
                             firelink::ReadTag tag)
#pragma clang diagnostic pop

{
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "socket error " << std::to_string(static_cast<int>(error)) << std::endl;
    caller->stop_io_context();
    return;
  }

  if (bytes_transferred < 0)
  {
    std::cerr << "firelink::Socket::recv() error!" << std::endl;
    caller->stop_io_context();
    return;
  }
  else if (bytes_transferred == 0)
  {
    std::cout << "disconnected." << std::endl;
    caller->stop_io_context();
    return;
  }
  else
  {
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
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
static void on_disconnect_complete(std::shared_ptr<firelink::Socket> caller,
                                   std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                                   firelink::DisconnectTag tag)
#pragma clang diagnostic pop
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

int main()
{
  auto io_core_pending = firelink::IOCore::create({2, 2, 2, 2});
  if (!io_core_pending.has_value())
  {
    std::cerr << "firelink::IOCore::create error " << static_cast<int>(io_core_pending.error())
              << std::endl;
    return -1;
  }

  std::shared_ptr<firelink::IOCore> io_core = std::move(io_core_pending.value());

  firelink::ErrorCode err = io_core->initialize();
  if (err != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::IOCore::initialize error " << static_cast<int>(err) << std::endl;
    return -1;
  }

  auto sock_pending = firelink::Socket::create(io_core);
  if (!sock_pending.has_value())
  {
    std::cerr << "firelink::Socket::create error " << static_cast<int>(sock_pending.error())
              << std::endl;
    io_core->release();
    return -1;
  }

  auto sock = std::move(sock_pending.value());

  if (sock->socket(firelink::AddressFamily::IPv4, firelink::SocketType::Stream,
                   firelink::Protocol::Tcp) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::socket() error!" << std::endl;
    io_core->release();
    return -1;
  }

  firelink::Endpoint target_ep = firelink::Endpoint(firelink::IPv4Address({127, 0, 0, 1}, 63000));

  std::cout << "connecting to " << firelink::inet_ntop(firelink::AddressFamily::IPv4, target_ep)
            << std::endl;

  auto socket_buffer = std::make_shared<SocketBuffer>();

  if (sock->start_connect(target_ep, std::static_pointer_cast<void>(socket_buffer),
                          on_connect_complete) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::socket() error!" << std::endl;
    sock->close();
    io_core->release();
    return -1;
  }

  io_core->run();

  if (sock->close() != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::close() error!" << std::endl;
    io_core->release();
    return -1;
  }
  std::cout << "socket closed." << std::endl;

  if (io_core->release() != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::IOCore::release() error" << std::endl;
    return -1;
  }
  std::cout << "resources released." << std::endl;

  return 0;
}
