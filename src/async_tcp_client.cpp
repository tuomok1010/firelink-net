#include "firelink/async_tcp_client.hpp"
#include <memory>

firelink::AsyncTCPClient::AsyncTCPClient() : socket_(nullptr), connected_(false)
{
}

firelink::AsyncTCPClient::~AsyncTCPClient()
{
}

firelink::ErrorCode firelink::AsyncTCPClient::init(std::shared_ptr<firelink::IOCore> io_core)
{
  auto sock_pending = firelink::Socket::create(io_core);
  if (!sock_pending.has_value())
  {
    return ErrorCode::SystemError;
  }

  socket_ = std::move(sock_pending.value());
  return socket_->socket(firelink::AddressFamily::Unspecified, firelink::SocketType::Stream,
                         firelink::Protocol::Tcp);
}

firelink::ErrorCode firelink::AsyncTCPClient::close()
{
  return socket_->close();
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
firelink::ErrorCode firelink::AsyncTCPClient::connect(const firelink::Endpoint& dst,
                                                      std::shared_ptr<void> user_op_data,
                                                      ConnectHandler handler)
{
  auto ctx = std::make_shared<OpContext>();
  ctx->client_ = weak_from_this();
  ctx->user_op_data_ = std::move(user_op_data);

  return socket_->start_connect(dst, std::static_pointer_cast<void>(ctx),
                                [ctx, handler](auto caller, auto op_data, auto err, auto tag)
                                {
                                  auto client = ctx->client_.lock();
                                  if (!client)
                                    return; // client was destroyed

                                  if (handler)
                                  {
                                    handler(client, err, ctx->user_op_data_);
                                  }

                                  if (err == ErrorCode::Success)
                                    client->connected_ = true;
                                });
}

firelink::ErrorCode firelink::AsyncTCPClient::disconnect(std::shared_ptr<void> user_op_data,
                                                         DisconnectHandler handler)
{
  auto ctx = std::make_shared<OpContext>();
  ctx->client_ = weak_from_this();
  ctx->user_op_data_ = std::move(user_op_data);

  return socket_->start_disconnect(false, std::static_pointer_cast<void>(ctx),
                                   [ctx, handler](auto caller, auto op_data, auto err, auto tag)
                                   {
                                     auto client = ctx->client_.lock();
                                     if (!client)
                                       return; // client was destroyed

                                     if (handler)
                                     {
                                       handler(client, err, ctx->user_op_data_);
                                     }

                                     if (err == ErrorCode::Success)
                                       client->connected_ = false;
                                   });
}

firelink::ErrorCode firelink::AsyncTCPClient::recv(std::span<std::byte> buffer,
                                                   std::shared_ptr<void> user_op_data,
                                                   RecvHandler handler)
{
  auto ctx = std::make_shared<OpContext>();
  ctx->client_ = weak_from_this();
  ctx->user_op_data_ = std::move(user_op_data);

  return socket_->start_recv(
    buffer, std::static_pointer_cast<void>(ctx),
    [ctx, handler](auto caller, auto buf, auto op_data, auto err, auto bytes_transferred, auto tag)
    {
      auto client = ctx->client_.lock();
      if (!client)
        return; // client was destroyed

      if (bytes_transferred == 0)
        client->connected_ = false;

      if (handler)
      {
        handler(client, buf, bytes_transferred, err, ctx->user_op_data_);
      }
    });
}

firelink::ErrorCode firelink::AsyncTCPClient::send(std::span<std::byte> data,
                                                   std::shared_ptr<void> user_op_data,
                                                   SendHandler handler)
{
  auto ctx = std::make_shared<OpContext>();
  ctx->client_ = weak_from_this();
  ctx->user_op_data_ = std::move(user_op_data);

  return socket_->start_send(
    data, std::static_pointer_cast<void>(ctx),
    [ctx, handler](auto caller, auto buf, auto op_data, auto err, auto bytes_transferred, auto tag)
    {
      auto client = ctx->client_.lock();
      if (!client)
        return; // client was destroyed

      if (handler)
      {
        handler(client, buf, bytes_transferred, err, ctx->user_op_data_);
      }
    });
}
#pragma clang diagnostic pop
