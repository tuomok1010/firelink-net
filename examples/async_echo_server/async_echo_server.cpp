#include "firelink/io_core.hpp"
#include "firelink/socket.hpp"
#include <iostream>
#include <array>

static void on_accept_complete(std::shared_ptr<firelink::Socket> caller,
                               std::shared_ptr<firelink::Socket> accepted_socket,
                               const firelink::Endpoint& local_endpoint,
                               const firelink::Endpoint& peer_endpoint,
                               firelink::ErrorCode error, firelink::AcceptTag tag);

static void on_send_complete(std::shared_ptr<firelink::Socket> caller,
                             firelink::ErrorCode error,
                             std::int32_t bytes_transferred,
                             firelink::WriteTag tag);

static void on_recv_complete(std::shared_ptr<firelink::Socket> caller,
                             firelink::ErrorCode error,
                             std::int32_t bytes_transferred,
                             firelink::ReadTag tag);

std::array<std::byte, 512>& get_recv_buffer();
std::span<std::byte> get_recv_span();

std::array<std::byte, 512>& get_recv_buffer()
{
  static std::array<std::byte, 512> buffer{};
  return buffer;
}

std::span<std::byte> get_recv_span()
{
  static std::span<std::byte> span(get_recv_buffer());
  return span;
}

static bool run = true;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
static void on_accept_complete(std::shared_ptr<firelink::Socket> caller,
                               std::shared_ptr<firelink::Socket> accepted_socket,
                               const firelink::Endpoint& local_endpoint,
                               const firelink::Endpoint& peer_endpoint,
                               firelink::ErrorCode error, firelink::AcceptTag tag)
#pragma clang diagnostic pop
{ 
  if(error != firelink::ErrorCode::Success)
  {
    std::cerr << "socket error " << std::to_string(static_cast<int>(error)) << std::endl;
    run = false;
    return;
  }

  auto accepted = std::shared_ptr<firelink::Socket>(std::move(accepted_socket));
  
  std::cout
    << firelink::inet_ntop(caller->get_addr_family(), local_endpoint)
    << " accepted connection from "
    << firelink::inet_ntop(accepted->get_addr_family(), peer_endpoint)
    << std::endl;
 
  if(accepted->start_recv(get_recv_span(), on_recv_complete) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_recv() error!" << std::endl;
    run = false;
    return;
  }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
static void on_send_complete(std::shared_ptr<firelink::Socket> caller,
                             firelink::ErrorCode error, std::int32_t bytes_transferred,
                             firelink::WriteTag tag)
#pragma clang diagnostic pop
{
  if(error != firelink::ErrorCode::Success)
  {
    std::cerr << "socket error " << std::to_string(static_cast<int>(error)) << std::endl;
    run = false;
    return;
  }

  std::cout
    << "sent " << bytes_transferred << " bytes"
    << std::endl;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
static void on_recv_complete(std::shared_ptr<firelink::Socket> caller,
                             firelink::ErrorCode error, std::int32_t bytes_transferred,
                             firelink::ReadTag tag)
#pragma clang diagnostic pop
{ 
  if(error != firelink::ErrorCode::Success)
  {
    std::cerr << "socket error " << std::to_string(static_cast<int>(error)) << std::endl;
    run = false;
    return;
  }
    
  if(bytes_transferred < 0)
  {
    std::cerr << "firelink::Socket::recv() error!" << std::endl;
    run = false;
    return;
  }
  else if(bytes_transferred == 0)
  {
    std::cout << "client disconnected." << std::endl;
    run = false;
    return;
  }
  else
  {
    std::string str(reinterpret_cast<const char*>(get_recv_span().data()), get_recv_span().size());

    std::cout
      << "received " << bytes_transferred << " bytes"
      << ", data:\n" << str << std::endl;
    
    std::memcpy(get_recv_span().data(), "server test", 12);
    
    if(caller->start_send(get_recv_span(), on_send_complete) != firelink::ErrorCode::Success)
    {
      std::cerr << "firelink::Socket::start_send() error!" << std::endl;
      run = false;
      return;
    }

    if(caller->start_recv(get_recv_span(), on_recv_complete) != firelink::ErrorCode::Success)
    {
      std::cerr << "firelink::Socket::start_send() error!" << std::endl;
      run = false;
      return;
    }
  }
}

int main()
{
  auto io_core_pending = firelink::IOCore::create({2, 2, 2, 2});
  if(!io_core_pending.has_value())
  {
    std::cerr << "firelink::IOCore::create error " << static_cast<int>(io_core_pending.error()) << std::endl;
    return -1;
  }

  std::shared_ptr<firelink::IOCore> io_core = std::move(io_core_pending.value());
  
  firelink::ErrorCode err = io_core->initialize();
  if(err != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::IOCore::initialize error " << static_cast<int>(err) << std::endl;
    return -1;
  }

  auto sock_pending = firelink::Socket::create(io_core);
  if(!sock_pending.has_value())
  {
    std::cerr << "firelink::Socket::create error " << static_cast<int>(sock_pending.error()) << std::endl;
    io_core->release();
    return -1;
  }

  auto sock = std::move(sock_pending.value());
    
  if(sock->socket(firelink::AddressFamily::IPv4, firelink::SocketType::Stream, firelink::Protocol::Tcp) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::socket() error!" << std::endl;
    io_core->release();
    return -1;
  }

  firelink::Endpoint listener_ep = firelink::Endpoint(firelink::IPv4Address({127,0,0,1}, 63000));
  if(sock->bind(listener_ep) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::bind() error " << std::endl;
    io_core->release();
    return -1;
  }

  std::cout << "listener bound to " << firelink::inet_ntop(firelink::AddressFamily::IPv4, listener_ep) << std::endl;

  if(sock->listen(5) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::listen() error!" << std::endl;
    sock->close();
    io_core->release();
    return -1;    
  }

  auto accept_sock_pending = firelink::Socket::create(io_core);
  if(!accept_sock_pending.has_value())
  {
    std::cerr << "firelink::Socket::create error " << static_cast<int>(sock_pending.error()) << std::endl;
    sock->close();
    io_core->release();
    return -1;
  }

  auto accept_sock = std::move(accept_sock_pending.value());
  
  if(accept_sock->socket(firelink::AddressFamily::IPv4, firelink::SocketType::Stream, firelink::Protocol::Tcp) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::socket() error!" << std::endl;
    sock->close();
    io_core->release();
    return -1;
  }

  if(sock->start_accept(accept_sock, on_accept_complete) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_accept() error!" << std::endl;
    sock->close();
    accept_sock->close();
    io_core->release();
    return -1;    
  }
  
  std::cout << "waiting connections..." << std::endl;


  while(run)
  {
    
  }

  if(accept_sock->close() != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::close() error!" << std::endl;
    io_core->release();
    return -1;
  }
  std::cout << "socket closed." << std::endl;

  
  if(sock->close() != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::close() error!" << std::endl;
    io_core->release();
    return -1;
  }
  std::cout << "socket closed." << std::endl;

  if(io_core->release() != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::IOCore::release error " << static_cast<int>(err) << std::endl;
    return -1;
  }

  std::cout << "resources released." << std::endl;
  
  return 0;
}
