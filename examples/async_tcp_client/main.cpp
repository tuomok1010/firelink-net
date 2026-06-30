#include "async_tcp_client.hpp"
#include <iostream>

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

  firelink::Endpoint target_ep = firelink::Endpoint(firelink::IPv4Address({127, 0, 0, 1}), 63000);
  AsyncTCPClient client;
  
  err = client.run(io_core, target_ep);
  if (err != firelink::ErrorCode::Success)
  {
    std::cerr << "AsyncTCPClient::run error " << static_cast<int>(err) << std::endl;
    return -1;
  }

  io_core->run();

  if (client.close() != firelink::ErrorCode::Success)
  {
    std::cerr << "AsyncTCPClient::close() error!" << std::endl;
    io_core->release();
    return -1;
  }
  std::cout << "client closed." << std::endl;

  if (io_core->release() != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::IOCore::release() error" << std::endl;
    return -1;
  }
  std::cout << "resources released." << std::endl;

  return 0;
}
