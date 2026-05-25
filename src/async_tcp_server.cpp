#include "firelink/async_tcp_server.hpp"

firelink::AsyncTCPServer::AsyncTCPServer()
{
}

firelink::AsyncTCPServer::~AsyncTCPServer()
{
}

firelink::ErrorCode firelink::AsyncTCPServer::init(std::shared_ptr<firelink::IOCore> io_core,
                                                   std::size_t max_clients)
{
  if (clients_.size() > 0)
    clients_.clear();
  else
    clients_.reserve(max_clients_);

  for (int i = 0; i < max_clients_; ++i)
    clients_.push_back(std::make_shared<AsyncTCPClient>());

  auto listener_pending = firelink::Socket::create(io_core);
  if (!listener_pending.has_value())
  {
    return ErrorCode::SystemError;
  }

  listener_ = std::move(listener_pending.value());
  return 0;
}

firelink::ErrorCode firelink::AsyncTCPServer::close()
{
}

void firelink::AsyncTCPServer::stop_io_context()
{
}

firelink::ErrorCode firelink::AsyncTCPServer::listen(const firelink::Endpoint& endpoint,
                                                     int backlog,
                                                     std::shared_ptr<void> user_op_data,
                                                     AcceptHandler handler)
{
}

firelink::ErrorCode firelink::AsyncTCPServer::recv(std::span<std::byte> buffer,
                                                   std::shared_ptr<void> user_op_data,
                                                   RecvHandler handler)
{
}

firelink::ErrorCode firelink::AsyncTCPServer::send(std::span<std::byte> data,
                                                   std::shared_ptr<void> user_op_data,
                                                   SendHandler handler)
{
}

firelink::AddressFamily firelink::AsyncTCPServer::get_addr_family() const
{
}

firelink::Endpoint firelink::AsyncTCPServer::get_local_endpoint() const
{
}

firelink::Endpoint firelink::AsyncTCPServer::get_remote_endpoint() const
{
}
