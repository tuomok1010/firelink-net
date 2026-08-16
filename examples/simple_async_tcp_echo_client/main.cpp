/*
   This is a simple async tcp client. It connects to a server, sends test msg(s), and waits
   for echos and then terminates.

   This example demonstrates the usage of the async API of the firelink::Socket class.
   If any errors occurs, the program will terminate.

   This includes:
   Initializing and using the firelink::IOCore class
   Creating firelink::Socket instances and initializing them
   Using user-defined async socket callback functions
   Passing user defined data to the socket callback functions

   This program is meant to be used with the simple_async_tcp_echo_server example.

   cmd line arguments:
   arg                             description                                               example
   -s, --server                 server endpoint 127.0.0.1:63000 /
   [::1]:63000

   -c, --client                 client endpoint 127.0.0.1:63001 /
   [::1]:63001

   -m, --message          message to send                                      "test message"

   -n, --num-packets    number of messages to send                  1

   -i, --interval             time in milliseconds between messages  1000
*/

#include "firelink/socket.hpp"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>

#include <chrono>
#include <stop_token>
#include <thread>

const static constexpr std::uint32_t IO_THREADPOOL_MIN_THREADS = 2;
const static constexpr std::uint32_t IO_THREADPOOL_MAX_THREADS = 4;
const static constexpr std::uint32_t USER_THREADPOOL_MIN_THREADS = 2;
const static constexpr std::uint32_t USER_THREADPOOL_MAX_THREADS = 4;

const static constexpr std::uint16_t READ_BUFFER_LEN = 1024;
const static constexpr std::uint16_t WRITE_BUFFER_LEN = 1024;

static firelink::Endpoint client_endpoint(firelink::IPv4Address({0, 0, 0, 0}), 0);
static firelink::Endpoint server_endpoint(firelink::IPv4Address({127, 0, 0, 1}), 63000);

static std::string message = "test";

// Used to inform main() if there was an error in the async callbacks
static std::atomic_int result = 0;

// Timer thread and packet send frequency
static std::jthread timer_worker{};
static std::chrono::milliseconds timer_interval = std::chrono::milliseconds(1000);

static std::atomic_int num_packets = 3;
static std::atomic_int packets_sent = 0;

// An instance of this will be used as user-defined data to the socket callback functions
struct UserData
{
  std::array<std::byte, READ_BUFFER_LEN> read_buffer_;
  std::array<std::byte, WRITE_BUFFER_LEN> write_buffer_;
};

void on_connect_complete(std::shared_ptr<firelink::Socket> caller,
                         std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                         firelink::ConnectTag tag);

void on_recv_complete(std::shared_ptr<firelink::Socket> caller, std::span<std::byte> user_buffer,
                      std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                      std::int32_t bytes_transferred, firelink::ReadTag tag);

void on_send_complete(std::shared_ptr<firelink::Socket> caller, std::span<std::byte> user_buffer,
                      std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                      std::int32_t bytes_transferred, firelink::WriteTag tag);

void on_disconnect_complete(std::shared_ptr<firelink::Socket> caller,
                            std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                            firelink::DisconnectTag tag);

static void timer_callback(std::shared_ptr<firelink::Socket> caller,
                           std::shared_ptr<UserData> user_op_data);

void on_connect_complete(std::shared_ptr<firelink::Socket> caller,
                         std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                         firelink::ConnectTag tag)
{
  // Should check for errors before proceeding
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "on_connect_complete() error " << static_cast<int>(error) << std::endl;
    caller->stop_io_context(); // breaks out of IOCore::run() in main
    result = -1;
    return;
  }

  firelink::Endpoint local_endpoint{};
  firelink::Endpoint peer_endpoint{};
  caller->get_sock_name(local_endpoint);
  caller->get_peer_name(peer_endpoint);

  // Log the local and peer endpoints of the new connection
  std::cout << firelink::inet_ntop(local_endpoint) << " connected to "
            << firelink::inet_ntop(peer_endpoint) << std::endl;

  // Access the user data passed in
  auto user_data = std::static_pointer_cast<UserData>(user_op_data);

  // Get a write span from user data
  auto write_span = std::span<std::byte>(user_data->write_buffer_)
                      .first(static_cast<std::size_t>(message.length()));

  // Copy the test msg into the write span
  std::memcpy(write_span.data(), message.data(), static_cast<uint32_t>(message.length()));

  // Send the first packet
  error = caller->start_send(write_span, user_op_data, on_send_complete);
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_send() error " << static_cast<int>(error) << std::endl;
    caller->stop_io_context();
    result = -1;
    return;
  }

  // Start the timer and send subsequent packets at each interval
  timer_worker = std::jthread(
    [caller, user_data](std::stop_token stoken)
    {
      while (!stoken.stop_requested())
      {
        std::this_thread::sleep_for(timer_interval);
        if (!stoken.stop_requested())
        {
          timer_callback(caller, user_data);
        }
      }
    });
}

void on_recv_complete(std::shared_ptr<firelink::Socket> caller, std::span<std::byte> user_buffer,
                      std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                      std::int32_t bytes_transferred, firelink::ReadTag tag)
{
  // Should check for errors before proceeding
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "on_recv_complete() error " << static_cast<int>(error) << std::endl;
    caller->stop_io_context(); // breaks out of IOCore::run() in main
    result = -1;
    return;
  }

  // Disconnect has occurred
  if (bytes_transferred <= 0)
  {
    std::cout << "disconnected." << std::endl;
    caller->stop_io_context();
    return;
  }

  // Get the user_data
  auto user_data = std::static_pointer_cast<UserData>(user_op_data);

  // Get a read span.
  auto read_span = std::span<std::byte>(user_data->read_buffer_);

  // Convert the received data into a string and log into console
  std::string str(reinterpret_cast<const char*>(user_buffer.data()), user_buffer.size());
  std::cout << "received  " << bytes_transferred << " bytes"
            << ", data:\n"
            << str << std::endl;

  // Ready to receive more
  error = caller->start_recv(read_span, user_op_data, on_recv_complete);
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_recv error " << static_cast<int>(error) << std::endl;
    caller->stop_io_context();
    result = -1;
    return;
  }
}

void on_send_complete(std::shared_ptr<firelink::Socket> caller, std::span<std::byte> user_buffer,
                      std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                      std::int32_t bytes_transferred, firelink::WriteTag tag)
{
  // Should check for errors before proceeding
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "on_send_complete() error " << static_cast<int>(error) << std::endl;
    caller->stop_io_context(); // breaks out of IOCore::run() in main
    result = -1;
    return;
  }

  ++packets_sent;

  // Log bytes sent into console
  std::cout << "sent  " << bytes_transferred << " bytes" << std::endl;
}

void on_disconnect_complete(std::shared_ptr<firelink::Socket> caller,
                            std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                            firelink::DisconnectTag tag)
{
  // Should check for errors before proceeding
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "on_disconnect_complete() error " << static_cast<int>(error) << std::endl;
    caller->stop_io_context(); // breaks out of IOCore::run() in main
    result = -1;
    return;
  }

  // Disconnect has been succesfully initiated
  std::cout << "disconnecting..." << std::endl;

  // Stop timer thread
  if (timer_worker.joinable())
  {
    timer_worker.request_stop();
  }

  // Stop io context to allow main() to continue
  caller->stop_io_context();
}

void timer_callback(std::shared_ptr<firelink::Socket> caller, std::shared_ptr<UserData> user_data)
{
  if (packets_sent < num_packets)
  {
    // Get a write span from user data
    auto write_span = std::span<std::byte>(user_data->write_buffer_)
                        .first(static_cast<std::size_t>(message.length()));

    // Copy the test msg into the write span
    std::memcpy(write_span.data(), message.data(), static_cast<uint32_t>(message.length()));

    // Send test data
    firelink::ErrorCode error = caller->start_send(write_span, user_data, on_send_complete);
    if (error != firelink::ErrorCode::Success)
    {
      std::cerr << "firelink::Socket::start_send() error " << static_cast<int>(error) << std::endl;
      caller->stop_io_context();
      result = -1;
      return;
    }
  }
  else
  {
    // Get a read span.
    auto read_span = std::span<std::byte>(user_data->read_buffer_);

    // Ready to disconnect
    firelink::ErrorCode error = caller->start_disconnect(false, user_data, on_disconnect_complete);
    if (error != firelink::ErrorCode::Success)
    {
      std::cerr << "firelink::Socket::start_send() error " << static_cast<int>(error) << std::endl;
      caller->stop_io_context();
      result = -1;
      return;
    }
  }
}

static int process_args(int argc, char** argv)
{
  for (int i = 1; i < argc; ++i)
  {
    // -h, --help
    if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0)
    {
      std::cout << "usage: simple_async_tcp_client OPTIONAL: [-s, --server] <127.0.0.1:63000> "
                   "/ <[::1]:63000> [-c, --client] <127.0.0.1:64551> / <[::1]:64551>"
                   "[-m, --message] <\"test message\"> [-n, --number] <10> [-i, --interval]: <1000>"
                << std::endl;
      return 1;
    }

    if (i < argc - 1)
    {
      // -c, --client
      if (std::strcmp(argv[i], "-c") == 0 || std::strcmp(argv[i], "--client") == 0)
      {
        firelink::AddressFamily client_family = firelink::str_to_family(argv[i + 1]);
        firelink::ErrorCode err = firelink::inet_pton(client_family, argv[i + 1], client_endpoint);
        if (err != firelink::ErrorCode::Success)
        {
          std::cerr << "firelink::inet_pton error " << static_cast<int>(err) << std::endl;
          return -1;
        }
      }

      // -s, --server
      if (std::strcmp(argv[i], "-s") == 0 || std::strcmp(argv[i], "--server") == 0)
      {
        firelink::AddressFamily server_family = firelink::str_to_family(argv[i + 1]);
        firelink::ErrorCode err = firelink::inet_pton(server_family, argv[i + 1], server_endpoint);
        if (err != firelink::ErrorCode::Success)
        {
          std::cerr << "firelink::inet_pton error " << static_cast<int>(err) << std::endl;
          return -1;
        }
      }

      // -m, --message
      if (std::strcmp(argv[i], "-m") == 0 || std::strcmp(argv[i], "--message") == 0)
      {
        message = std::string(argv[i + 1]);
      }

      // -n, --number
      if (std::strcmp(argv[i], "-n") == 0 || std::strcmp(argv[i], "--num-packets") == 0)
      {
        num_packets = std::stoi(argv[i + 1]);
      }

      // -i, --interval
      if (std::strcmp(argv[i], "-i") == 0 || std::strcmp(argv[i], "--interval") == 0)
      {
        timer_interval = std::chrono::milliseconds(std::stoi(argv[i + 1]));
      }
    }
  }

  return 0;
}

int main(int argc, char** argv)
{
  // Process cmd line arguments
  int res = process_args(argc, argv);
  if (res != 0)
  {
    return res;
  }

  // Create an IOCore instance
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

  // Intialize the IOCore instance
  firelink::ErrorCode error = io_core->initialize();
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::IOCore::initialize error " << static_cast<int>(error) << std::endl;
    return -1;
  }

  // Create a new instance of a firelink::Socket. It is associated with the IOCore passed in.
  auto socket_pending = firelink::Socket::create(io_core);
  if (!socket_pending.has_value())
  {
    std::cerr << "firelink::Socket::create error " << static_cast<int>(socket_pending.error())
              << std::endl;
    return -1;
  }

  /*
      Setup the socket with an address family, socket type, and protocol by calling
      firelink::Socket::socket(). NOTE: After calling firelink::Socket::socket(),
      firelink::Socket::close() should be called when done with the socket
   */
  std::shared_ptr<firelink::Socket> sock = std::move(socket_pending.value());
  error =
    sock->socket(client_endpoint.family(), firelink::SocketType::Stream, firelink::Protocol::Tcp);
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::socket error " << static_cast<int>(error) << std::endl;
    return -1;
  }

  // Bind the socket to the endpoint
  error = sock->bind(client_endpoint);
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::bind error " << static_cast<int>(error) << std::endl;
    sock->close();
    return -1;
  }

  // Create user defined data and pass it in an async socket callback.
  auto user_data = std::make_shared<UserData>();
  error = sock->start_connect(server_endpoint, std::static_pointer_cast<void>(user_data),
                              on_connect_complete);
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_connect error " << static_cast<int>(error) << std::endl;
    sock->close();
    return -1;
  }

  std::cout << "connecting to " << firelink::inet_ntop(server_endpoint) << std::endl;

  /*
      Calling IOCore::run will make the thread go to sleep until IOCore::stop is called
      User can also stop the io core  by calling firelink::Socket::stop_io_context()
   */
  io_core->run();

  sock->close();
  io_core->release();

  std::cout << "exiting" << std::endl;
  return result;
}
