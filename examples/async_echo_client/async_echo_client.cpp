#include "firelink/socket.hpp"
#include <iostream>
#include <array>

static void on_connect_complete(std::shared_ptr<firelink::Socket> caller,
                                const firelink::Endpoint& local_endpoint,
                                const firelink::Endpoint& peer_endpoint,
                                firelink::ErrorCode error,
                                firelink::ConnectTag tag);

static void on_send_complete(std::shared_ptr<firelink::Socket> caller,
                             firelink::ErrorCode error,
                             std::int32_t bytes_transferred,
                             firelink::WriteTag tag);

static void on_recv_complete(std::shared_ptr<firelink::Socket> caller,
                             firelink::ErrorCode error,
                             std::int32_t bytes_transferred,
                             firelink::ReadTag tag);

static void on_disconnect_complete(std::shared_ptr<firelink::Socket> caller,
                                   const firelink::Endpoint& local_endpoint,
                                   const firelink::Endpoint& peer_endpoint,
                                   firelink::ErrorCode error,
                                   firelink::DisconnectTag tag);

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
static void on_connect_complete(std::shared_ptr<firelink::Socket> caller,
                                const firelink::Endpoint& local_endpoint,
                                const firelink::Endpoint& peer_endpoint,
                                firelink::ErrorCode error,
                                firelink::ConnectTag tag)
#pragma clang diagnostic pop
{
  if(error != firelink::ErrorCode::Success)
  {
    std::cerr << "socket error " << std::to_string(static_cast<int>(error)) << std::endl;
    run = false;
    return;
  }

  std::cout
    << firelink::inet_ntop(caller->get_addr_family(), local_endpoint)
    << " connected to "
    << firelink::inet_ntop(caller->get_addr_family(), peer_endpoint)
    << std::endl;
    
  std::memcpy(get_recv_span().data(), "client test", 12);
    
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
static void on_send_complete(std::shared_ptr<firelink::Socket> caller,
                             firelink::ErrorCode error,
                             std::int32_t bytes_transferred,
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
                             firelink::ErrorCode error,
                             std::int32_t bytes_transferred,
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
    std::cout << "disconnected." << std::endl;
    run = false;
    return;
  }
  else
  {

    std::string str(reinterpret_cast<const char*>(get_recv_span().data()), get_recv_span().size());    

    std::cout
      << "received  " << bytes_transferred << " bytes"
      << ", data:\n" << str << std::endl;
    
    if(caller->start_disconnect(false, on_disconnect_complete) != firelink::ErrorCode::Success)
    {
      std::cerr << "firelink::Socket::start_disconnect() error!" << std::endl;
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
static void on_disconnect_complete(std::shared_ptr<firelink::Socket> caller,
                                   const firelink::Endpoint& local_endpoint,
                                   const firelink::Endpoint& peer_endpoint,
                                   firelink::ErrorCode error,
                                   firelink::DisconnectTag tag)
#pragma clang diagnostic pop
{
  if(error != firelink::ErrorCode::Success)
  {
    std::cerr << "socket error " << std::to_string(static_cast<int>(error)) << std::endl;
    run = false;
    return;
  }

  std::cout
    << firelink::inet_ntop(caller->get_addr_family(), local_endpoint)
    << " disconnecting from "
    << firelink::inet_ntop(caller->get_addr_family(), peer_endpoint)
    << std::endl;
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

  firelink::Endpoint target_ep = firelink::Endpoint(firelink::IPv4Address({127,0,0,1}, 63000));
    
  std::cout << "connecting to " << firelink::inet_ntop(firelink::AddressFamily::IPv4, target_ep) << std::endl;
  
  if(sock->start_connect(target_ep, on_connect_complete) != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::socket() error!" << std::endl;
    sock->close();
    io_core->release();
    return -1;    
  }

  while(run)
  {
    
  }

  if(sock->close() != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::close() error!" << std::endl;
    io_core->release();
    return -1;
  }
  std::cout << "socket closed." << std::endl;

  if(io_core->release() != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::IOCore::release() error" << std::endl;
    return -1;
  }
  std::cout << "resources released." << std::endl;
  
  return 0;
}
