#include "firelink/io_core.hpp"
#include "firelink/socket.hpp"
#include <cstdint>
#include <iostream>
#include <array>
#include <memory>

static constexpr int MAX_N_CLIENTS = 2;
static constexpr int READ_BUFFER_LEN = 512;
static constexpr int WRITE_BUFFER_LEN = 512;

static std::shared_ptr<firelink::Socket> listener = nullptr;

struct SocketBuffer
{
  std::array<std::byte, READ_BUFFER_LEN> read_buffer_;
  std::array<std::byte, WRITE_BUFFER_LEN> write_buffer_;
};

static void on_accept_complete(std::shared_ptr<firelink::Socket> caller,
                               std::shared_ptr<firelink::Socket> accepted_socket,
                               const firelink::Endpoint& local_endpoint,
                               const firelink::Endpoint& peer_endpoint,
                               std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                               firelink::AcceptTag tag);

static void on_send_complete(std::shared_ptr<firelink::Socket> caller,
                             std::span<std::byte> user_buffer, std::shared_ptr<void> user_op_data,
                             firelink::ErrorCode error, std::int32_t bytes_transferred,
                             firelink::WriteTag tag);

static void on_recv_complete(std::shared_ptr<firelink::Socket> caller,
                             std::span<std::byte> user_buffer, std::shared_ptr<void> user_op_data,
                             firelink::ErrorCode error, std::int32_t bytes_transferred,
                             firelink::ReadTag tag);

static int
initialize_client_sockets(std::shared_ptr<firelink::IOCore> io_core,
                          std::shared_ptr<firelink::Socket> listener,
                          std::array<std::shared_ptr<firelink::Socket>, MAX_N_CLIENTS>& clients,
                          std::array<std::shared_ptr<SocketBuffer>, MAX_N_CLIENTS>& socketBuffers);

static int initialize_client_socket(std::shared_ptr<firelink::Socket> listener_socket,
                                    std::shared_ptr<firelink::Socket> client,
                                    std::shared_ptr<SocketBuffer> socketBuffer);

static int
close_client_sockets(std::array<std::shared_ptr<firelink::Socket>, MAX_N_CLIENTS>& clients);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
static void on_accept_complete(std::shared_ptr<firelink::Socket> caller,
                               std::shared_ptr<firelink::Socket> accepted_socket,
                               const firelink::Endpoint& local_endpoint,
                               const firelink::Endpoint& peer_endpoint,
                               std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                               firelink::AcceptTag tag)
#pragma clang diagnostic pop
{
  /*
   * Close erronous socket, attempt to make it available for accept again
   */
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "socket error " << std::to_string(static_cast<int>(error)) << std::endl;
    if (initialize_client_socket(caller, accepted_socket,
                                 std::static_pointer_cast<SocketBuffer>(user_op_data)) != 0)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
      caller->stop_io_context();
    }
    return;
  }

  auto accepted = std::shared_ptr<firelink::Socket>(std::move(accepted_socket));
  auto sock_buffer = std::static_pointer_cast<SocketBuffer>(user_op_data);

  std::cout << firelink::inet_ntop(caller->get_addr_family(), local_endpoint)
            << " accepted connection from "
            << firelink::inet_ntop(accepted->get_addr_family(), peer_endpoint) << std::endl;

  if (accepted->start_recv(std::span<std::byte>(sock_buffer->read_buffer_), user_op_data,
                           on_recv_complete) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_recv() error!" << std::endl;
    if (initialize_client_socket(caller, accepted_socket,
                                 std::static_pointer_cast<SocketBuffer>(user_op_data)) != 0)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
      caller->stop_io_context();
    }
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
  /*
   * Close erronous socket, attempt to make it available for accept again
   */
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "socket error " << std::to_string(static_cast<int>(error)) << std::endl;
    if (initialize_client_socket(listener, caller,
                                 std::static_pointer_cast<SocketBuffer>(user_op_data)) != 0)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
      caller->stop_io_context();
    }
    return;
  }

  std::cout << "sent " << bytes_transferred << " bytes" << std::endl;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
static void on_recv_complete(std::shared_ptr<firelink::Socket> caller,
                             std::span<std::byte> user_buffer, std::shared_ptr<void> user_op_data,
                             firelink::ErrorCode error, std::int32_t bytes_transferred,
                             firelink::ReadTag tag)
#pragma clang diagnostic pop
{
  /*
   * Close erronous socket, attempt to make it available for accept again
   */
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "socket error " << std::to_string(static_cast<int>(error)) << std::endl;
    if (initialize_client_socket(listener, caller,
                                 std::static_pointer_cast<SocketBuffer>(user_op_data)) != 0)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
      caller->stop_io_context();
    }
    return;
  }

  if (bytes_transferred < 0)
  {
    std::cerr << "firelink::Socket::recv() error!" << std::endl;
    if (initialize_client_socket(listener, caller,
                                 std::static_pointer_cast<SocketBuffer>(user_op_data)) != 0)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
      caller->stop_io_context();
    }
    return;
  }
  else if (bytes_transferred == 0)
  {
    std::cout << "client disconnected." << std::endl;
    if (initialize_client_socket(listener, caller,
                                 std::static_pointer_cast<SocketBuffer>(user_op_data)) != 0)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
      caller->stop_io_context();
    }
    return;
  }
  else
  {
    std::cout << "received " << bytes_transferred << " bytes" << std::endl;

    auto sock_buffer = std::static_pointer_cast<SocketBuffer>(user_op_data);
    auto write_span = std::span<std::byte>(sock_buffer->write_buffer_)
                        .first(static_cast<std::size_t>(bytes_transferred));
    std::memcpy(write_span.data(), user_buffer.data(), static_cast<uint32_t>(bytes_transferred));

    if (caller->start_send(write_span, user_op_data, on_send_complete) !=
        firelink::ErrorCode::Success)
    {
      std::cerr << "firelink::Socket::start_send() error!" << std::endl;
      if (initialize_client_socket(listener, caller,
                                   std::static_pointer_cast<SocketBuffer>(user_op_data)) != 0)
      {
        std::cerr << "initialize_client_socket() error!" << std::endl;
        caller->stop_io_context();
      }
      return;
    }

    sock_buffer->read_buffer_.fill(std::byte{0});
    if (caller->start_recv(std::span<std::byte>(sock_buffer->read_buffer_), user_op_data,
                           on_recv_complete) != firelink::ErrorCode::Success)
    {
      std::cerr << "firelink::Socket::start_recv() error!" << std::endl;
      if (initialize_client_socket(listener, caller,
                                   std::static_pointer_cast<SocketBuffer>(user_op_data)) != 0)
      {
        std::cerr << "initialize_client_socket() error!" << std::endl;
        caller->stop_io_context();
      }
      return;
    }
  }
}

static int
initialize_client_sockets(std::shared_ptr<firelink::IOCore> io_core,
                          std::shared_ptr<firelink::Socket> listener_socket,
                          std::array<std::shared_ptr<firelink::Socket>, MAX_N_CLIENTS>& clients,
                          std::array<std::shared_ptr<SocketBuffer>, MAX_N_CLIENTS>& socketBuffers)
{
  for (unsigned int i = 0; i < MAX_N_CLIENTS; ++i)
  {
    auto accept_sock_pending = firelink::Socket::create(io_core);
    if (!accept_sock_pending.has_value())
    {
      std::cerr << "firelink::Socket::create error "
                << static_cast<int>(accept_sock_pending.error()) << std::endl;
      close_client_sockets(clients);
      return -1;
    }

    clients[i] = std::move(accept_sock_pending.value());
    socketBuffers[i] = std::make_shared<SocketBuffer>();

    if (initialize_client_socket(listener_socket, clients[i], socketBuffers[i]) != 0)
    {
      std::cerr << "initialize_client_socket() error!" << std::endl;
      close_client_sockets(clients);
      return -1;
    }
  }

  return 0;
}

static int initialize_client_socket(std::shared_ptr<firelink::Socket> listener_socket,
                                    std::shared_ptr<firelink::Socket> client,
                                    std::shared_ptr<SocketBuffer> socket_buffer)
{
  // Close the socket in case it has been initialized already
  if (client->close() != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::close() error!" << std::endl;
    return -1;
  }

  // Reopen socket
  if (client->socket(firelink::AddressFamily::IPv4, firelink::SocketType::Stream,
                     firelink::Protocol::Tcp) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::socket() error!" << std::endl;
    return -2;
  }

  // Ready to accept a new connection in the socket
  if (listener_socket->start_accept(client, std::static_pointer_cast<void>(socket_buffer),
                                    on_accept_complete) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_accept() error!" << std::endl;
    return -3;
  }

  return 0;
}

static int
close_client_sockets(std::array<std::shared_ptr<firelink::Socket>, MAX_N_CLIENTS>& clients)
{
  int ret = 0;
  for (unsigned int i = 0; i < MAX_N_CLIENTS; ++i)
  {
    if (clients[i]->close() != firelink::ErrorCode::Success)
    {
      std::cerr << "firelink::Socket::close() error!" << std::endl;
      ret = -1;
    }
  }

  return ret;
}

int main(int argc, char** argv)
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

  auto listener_pending = firelink::Socket::create(io_core);
  if (!listener_pending.has_value())
  {
    std::cerr << "firelink::Socket::create error " << static_cast<int>(listener_pending.error())
              << std::endl;
    io_core->release();
    return -1;
  }

  /*std::shared_ptr<firelink::Socket>*/ listener = std::move(listener_pending.value());

  if (listener->socket(firelink::AddressFamily::IPv4, firelink::SocketType::Stream,
                       firelink::Protocol::Tcp) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::socket() error!" << std::endl;
    io_core->release();
    return -1;
  }

  firelink::Endpoint listener_ep = firelink::Endpoint(firelink::IPv4Address({127, 0, 0, 1}, 63000));
  if (listener->bind(listener_ep) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::bind() error " << std::endl;
    io_core->release();
    return -1;
  }

  std::cout << "listener bound to "
            << firelink::inet_ntop(firelink::AddressFamily::IPv4, listener_ep) << std::endl;

  if (listener->listen(5) != firelink::ErrorCode::Success)
  {

    std::cerr << "firelink::Socket::listen() error!" << std::endl;
    listener->close();
    io_core->release();
    return -1;
  }

  std::array<std::shared_ptr<firelink::Socket>, MAX_N_CLIENTS> clients{};
  std::array<std::shared_ptr<SocketBuffer>, MAX_N_CLIENTS> socket_buffers{};

  if (initialize_client_sockets(io_core, listener, clients, socket_buffers) != 0)
  {
    std::cerr << "async_echo_server initialize_client_sockets() error!" << std::endl;
    listener->close();
    io_core->release();
    return -1;
  }

  std::cout << "waiting connections..." << std::endl;

  io_core->run();

  if (close_client_sockets(clients) != 0)
  {
    std::cerr << "async_echo_server close_client_sockets() error!" << std::endl;
    listener->close();
    io_core->release();
    return -1;
  }
  std::cout << "client sockets closed." << std::endl;

  if (listener->close() != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::close() error!" << std::endl;
    io_core->release();
    return -1;
  }
  std::cout << "listener socket closed." << std::endl;

  if (io_core->release() != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::IOCore::release error " << static_cast<int>(err) << std::endl;
    return -1;
  }

  std::cout << "resources released." << std::endl;
  return 0;
}
