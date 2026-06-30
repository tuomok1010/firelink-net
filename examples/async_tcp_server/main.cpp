#include "async_tcp_server.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>

const static constexpr std::uint32_t IO_THREADPOOL_MIN_THREADS = 2;
const static constexpr std::uint32_t IO_THREADPOOL_MAX_THREADS = 4;
const static constexpr std::uint32_t USER_THREADPOOL_MIN_THREADS = 2;
const static constexpr std::uint32_t USER_THREADPOOL_MAX_THREADS = 4;

// These can be overwritten by cmdline args
const static constexpr std::uint32_t MAX_CLIENTS = 10000;
const static constexpr char* listener_addr = "127.0.0.1:63000";

static int process_args(int argc, char** argv, firelink::Endpoint& listener_ep,
                        std::int32_t& max_clients)
{
  // Assign the default addr + port to the endpoint. Will be overwritten if user supplies addr+port
  // via cmdline args
  if (firelink::inet_pton(firelink::AddressFamily::IPv4, listener_addr, listener_ep) !=
      firelink::ErrorCode::Success)
  {
    return -1;
  }

  // Assign the default max num clients
  max_clients = MAX_CLIENTS;

  for (int i = 1; i < argc; ++i)
  {
    if (i < argc - 1)
    {
      // -l = listener <ip>:<port>
      if (std::strcmp(argv[i], "-l") == 0)
      {
        firelink::AddressFamily family = firelink::str_to_family(argv[i + 1]);
        if (firelink::inet_pton(family, argv[i + 1], listener_ep) != firelink::ErrorCode::Success)
        {
          return -1;
        }
      }

      // -c = max clients count
      else if (std::strcmp(argv[i], "-c") == 0)
      {
        max_clients = strtol(argv[i + 1], nullptr, 10);
        if (max_clients == 0 || max_clients == LONG_MAX || max_clients == LONG_MIN)
        {
          return -1;
        }
      }
    }
  }

  return 0;
}

int main(int argc, char** argv)
{
  firelink::Endpoint listener_ep{};
  std::int32_t max_clients{};
  if (process_args(argc, argv, listener_ep, max_clients) != 0)
  {
    std::cerr << "error parsing args!" << std::endl;
    return -1;
  }

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
    
  AsyncTCPServer server(max_clients);

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
