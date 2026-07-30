/*
   This is a simple async tcp server. It listens for a connection, receives packets from the
   peer, echoes them back and waits for a shutdown from the peer, and then terminates.
   The data is expected as a string(const char*) message.

   This example demonstrates the usage of the async API of the firelink::Socket class.
   If any errors occurs, the program will terminate.

   This includes:
   Initializing and using the firelink::IOCore class
   Creating firelink::Socket instances and initializing them
   Using user-defined async socket callback functions
   Passing user defined data to the socket callback functions

   NOTES:
   This program has all of the async socket callbacks implemented as an example, but they are
   not all used

   This program is meant to be used with the simple_async_tcp_client example.

   cmd line arguments:
   arg                             description                                               example
   -e, --endpoint             server endpoint , optional                      127.0.0.1:63000 /
   [::1]:63000
*/

#include "firelink/socket.hpp"
#include <cstdint>
#include <iostream>

const static constexpr std::uint32_t IO_THREADPOOL_MIN_THREADS = 2;
const static constexpr std::uint32_t IO_THREADPOOL_MAX_THREADS = 4;
const static constexpr std::uint32_t USER_THREADPOOL_MIN_THREADS = 2;
const static constexpr std::uint32_t USER_THREADPOOL_MAX_THREADS = 4;

const static constexpr std::uint16_t READ_BUFFER_LEN = 1024;
const static constexpr std::uint16_t WRITE_BUFFER_LEN = 1024;

const static constexpr char* server_addr_str = "127.0.0.1:63000";
const static constexpr std::uint32_t backlog = 5;

// Used to inform main() if there was an error in the async callbacks
static std::atomic_int result = 0;

// An instance of this will be used as user-defined data to the socket callback functions
struct UserData
{
  std::array<std::byte, READ_BUFFER_LEN> read_buffer_;
  std::array<std::byte, WRITE_BUFFER_LEN> write_buffer_;
};

// Asynchronous socket operation callback declarations
void on_accept_complete(std::shared_ptr<firelink::Socket> caller,
                        std::shared_ptr<firelink::Socket> accepted_socket,
                        const firelink::Endpoint& local_endpoint,
                        const firelink::Endpoint& peer_endpoint, std::shared_ptr<void> user_op_data,
                        firelink::ErrorCode error, firelink::AcceptTag tag);

void on_connect_complete(std::shared_ptr<firelink::Socket> caller,
                         std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                         firelink::ConnectTag tag);

void on_recv_complete(std::shared_ptr<firelink::Socket> caller, std::span<std::byte> user_buffer,
                      std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                      std::int32_t bytes_transferred, firelink::ReadTag tag);

void on_recv_from_complete(std::shared_ptr<firelink::Socket> caller,
                           std::span<std::byte> user_buffer, std::shared_ptr<void> user_op_data,
                           firelink::ErrorCode error, std::int32_t bytes_transferred,
                           firelink::ReadTag tag);

void on_send_complete(std::shared_ptr<firelink::Socket> caller, std::span<std::byte> user_buffer,
                      std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                      std::int32_t bytes_transferred, firelink::WriteTag tag);

void on_send_to_complete(std::shared_ptr<firelink::Socket> caller, std::span<std::byte> user_buffer,
                         std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                         std::int32_t bytes_transferred, firelink::WriteTag tag);

void on_disconnect_complete(std::shared_ptr<firelink::Socket> caller,
                            std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                            firelink::DisconnectTag tag);

/*
 * Asynchronous socket operation callback implementations
 */
void on_accept_complete(std::shared_ptr<firelink::Socket> caller,
                        std::shared_ptr<firelink::Socket> accepted_socket,
                        const firelink::Endpoint& local_endpoint,
                        const firelink::Endpoint& peer_endpoint, std::shared_ptr<void> user_op_data,
                        firelink::ErrorCode error, firelink::AcceptTag tag)
{
  // Should check for errors before proceeding
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "on_accept_complete() error " << static_cast<int>(error) << std::endl;
    caller->stop_io_context(); // breaks out of IOCore::run() in main
    result = -1;
    return;
  }

  // Log the local and peer endpoints of the new connection
  std::cout << firelink::inet_ntop(local_endpoint) << " accepted connection from "
            << firelink::inet_ntop(peer_endpoint) << std::endl;

  // Access the user data passed into firelink::Socket::start_accept
  auto data = std::static_pointer_cast<UserData>(user_op_data);

  // The accepted socket is ready for socket operations such as firelink::Socket::start_recv()
  error = accepted_socket->start_recv(std::span<std::byte>(data->read_buffer_), user_op_data,
                                      on_recv_complete);
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_recv error " << static_cast<int>(error) << std::endl;
    caller->stop_io_context();
    result = -1;
    return;
  }
}

void on_connect_complete(std::shared_ptr<firelink::Socket> caller,
                         std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                         firelink::ConnectTag tag)
{
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

  if (bytes_transferred <= 0)
  {
    std::cout << "client disconnected." << std::endl;
    caller->stop_io_context();
    return;
  }

  // Convert the received data into a string and log into console
  std::string str(reinterpret_cast<const char*>(user_buffer.data()), user_buffer.size());
  std::cout << "received  " << bytes_transferred << " bytes"
            << ", data:\n"
            << str << std::endl;

  // Get the user_data
  auto user_data = std::static_pointer_cast<UserData>(user_op_data);

  /*
    Get a write span from user data
    NOTE: We do not want the entire buffer, otherwise we would send the entire buffer regardless of
    the actual msg length. We are making sure the span length = msg length
  */
  auto write_span = std::span<std::byte>(user_data->write_buffer_)
                      .first(static_cast<std::size_t>(bytes_transferred));

  // Copy the received msg into the write span
  std::memcpy(write_span.data(), user_buffer.data(), static_cast<uint32_t>(bytes_transferred));

  // Echo the msg back to the client
  error = caller->start_send(write_span, user_op_data, on_send_complete);
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_send() error " << static_cast<int>(error) << std::endl;
    caller->stop_io_context();
    result = -1;
    return;
  }
}

void on_recv_from_complete(std::shared_ptr<firelink::Socket> caller,
                           std::span<std::byte> user_buffer, std::shared_ptr<void> user_op_data,
                           firelink::ErrorCode error, std::int32_t bytes_transferred,
                           firelink::ReadTag tag)
{
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

  // Log bytes sent into console
  std::cout << "sent  " << bytes_transferred << " bytes" << std::endl;

  // Get the user_data
  auto user_data = std::static_pointer_cast<UserData>(user_op_data);

  // Get a read span. NOTE: This can (and should) be the entire buffer.
  auto read_span = std::span<std::byte>(user_data->read_buffer_);

  // Clear the recv buffer and get ready to receive more data
  user_data->read_buffer_.fill(std::byte{0});

  // Ready to receive more data
  error = caller->start_recv(read_span, user_op_data, on_recv_complete);
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_recv() error " << static_cast<int>(error) << std::endl;
    caller->stop_io_context();
    result = -1;
    return;
  }
}

void on_send_to_complete(std::shared_ptr<firelink::Socket> caller, std::span<std::byte> user_buffer,
                         std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                         std::int32_t bytes_transferred, firelink::WriteTag tag)
{
}

void on_disconnect_complete(std::shared_ptr<firelink::Socket> caller,
                            std::shared_ptr<void> user_op_data, firelink::ErrorCode error,
                            firelink::DisconnectTag tag)
{
}

static int process_args(int argc, char** argv, firelink::Endpoint& listener_ep)
{
  // Assign the default addr + port to the endpoint.
  firelink::AddressFamily family = firelink::str_to_family(server_addr_str);
  if (firelink::inet_pton(family, server_addr_str, listener_ep) != firelink::ErrorCode::Success)
  {
    return -1;
  }

  for (int i = 1; i < argc; ++i)
  {
    // -h, --help
    if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0)
    {
      std::cout << "usage: simple_async_tcp_server OPTIONAL: [-s, --server] <127.0.0.1:63000> "
                   "/ <[::1]:63000>"
                << std::endl;
      return 1;
    }
    
    if (i < argc - 1)
    {
      // -s, --server
      if (std::strcmp(argv[i], "-s") == 0 || std::strcmp(argv[i], "--server") == 0)
      {
        family = firelink::str_to_family(argv[i + 1]);
        firelink::ErrorCode err = firelink::inet_pton(family, argv[i + 1], listener_ep);
        if (err != firelink::ErrorCode::Success)
        {
          std::cerr << "firelink::inet_pton error " << static_cast<int>(err) << std::endl;
          return -1;
        }
      }
    }
  }

  return 0;
}

int main(int argc, char** argv)
{
  // Process cmd line arguments
  firelink::Endpoint listener_endpoint{};
  int res = process_args(argc, argv, listener_endpoint);
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
  auto listener_pending = firelink::Socket::create(io_core);
  if (!listener_pending.has_value())
  {
    std::cerr << "firelink::Socket::create error " << static_cast<int>(listener_pending.error())
              << std::endl;
    return -1;
  }

  /*
      Setup the socket with an address family, socket type, and protocol by calling
      firelink::Socket::socket(). NOTE: After calling firelink::Socket::socket(),
      firelink::Socket::close() should be called when done with the socket
   */
  std::shared_ptr<firelink::Socket> listener = std::move(listener_pending.value());
  error = listener->socket(listener_endpoint.family(), firelink::SocketType::Stream,
                           firelink::Protocol::Tcp);
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::socket error " << static_cast<int>(error) << std::endl;
    return -1;
  }

  // Bind the listener to the endpoint
  error = listener->bind(listener_endpoint);
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::bind error " << static_cast<int>(error) << std::endl;
    listener->close();
    return -1;
  }

  // Listen for incoming connections
  error = listener->listen(backlog);
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::listen error " << static_cast<int>(error) << std::endl;
    listener->close();
    return -1;
  }

  // firelink::Socket::start_accept requires an initialized client socket, so let's prepare one
  auto accept_socket_pending = firelink::Socket::create(io_core);
  if (!accept_socket_pending.has_value())
  {
    std::cerr << "firelink::Socket::create error "
              << static_cast<int>(accept_socket_pending.error()) << std::endl;
    listener->close();
    return -1;
  }

  /*
      Setup the socket with an address family, socket type, and protocol by calling
      firelink::Socket::socket(). NOTE: After calling firelink::Socket::socket(),
      firelink::Socket::close() should be called when done with the socket
   */
  std::shared_ptr<firelink::Socket> accept_socket = std::move(accept_socket_pending.value());
  error = accept_socket->socket(listener_endpoint.family(), firelink::SocketType::Stream,
                                firelink::Protocol::Tcp);
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::socket error " << static_cast<int>(error) << std::endl;
    listener->close();
    return -1;
  }

  /*
      Client socket (accept_socket) is now ready to be passed into firelink::Socket::start_accept.
      Create user defined data and pass it in an async socket callback.
   */
  auto user_data = std::make_shared<UserData>();
  error = listener->start_accept(accept_socket, std::static_pointer_cast<void>(user_data),
                                 on_accept_complete);
  if (error != firelink::ErrorCode::Success)
  {
    std::cerr << "firelink::Socket::start_accept error " << static_cast<int>(error) << std::endl;
    accept_socket->close();
    listener->close();
    return -1;
  }

  std::cout << "waiting for connections on " << firelink::inet_ntop(listener_endpoint) << std::endl;

  /*
      Calling IOCore::run will make the thread go to sleep until IOCore::stop is called
      User can also stop the io core  by calling firelink::Socket::stop_io_context()
   */
  io_core->run();
  accept_socket->close();
  listener->close();
  io_core->release();

  std::cout << "exiting" << std::endl;
  return result;
}
