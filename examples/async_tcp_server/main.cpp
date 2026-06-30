#include "async_tcp_server.hpp"

#include <cstdint>
#include <iostream>

static constexpr std::uint32_t IO_THREADPOOL_MIN_THREADS = 2;
static constexpr std::uint32_t IO_THREADPOOL_MAX_THREADS = 4;
static constexpr std::uint32_t USER_THREADPOOL_MIN_THREADS = 2;
static constexpr std::uint32_t USER_THREADPOOL_MAX_THREADS = 4;

static constexpr std::uint32_t MAX_CLIENTS = 10000;

int main()
{
  auto io_core_pending =
    firelink::IOCore::create({IO_THREADPOOL_MIN_THREADS, IO_THREADPOOL_MAX_THREADS,
                              USER_THREADPOOL_MIN_THREADS, USER_THREADPOOL_MAX_THREADS});

  if (!io_core_pending.has_value())
  {
    std::cerr << "firelink::IOCore::create error " << static_cast<int>(io_core_pending.error())
              << std::endl;
    return -1;
  }

  std::shared_ptr<firelink::IOCore> io_core = std::move(io_core_pending.value());

  firelink::ErrorCode error = io_core->initialize();
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::IOCore::initialize error " << static_cast<int>(error) << std::endl;
    return -1;
  }

  firelink::Endpoint listener_ep = firelink::Endpoint(firelink::IPv4Address({127, 0, 0, 1}), 63000);
  AsyncTCPServer server(MAX_CLIENTS);

  while (1)
  {
    error = server.run(io_core, listener_ep, 5);
    if (error != firelink::ErrorCode::Success)
    {
      std::cerr << "AsyncTCPServer::run error " << static_cast<int>(error) << std::endl;
      server.close();
      break;
    }

    io_core->run();
    server.close();
  }

  std::cout << "resources released." << std::endl;
  return 0;
}
