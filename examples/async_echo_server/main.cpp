#include "echo_server.hpp"

#include <cstdint>
#include <iostream>

static constexpr std::uint32_t IO_THREADPOOL_MIN_THREADS = 2;
static constexpr std::uint32_t IO_THREADPOOL_MAX_THREADS = 4;
static constexpr std::uint32_t USER_THREADPOOL_MIN_THREADS = 2;
static constexpr std::uint32_t USER_THREADPOOL_MAX_THREADS = 4;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
int main(int argc, char** argv)
#pragma clang diagnostic pop
{
  firelink::IOCoreConfig config{};
  config.io_threadpool_min_threads_ = IO_THREADPOOL_MIN_THREADS;
  config.io_threadpool_max_threads_ = IO_THREADPOOL_MAX_THREADS;
  config.user_threadpool_min_threads_ = USER_THREADPOOL_MIN_THREADS;
  config.user_threadpool_max_threads_ = USER_THREADPOOL_MAX_THREADS;

  EchoServer server(config, 10000);
  server.init();

  std::cout << "starting server" << std::endl;
  while (true)
  {
    // (Re)init server
    server.init();
    
    // Blocks until failure
    server.start(firelink::Endpoint(firelink::IPv4Address({127, 0, 0, 1}, 63000)), 5);

    // Stop server
    server.stop();

    // Release server
    server.release();
  }

  std::cout << "resources released." << std::endl;
  return 0;
}
