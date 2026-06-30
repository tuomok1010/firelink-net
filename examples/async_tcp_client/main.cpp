#include "async_tcp_client.hpp"
#include <iostream>

const static constexpr char* target_addr = "127.0.0.1:63000";

static int process_args(int argc, char** argv, firelink::Endpoint& target_ep)
{
  // Assign the default addr + port to the endpoint. Will be overwritten if user supplies addr+port
  // via cmdline args
  if (firelink::inet_pton(firelink::AddressFamily::IPv4, target_addr, target_ep) !=
      firelink::ErrorCode::Success)
  {
    return -1;
  }

  for (int i = 1; i < argc; ++i)
  {
    if (i < argc - 1)
    {
      // -t = target <ip>:<port>
      if (std::strcmp(argv[i], "-t") == 0)
      {
        firelink::AddressFamily family = firelink::str_to_family(argv[i + 1]);
        if (firelink::inet_pton(family, argv[i + 1], target_ep) != firelink::ErrorCode::Success)
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
  firelink::Endpoint target_ep{};
  if (process_args(argc, argv, target_ep) != 0)
  {
    std::cerr << "error parsing args!" << std::endl;
    return -1;
  }

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
