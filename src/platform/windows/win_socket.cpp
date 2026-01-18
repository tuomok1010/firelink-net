#include "firelink/platform/windows/win_socket.hpp"
#include <WinSock2.h>
#include <guiddef.h>
#include <memory>
#include <minwinbase.h>
#include <mstcpip.h>
#include <string>
#include <iostream>
#include <array>
#include <system_error>
#include <winnt.h>
#include <ws2ipdef.h>

// TODO: per op sockaddrs, firelink Addresses --> all host byte order, edit conversion funcs accordingly
// Implement inet_pton etc.

TP_CALLBACK_ENVIRON firelink::platform::WinSocket::io_threadpool_environ_ = {};
PTP_CLEANUP_GROUP firelink::platform::WinSocket::io_cleanup_group_ = nullptr;
PTP_POOL firelink::platform::WinSocket::io_threadpool_ = nullptr;
PTP_WIN32_IO_CALLBACK firelink::platform::WinSocket::io_routine_ = socket_io_routine;

TP_CALLBACK_ENVIRON firelink::platform::WinSocket::callback_threadpool_environ_ = {};
PTP_CLEANUP_GROUP firelink::platform::WinSocket::callback_cleanup_group_ = nullptr;
PTP_POOL firelink::platform::WinSocket::callback_threadpool_ = nullptr;

LPFN_ACCEPTEX firelink::platform::WinSocket::lpfn_accept_ex_ = nullptr;
LPFN_GETACCEPTEXSOCKADDRS firelink::platform::WinSocket::lpfn_get_accept_ex_sockaddrs_ = nullptr;
LPFN_CONNECTEX firelink::platform::WinSocket::lpfn_connect_ex_ = nullptr;
LPFN_DISCONNECTEX firelink::platform::WinSocket::lpfn_disconnect_ex_ = nullptr;

firelink::platform::WSCK_THREADPOOL_ROLLBACK firelink::platform::WinSocket::io_rollback_ = WSCK_ROLLBACK_NONE;
firelink::platform::WSCK_THREADPOOL_ROLLBACK firelink::platform::WinSocket::callback_rollback_ = WSCK_ROLLBACK_NONE;

firelink::platform::WinSocket::WinSocket() : firelink::Socket(),
                                             socket_io_handle_(nullptr)                                           
{
  socket_ = INVALID_SOCKET;
  addr_family_= AddressFamily::NotSupported;
  sock_type_ = SocketType::NotSupported;
  protocol_ = Protocol::NotSupported;
  is_bound_ = false;
}

firelink::platform::WinSocket::~WinSocket()
{
  if (socket_io_handle_ != nullptr)
  {
    WaitForThreadpoolIoCallbacks(socket_io_handle_, FALSE);
    CloseThreadpoolIo(socket_io_handle_);
  }
}

/*
 * Initializes resources required by the WinSocket class
*/
firelink::ErrorCode firelink::platform::WinSocket::initialize()
{
  ErrorCode res = ErrorCode::Success;

  res = initialize_shared_resources();
  if (res != ErrorCode::Success)
  {
    release_shared_resources();
    return res;
  }

  res = get_extended_socket_functions();
  if (res != ErrorCode::Success)
  {
    release_shared_resources();
    return res;
  }

  return res;
}

/*
 * Releases resources allocated by the firelink::platform::WinSocket class
*/
firelink::ErrorCode firelink::platform::WinSocket::release()
{
  return release_shared_resources();
}

/*
 * Creates a socket of the specified addr_family, sock_type and protocol
 */
firelink::ErrorCode firelink::platform::WinSocket::socket(AddressFamily addr_family, SocketType sock_type, Protocol protocol)
{
  socket_ = WSASocketW(static_cast<int>(addr_family), static_cast<int>(sock_type),
                         static_cast<int>(protocol), nullptr, 0, WSA_FLAG_OVERLAPPED);
  
  if (socket_ == INVALID_SOCKET)
    return static_cast<ErrorCode>(WSAGetLastError());

  socket_io_handle_ = CreateThreadpoolIo(reinterpret_cast<HANDLE>(socket_), io_routine_, nullptr, &io_threadpool_environ_);
  
  if (socket_io_handle_ == nullptr)
  {
    int err = static_cast<int>(GetLastError());
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
   
    return static_cast<ErrorCode>(err);
  }

  this->addr_family_ = addr_family;
  this->sock_type_ = sock_type;
  this->protocol_ = protocol;
  
  return ErrorCode::Success;
}

/*
 * DESCRIPTION:
 * Binds the socket to the given address and port
 */
firelink::ErrorCode firelink::platform::WinSocket::bind(const Endpoint& endpoint)
{
  SOCKADDR_STORAGE local_win_addr{};
  ErrorCode err = endpoint_to_sockaddr(addr_family_, endpoint, local_win_addr);
  if(err != ErrorCode::Success)
    return err;
  
  PSOCKADDR addr_ptr = reinterpret_cast<PSOCKADDR>(&local_win_addr);
  if (::bind(socket_, addr_ptr, sizeof(SOCKADDR_STORAGE)) != 0)
    return static_cast<ErrorCode>(WSAGetLastError());

  is_bound_ = true;
  
  return ErrorCode::Success;
}

/*
 * Puts the socket in a listening state for incoming connections
 */
firelink::ErrorCode firelink::platform::WinSocket::listen(int backlog)
{
  if (::listen(socket_, backlog) != 0)
    return static_cast<ErrorCode>(WSAGetLastError());

  return ErrorCode::Success;
}

/*
 * Shuts down the socket
 */
firelink::ErrorCode firelink::platform::WinSocket::shutdown(ShutdownHow how)
{
  if(::shutdown(socket_, static_cast<int>(how)) != 0)
    return static_cast<ErrorCode>(WSAGetLastError());

  return firelink::ErrorCode::Success;
}

/*
 * Waits for all unfinished async socket operations to complete, closes the threadpool IO, 
 * and closes the socket.
 */
firelink::ErrorCode firelink::platform::WinSocket::close()
{
  ErrorCode err = ErrorCode::Success;
  if (socket_io_handle_ != nullptr)
  {
    WaitForThreadpoolIoCallbacks(socket_io_handle_, FALSE);
    CloseThreadpoolIo(socket_io_handle_);
    socket_io_handle_ = nullptr;
  }

  if (socket_ != INVALID_SOCKET)
  {
    if (closesocket(socket_) == SOCKET_ERROR)
      err = static_cast<ErrorCode>(WSAGetLastError());
  }
  
  socket_ = INVALID_SOCKET;
  addr_family_ = AddressFamily::NotSupported;
  sock_type_ = SocketType::NotSupported;
  protocol_ = Protocol::NotSupported;
  is_bound_ = false;

  return err;
}

/*
 * Accepts an incoming connection. On success, the accept_socket contains the accepted connection.
 * remote_addr_str and dst_port will be filled with the peer address and port of the accepted 
 * connection.
 */
firelink::ErrorCode firelink::platform::WinSocket::accept(std::shared_ptr<firelink::Socket> accept_socket)
{
  ErrorCode err = ErrorCode::Success;

  auto* accept_win_socket = static_cast<firelink::platform::WinSocket*>(accept_socket.get());

  SOCKADDR_STORAGE peer_win_addr{};
  int addr_len = sizeof(peer_win_addr);
  SOCKET s = ::accept(socket_, reinterpret_cast<PSOCKADDR>(&peer_win_addr), &addr_len);
  
  if (s == INVALID_SOCKET)
    return static_cast<ErrorCode>(WSAGetLastError());

  WSAPROTOCOL_INFOW info{};
  int opt_len = sizeof(info);
  if (getsockopt(s, SOL_SOCKET, SO_PROTOCOL_INFO, reinterpret_cast<char*>(&info), &opt_len) != 0)
  {
    err = static_cast<ErrorCode>(WSAGetLastError());
    closesocket(s);
    return err;
  }

  accept_win_socket->addr_family_ = static_cast<AddressFamily>(info.iAddressFamily);
  accept_win_socket->sock_type_ = static_cast<SocketType>(info.iSocketType);
  accept_win_socket->protocol_ = static_cast<Protocol>(info.iProtocol);
  accept_win_socket->socket_ = s;

  // associate the accept socket with the socket IO threadpool
  accept_win_socket->socket_io_handle_ = CreateThreadpoolIo(reinterpret_cast<HANDLE>(accept_win_socket->socket_),
                                                            io_routine_, nullptr, &io_threadpool_environ_);

  if (accept_win_socket->socket_io_handle_ == nullptr)
  {
    err = static_cast<ErrorCode>(static_cast<int>(GetLastError()));
    accept_win_socket->close();
    return err;
  }

  accept_win_socket->is_bound_ = true;
  return err;
}

/*
 * Connects to the given endpoint
 */
firelink::ErrorCode firelink::platform::WinSocket::connect(const Endpoint& dst)
{
  SOCKADDR_STORAGE peer_win_addr{};
  ErrorCode err = endpoint_to_sockaddr(addr_family_, dst, peer_win_addr);
  if(err != ErrorCode::Success)
    return err;

  if (::connect(socket_, reinterpret_cast<PSOCKADDR>(&peer_win_addr), sizeof(peer_win_addr)) != 0)
  {
    err = static_cast<ErrorCode>(WSAGetLastError());
    return err;
  }

  is_bound_ = true;
  return ErrorCode::Success;
}

/*
 * Receives data and stores it in the buffer
 */
std::int32_t firelink::platform::WinSocket::recv(std::span<std::byte> buffer)
{ 
  int bytes_received = ::recv(socket_, reinterpret_cast<char*>(buffer.data()),
                              static_cast<int>(buffer.size()), 0);
  
  if (bytes_received == SOCKET_ERROR)
    return -1;

  return bytes_received;
}

/*
 * Receives data and stores it in the buffer. The sender address and port are stored in the
 * given endpoint.
 */
std::int32_t firelink::platform::WinSocket::recv_from(std::span<std::byte> buffer, Endpoint& peer)
{
  SOCKADDR_STORAGE peer_win_addr{};
  int addr_len = sizeof(peer_win_addr);

  int bytes_received = recvfrom(socket_, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()),
                                0, reinterpret_cast<PSOCKADDR>(&peer_win_addr), &addr_len);
  
  if (bytes_received == SOCKET_ERROR)
    return -1;

  if(sockaddr_to_endpoint(peer_win_addr, peer) != ErrorCode::Success)
    return -1;

  return bytes_received;
}

/*
 * Sends data to the destination that the socket is connected to. 
 */
std::int32_t firelink::platform::WinSocket::send(std::span<std::byte> data)
{ 
  int bytes_sent = ::send(socket_, reinterpret_cast<const char*>(data.data()),
                          static_cast<int>(data.size()), 0);
  
  if (bytes_sent == SOCKET_ERROR)
    return -1;

  return bytes_sent;
}

/*
 * Sends data to the given endpoint
 */
std::int32_t firelink::platform::WinSocket::send_to(std::span<std::byte> data, const Endpoint& dst)
{
  SOCKADDR_STORAGE peer_win_addr{};
  if(endpoint_to_sockaddr(addr_family_, dst, peer_win_addr) != ErrorCode::Success)
    return -1;

  int addr_len = sizeof(peer_win_addr);	
  int bytes_sent = ::sendto(socket_, reinterpret_cast<const char*>(data.data()),
                            static_cast<int>(data.size()), 0, reinterpret_cast<PSOCKADDR>(&peer_win_addr),
                            addr_len);
  
  if (bytes_sent == SOCKET_ERROR)
    return -1;

  is_bound_ = true;
  return bytes_sent;
}

/*
 * Sends a shutdown signal to the peer and waits for any leftover data from the peer.
 * Leftover data is scrapped.
 */
firelink::ErrorCode firelink::platform::WinSocket::disconnect(int timeout_ms)
{
  if (::shutdown(socket_, SD_SEND) != 0)
    return static_cast<ErrorCode>(WSAGetLastError());

  // scrap any incoming left over data in this buffer
  char scrap_buffer[512];
  int scrap_buf_len = sizeof(scrap_buffer);
  ZeroMemory(scrap_buffer, scrap_buf_len);

  // We will use the select() function to implement timeout, it requires FD_SET and TIMEVAL structures
  FD_SET fd_select_set{};
  TIMEVAL time_val{};
  time_val.tv_sec = timeout_ms / 1000;
  time_val.tv_usec = (timeout_ms % 1000) * 1000;

  ULONGLONG operation_start_time = GetTickCount64();
  for (;;)
  {
    ULONGLONG elapsed = GetTickCount64() - operation_start_time;
    if (timeout_ms != 0 && elapsed >= static_cast<ULONGLONG>(timeout_ms))
      return ErrorCode::TimedOut;

    ULONGLONG new_timeout = static_cast<ULONGLONG>(timeout_ms) - elapsed;
    time_val.tv_sec = static_cast<long>((new_timeout / 1000));
    time_val.tv_usec = (new_timeout % 1000) * 1000;

    FD_ZERO(&fd_select_set);

    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
    FD_SET(socket_, &fd_select_set);
    #pragma clang diagnostic pop
    
    int res = select(1, &fd_select_set, nullptr, nullptr, timeout_ms == 0 ? nullptr : &time_val);

    // there is data to be read
    if (res > 0)
    {
      int n_bytes_received = ::recv(socket_, scrap_buffer, scrap_buf_len, 0);
      if (n_bytes_received == SOCKET_ERROR)
      {
        return static_cast<ErrorCode>(WSAGetLastError());
      }
      // peer has sent everything, shutdown is completed
      else if (n_bytes_received == 0)
      {
        return ErrorCode::Success;
      }
    }
    // error
    else if (res == SOCKET_ERROR)
    {
      return static_cast<ErrorCode>(WSAGetLastError());
    }
  }

  // Unreachable
}

/*
 * Begins an asynchronous accept operation. accept_socket is filled with the new connection.
 */
firelink::ErrorCode firelink::platform::WinSocket::start_accept(std::shared_ptr<firelink::Socket> accept_socket, AcceptHandler handler)
{
  // If the user hasn't initialized the socket with Socket(), we will do it for them
  if (!accept_socket->is_valid())
  {
    ErrorCode res = accept_socket->socket(addr_family_, sock_type_, protocol_);
    if (res != ErrorCode::Success)
      return res;
  }

  IOData* io_data = new IOData{};
  io_data->socket_ = shared_from_this();
  io_data->accept_socket_ = std::shared_ptr<Socket>(std::move(accept_socket));
  io_data->user_handler_ = std::move(handler);
  
  StartThreadpoolIo(socket_io_handle_);

  SOCKET accept_sock_handle = static_cast<WinSocket*>(io_data->accept_socket_.get())->socket_;
  DWORD addr_len = sizeof(SOCKADDR_STORAGE) + 16;
  DWORD bytes_received = ULONG_MAX;
  
  if (lpfn_accept_ex_(socket_, accept_sock_handle, static_cast<PVOID>(io_data->accept_address_buffer_.data()), 0, 
                      addr_len, addr_len, &bytes_received, &io_data->overlapped_) != TRUE)
  {
    int error = WSAGetLastError();
    if (error != ERROR_IO_PENDING)
    {
      delete io_data;
      CancelThreadpoolIo(socket_io_handle_);
      return static_cast<ErrorCode>(error);
    }
  }
  // Completed synchronously, no need to do anything
  else if (bytes_received != ULONG_MAX)
  {

  }

  return ErrorCode::Success;
}

/*
 * DESCRIPTION:
 * Begins an asynchronous connect operation to the given address and port
 */
firelink::ErrorCode firelink::platform::WinSocket::start_connect(const Endpoint& dst, ConnectHandler handler)
{
  IOData* io_data = new IOData{};
  io_data->socket_ = shared_from_this();
  io_data->user_handler_ = std::move(handler);
  
  ErrorCode err = endpoint_to_sockaddr(addr_family_, dst, io_data->peer_win_addr_);
  if(err != ErrorCode::Success)
    return err;
	
  // ConnectEx requires a bound socket. Check if socket is bound and if not, then bind it.
  if (io_data->peer_win_addr_.ss_family == AF_INET)
  {
    if(is_bound_ == false)
    {
      err = this->bind(IPv4Address::any());
      if(err != ErrorCode::Success)
        return err;
    }
  }
  else if (io_data->peer_win_addr_.ss_family == AF_INET6)
  {
    if(is_bound_ == false)
    {
      err = this->bind(IPv6Address::any());
      if(err != ErrorCode::Success)
        return err;
    }
  }
  
  StartThreadpoolIo(socket_io_handle_);
  
  DWORD n_bytes_sent = 0;
  if (lpfn_connect_ex_(socket_, reinterpret_cast<PSOCKADDR>(&io_data->peer_win_addr_), sizeof(io_data->peer_win_addr_), nullptr, 0,
                       &n_bytes_sent, &io_data->overlapped_) != TRUE)
  {
    int error = WSAGetLastError();
    if (error != ERROR_IO_PENDING)
    {
      delete io_data;
      CancelThreadpoolIo(socket_io_handle_);
      return static_cast<ErrorCode>(error);
    }
  }
  // Completed synchronously.
  else
  {

  }

  return ErrorCode::Success;
}

/*
 * DESCRIPTION:
 * Begins an asynchronous recv operation. The data is stored in buffer.
 */
firelink::ErrorCode firelink::platform::WinSocket::start_recv(std::span<std::byte> buffer, ReadHandler handler)
{
  IOData* io_data = new IOData{};
  io_data->socket_ = shared_from_this();
  io_data->user_handler_ = std::move(handler);
  io_data->user_buffer_ = buffer;

  WSABUF wsa_buf{};
  wsa_buf.buf = reinterpret_cast<char*>(buffer.data());
  wsa_buf.len = static_cast<ULONG>(buffer.size());
  
  StartThreadpoolIo(socket_io_handle_);

  DWORD flags = 0;
  int result = WSARecv(socket_, &wsa_buf, 1, nullptr, &flags, &io_data->overlapped_, nullptr);
  if (result == SOCKET_ERROR)
  {
    result = WSAGetLastError();
    if (result != ERROR_IO_PENDING)
    {
      delete io_data;
      CancelThreadpoolIo(socket_io_handle_);
      return static_cast<ErrorCode>(result);
    }
  }
  /*
   * Completed synchronously.
   */
  else if (result == 0)
  {

  }

  return ErrorCode::Success;
}

/*
 * Begins an asynchronous recvfrom operation. The data is stored in buffer
 */
firelink::ErrorCode firelink::platform::WinSocket::start_recv_from(std::span<std::byte> buffer, ReadHandler handler)
{
  IOData* io_data = new IOData{};
  io_data->socket_ = shared_from_this();
  io_data->user_handler_ = std::move(handler);
  io_data->user_buffer_ = buffer;

  WSABUF wsa_buf{};
  wsa_buf.buf = reinterpret_cast<char*>(buffer.data());
  wsa_buf.len = static_cast<ULONG>(buffer.size());

  StartThreadpoolIo(socket_io_handle_);

  DWORD flags = 0;
  INT remote_addr_len = sizeof(io_data->peer_win_addr_);
  int res = WSARecvFrom(socket_, &wsa_buf, 1, nullptr, &flags, reinterpret_cast<LPSOCKADDR>(&io_data->peer_win_addr_),
                        &remote_addr_len, &io_data->overlapped_, nullptr);

  if (res == SOCKET_ERROR)
  {
    int error = WSAGetLastError();
    if (error != ERROR_IO_PENDING)
    {
      delete io_data;
      CancelThreadpoolIo(socket_io_handle_);
      return static_cast<ErrorCode>(error);
    }
  }
  /*
   * Completed synchronously.
   */
  else if (res == 0) 
  {

  }

  return ErrorCode::Success;
}

/*
 * Begins an asynchronous send operation.
 */
firelink::ErrorCode firelink::platform::WinSocket::start_send(std::span<std::byte> data, WriteHandler handler)
{
  IOData* io_data = new IOData{};
  io_data->socket_ = shared_from_this();
  io_data->user_handler_ = std::move(handler);
  io_data->user_buffer_ = data;
  
  WSABUF wsa_buf{};
  wsa_buf.buf = reinterpret_cast<char*>(data.data());
  wsa_buf.len = static_cast<ULONG>(data.size());
  
  StartThreadpoolIo(socket_io_handle_);

  DWORD flags = 0;
  int res = WSASend(socket_, &wsa_buf, 1, nullptr, flags, &io_data->overlapped_, nullptr);

  if (res == SOCKET_ERROR)
  {
    int error = WSAGetLastError();
    if (error != ERROR_IO_PENDING)
    {
      delete io_data;
      CancelThreadpoolIo(socket_io_handle_);
      return static_cast<ErrorCode>(error);
    }
  }
  /*
   * Completed synchronously.
   */
  else if (res == 0)
  {

  }

  return ErrorCode::Success;
}

/*
 * Begins an asynchronous sendto operation to the given address and port pointed to by 
 * dst_addr and dst_port.
 */
firelink::ErrorCode firelink::platform::WinSocket::start_send_to(std::span<std::byte> data, const Endpoint& dst, WriteHandler handler)
{
  IOData* io_data = new IOData{};
  io_data->socket_ = shared_from_this();
  io_data->user_handler_ = std::move(handler);
  io_data->user_buffer_ = data;

  ErrorCode error = endpoint_to_sockaddr(addr_family_, dst, io_data->peer_win_addr_);
  if(error != ErrorCode::Success)
    return error;

  WSABUF wsa_buf{};
  wsa_buf.buf = reinterpret_cast<char*>(data.data());
  wsa_buf.len = static_cast<ULONG>(data.size());
  
  StartThreadpoolIo(socket_io_handle_);

  DWORD flags = 0;
  int result = WSASendTo(socket_, &wsa_buf, 1, nullptr, flags, reinterpret_cast<PSOCKADDR>(&io_data->peer_win_addr_),
                      sizeof(io_data->peer_win_addr_), &io_data->overlapped_, nullptr);

  if (result == SOCKET_ERROR)
  {
    result = WSAGetLastError();
    if (result != ERROR_IO_PENDING)
    {
      delete io_data;
      CancelThreadpoolIo(socket_io_handle_);
      return static_cast<ErrorCode>(result);
    }
  }
  /*
   * Completed synchronously.
   */
  else if (result == 0)
  {

  }

  return ErrorCode::Success;
}

/*
 * Begins an asynchronous disconnect operation.
 */
firelink::ErrorCode firelink::platform::WinSocket::start_disconnect(bool reuse_socket, DisconnectHandler handler)
{
  DWORD flags = 0;
  if (reuse_socket == TRUE)
    flags = TF_REUSE_SOCKET;

  IOData* io_data = new IOData{};
  io_data->socket_ = shared_from_this();
  io_data->user_handler_ = std::move(handler);
  
  StartThreadpoolIo(socket_io_handle_);

  if (lpfn_disconnect_ex_(socket_, &io_data->overlapped_, flags, 0) != TRUE)
  {
    int error = WSAGetLastError();
    if (error != ERROR_IO_PENDING)
    {
      delete io_data;
      CancelThreadpoolIo(socket_io_handle_);
      return static_cast<ErrorCode>(error);
    }
  }
  /*
   * Completed synchronously.
   */
  else
  {

  }
  
  return ErrorCode::Success;
}

/*
 * Sets a socket option.
 */
firelink::ErrorCode firelink::platform::WinSocket::set_socket_option(SocketOptionLevel level, SocketOption option,
                                      std::span<const std::byte> value)
{
  if (value.empty()) 
    return ErrorCode::InvalidArgument;
    
  int winsock_level = static_cast<int>(level);
  int winsock_option = static_cast<int>(option);
  
  if(winsock_level == -1 || winsock_option == -1)
    return ErrorCode::InvalidArgument;

  const char* opt_val = reinterpret_cast<const char*>(value.data());
  int opt_len = static_cast<int>(value.size());
  
  if (setsockopt(socket_, winsock_level, winsock_option, opt_val, opt_len) != 0)
    return static_cast<ErrorCode>(WSAGetLastError());

  return ErrorCode::Success;
}

/*
 * Gets a socket option.
 */
firelink::ErrorCode firelink::platform::WinSocket::get_socket_option(SocketOptionLevel level, SocketOption option,
                                          std::span<std::byte> value,
                                          std::size_t& value_size_out)
{
  int winsock_level = static_cast<int>(level);
  int winsock_option = static_cast<int>(option);
  
  if(winsock_level == -1 || winsock_option == -1)
    return static_cast<ErrorCode>(WSAEINVAL);

  char* opt_val = reinterpret_cast<char*>(value.data());
  int opt_len = static_cast<int>(value.size());
  
  if (getsockopt(socket_, winsock_level, winsock_option, opt_val, &opt_len) != 0)
    return static_cast<ErrorCode>(WSAGetLastError());

  value_size_out = static_cast<std::size_t>(opt_len);
  
  return ErrorCode::Success;
}

firelink::ErrorCode firelink::platform::WinSocket::get_sock_name(firelink::Endpoint& ep)
{
  SOCKADDR_STORAGE addr{};
  int name_len = sizeof(addr);
  int result = ::getsockname(socket_, reinterpret_cast<PSOCKADDR>(&addr), &name_len);
  if(result == SOCKET_ERROR)
    return static_cast<ErrorCode>(WSAGetLastError());
   
  return sockaddr_to_endpoint(addr, ep);  
}

firelink::ErrorCode firelink::platform::WinSocket::get_peer_name(firelink::Endpoint& ep)
{
  SOCKADDR_STORAGE addr{};
  int name_len = sizeof(addr);
  int result = ::getpeername(socket_, reinterpret_cast<PSOCKADDR>(&addr), &name_len);
  if(result == SOCKET_ERROR)
    return static_cast<ErrorCode>(WSAGetLastError());
   
  return sockaddr_to_endpoint(addr, ep);  
}

firelink::ErrorCode firelink::platform::WinSocket::get_acceptex_sockaddrs(PVOID buffer, LPSOCKADDR_STORAGE local_addr, LPSOCKADDR_STORAGE remote_addr,
                                                           DWORD local_addr_len, DWORD remote_addr_len)                                                           
{
  LPSOCKADDR local_addr_ptr = nullptr;
  LPSOCKADDR remote_addr_ptr = nullptr;
  int local_addr_len_out = 0;	// filled by get_accept_ex_sockaddrs
  int remote_addr_len_out = 0;	// filled by get_accept_ex_sockaddrs

  lpfn_get_accept_ex_sockaddrs_(buffer, 0, local_addr_len, remote_addr_len, &local_addr_ptr,
                                &local_addr_len_out, &remote_addr_ptr, &remote_addr_len_out);

  if (local_addr_ptr == nullptr || remote_addr_ptr == nullptr)
  {
    return ErrorCode::SystemError;
  }
  else
  {
    CopyMemory(local_addr, local_addr_ptr, local_addr_len_out);
    CopyMemory(remote_addr, remote_addr_ptr, remote_addr_len_out);
  }

  return ErrorCode::Success;
}

/*
 * Updates the properties of the accept_socket to match those of the listen_socket.
 * This function is called after the async Accept operation is completed.
 */
firelink::ErrorCode firelink::platform::WinSocket::update_accept_socket_context(firelink::platform::WinSocket* listen_socket,
                                                                                firelink::platform::WinSocket* accept_socket)
{
  if (setsockopt(accept_socket->socket_, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                 reinterpret_cast<char*>(&listen_socket->socket_), sizeof(listen_socket->socket_)) == SOCKET_ERROR)
  {
    return static_cast<ErrorCode>(WSAGetLastError());
  }

  return ErrorCode::Success;
}

/*
  
 * Enables the previously set properties and options of the connect_socket
 * This function should be called after the async Connect operation is completed.
 */
firelink::ErrorCode firelink::platform::WinSocket::update_connect_socket_context(firelink::platform::WinSocket* connect_socket)
{
  if (setsockopt(connect_socket->socket_, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) == SOCKET_ERROR)
  {
    return static_cast<ErrorCode>(WSAGetLastError());
  }

  return ErrorCode::Success;
}

/*
 * This is the socket IO thread pool work function, that handles the completion of async socket operations
 * such as start_accept, start_send, etc. The completed operations are then forwarded to the callback threadpool
 * that calls the user-defined handlers.
 */
VOID CALLBACK firelink::platform::WinSocket::socket_io_routine(PTP_CALLBACK_INSTANCE instance, PVOID context, PVOID overlapped,
                                                   ULONG io_result, ULONG_PTR n_bytes_transferred, PTP_IO io)
{
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(context);
  UNREFERENCED_PARAMETER(io);

  IOData* io_data = static_cast<IOData*>(overlapped);
  
  if(io_data)
  {
    WinSocket* caller = static_cast<WinSocket*>(io_data->socket_.get());
    io_data->bytes_transferred_ = static_cast<std::int32_t>(n_bytes_transferred);
    io_data->error_code_ = static_cast<ErrorCode>(static_cast<int>(io_result));

    if (std::visit([&io_data, &caller](auto&& h)
    {
      using HandlerType = std::decay_t<decltype(h)>;
      if constexpr (std::is_same_v<HandlerType, AcceptHandler>)
      {
        WinSocket* accept_win_socket = static_cast<WinSocket*>(io_data->accept_socket_.get());
        if(accept_win_socket)
        {
          ErrorCode err = update_accept_socket_context(caller, accept_win_socket);
          if(err != ErrorCode::Success)
          {
            // Let's not overwrite if there is an IO/Socket related error as they may be more useful to the user.
            if(io_data->error_code_ == ErrorCode::Success)
              io_data->error_code_ = err;    
          }
        }
      }
      else if constexpr (std::is_same_v<HandlerType, ConnectHandler>)
      {
        ErrorCode err = update_connect_socket_context(caller);
        if(err != ErrorCode::Success)
        {
          // Let's not overwrite if there is an IO/Socket related error as they may be more useful to the user.
          if(io_data->error_code_ == ErrorCode::Success)
            io_data->error_code_ = err;    
        }
      }

      // Returns true if user has defined a handler callback
      return bool(h);
      
    }, io_data->user_handler_))
    {
      if(TrySubmitThreadpoolCallback(user_callback, io_data, &callback_threadpool_environ_) != TRUE)
      {
        io_data->error_code_ = static_cast<ErrorCode>(GetLastError());
        user_callback(nullptr, io_data);
        delete io_data;        
      }
    }
  }
}

VOID CALLBACK firelink::platform::WinSocket::user_callback(PTP_CALLBACK_INSTANCE instance, PVOID context)
{
  UNREFERENCED_PARAMETER(instance);
  IOData* io_data = static_cast<IOData*>(context);
  
  std::visit([&](auto&& handler)
  {
    if (handler)
    {
      using T = std::decay_t<decltype(handler)>;

      if constexpr (std::is_same_v<T, AcceptHandler>)
      {
        ErrorCode err = get_acceptex_sockaddrs(io_data->accept_address_buffer_.data(), &io_data->local_win_addr_, &io_data->peer_win_addr_,
                                               sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16);
        if(err != ErrorCode::Success)
        {
          // Let's not overwrite if there is an IO/Socket related error as they may be more useful to the user.
          if(io_data->error_code_ == ErrorCode::Success)
            io_data->error_code_ = err; 
        }
        
        Endpoint local_ep{};
        Endpoint peer_ep{};
        
        err = sockaddr_to_endpoint(io_data->local_win_addr_, local_ep);
        if(err != ErrorCode::Success)
        {
          if(io_data->error_code_ == ErrorCode::Success)
            io_data->error_code_ = err;
        }

        err = sockaddr_to_endpoint(io_data->peer_win_addr_, peer_ep);
        if(err != ErrorCode::Success)
        {
          if(io_data->error_code_ == ErrorCode::Success)
            io_data->error_code_ = err;
        }
        
        handler(io_data->socket_, std::move(io_data->accept_socket_), local_ep, peer_ep, io_data->error_code_,  AcceptTag{});
      }
      else if constexpr (std::is_same_v<T, ConnectHandler>)
      {
        int name_len = sizeof(io_data->local_win_addr_); 
        int result = ::getsockname((static_cast<WinSocket*>(io_data->socket_.get()))->socket_,
                                   reinterpret_cast<PSOCKADDR>(&io_data->local_win_addr_),
                                   &name_len);
        
        if(result == SOCKET_ERROR)
        {
          if(io_data->error_code_ == ErrorCode::Success)
            io_data->error_code_ = static_cast<ErrorCode>(WSAGetLastError());
        }

        name_len = sizeof(io_data->peer_win_addr_);
        result = ::getpeername((static_cast<WinSocket*>(io_data->socket_.get()))->socket_,
                               reinterpret_cast<PSOCKADDR>(&io_data->peer_win_addr_),
                               &name_len);
        
        if(result == SOCKET_ERROR)
        {
          if(io_data->error_code_ == ErrorCode::Success)
            io_data->error_code_ = static_cast<ErrorCode>(WSAGetLastError());
        }

        Endpoint local_ep{};
        Endpoint peer_ep{};
  
        ErrorCode err = sockaddr_to_endpoint(io_data->local_win_addr_, local_ep);
        if(err != ErrorCode::Success)
        {
          if(io_data->error_code_ == ErrorCode::Success)
            io_data->error_code_ = err;
        }

        err = sockaddr_to_endpoint(io_data->peer_win_addr_, peer_ep);
        if(err != ErrorCode::Success)
        {
          if(io_data->error_code_ == ErrorCode::Success)
            io_data->error_code_ = err;
        }
        
        handler(io_data->socket_, local_ep, peer_ep, io_data->error_code_, ConnectTag{});
      }
      else if constexpr (std::is_same_v<T, ReadHandler>)
      { 
        handler(io_data->socket_, io_data->error_code_, io_data->bytes_transferred_, ReadTag{});
      }
      else if constexpr (std::is_same_v<T, WriteHandler>)
      { 
        handler(io_data->socket_, io_data->error_code_, io_data->bytes_transferred_, WriteTag{});
      }
      else if constexpr (std::is_same_v<T, DisconnectHandler>)

      {
        int name_len = sizeof(io_data->local_win_addr_); 
        int result = ::getsockname((static_cast<WinSocket*>(io_data->socket_.get()))->socket_,
                                   reinterpret_cast<PSOCKADDR>(&io_data->local_win_addr_),
                                   &name_len);

        if(result == SOCKET_ERROR)
        {
          if(io_data->error_code_ == ErrorCode::Success)
            io_data->error_code_ = static_cast<ErrorCode>(WSAGetLastError());
        }

        name_len = sizeof(io_data->peer_win_addr_);
        result = ::getpeername((static_cast<WinSocket*>(io_data->socket_.get()))->socket_,
                               reinterpret_cast<PSOCKADDR>(&io_data->peer_win_addr_),
                               &name_len);

        if(result == SOCKET_ERROR)
        {
          if(io_data->error_code_ == ErrorCode::Success)
            io_data->error_code_ = static_cast<ErrorCode>(WSAGetLastError());
        }

        Endpoint local_ep{};
        Endpoint peer_ep{};
  
        ErrorCode err = sockaddr_to_endpoint(io_data->local_win_addr_, local_ep);
        if(err != ErrorCode::Success)
        {
          if(io_data->error_code_ == ErrorCode::Success)
            io_data->error_code_ = err;
        }

        err = sockaddr_to_endpoint(io_data->peer_win_addr_, peer_ep);
        if(err != ErrorCode::Success)
        {
          if(io_data->error_code_ == ErrorCode::Success)
            io_data->error_code_ = err;
        }
        
        handler(io_data->socket_, local_ep, peer_ep, io_data->error_code_, DisconnectTag{});
      }
    }
  }, io_data->user_handler_);

  delete io_data;
}

/*
 * Creates threadpools and the related resources. Initializes winsock.
 * If this call fails, partially initialized resources may exist.
 * In that case release_shared_resources should be called. 
 */
firelink::ErrorCode firelink::platform::WinSocket::initialize_shared_resources()
{
  WSADATA wsa_data{};
  WORD winsock_version_requested = MAKEWORD(WSCK_SUPPORTED_WINSOCK_MAJOR_VERSION,
                                          WSCK_SUPPORTED_WINSOCK_MINOR_VERSION);

  int res = WSAStartup(winsock_version_requested, &wsa_data);
  if (res != 0)
  {
    return static_cast<ErrorCode>(res);
  }
  else if (wsa_data.wVersion != winsock_version_requested)
  {
    return ErrorCode::NotSupported;
  }

  if (wsa_data.wHighVersion != wsa_data.wVersion)
  {
    // Newer winsock version available
  }

  ErrorCode result = initialize_threadpool(IO_THREADPOOL_MIN_THREADS, IO_THREADPOOL_MAX_THREADS,
                                           &io_rollback_, &io_threadpool_environ_, &io_cleanup_group_, &io_threadpool_);
  
  if (result != ErrorCode::Success)
    return result;

  result = initialize_threadpool(CALLBACK_THREADPOOL_MIN_THREADS, CALLBACK_THREADPOOL_MAX_THREADS,
                                 &callback_rollback_, &callback_threadpool_environ_, &callback_cleanup_group_, &callback_threadpool_);
  
  if (result != ErrorCode::Success)
    return result;

  return ErrorCode::Success;
}
// TODO: take pointer to a pointer as param, otherwise null errors on releasing. <-- what..? 
firelink::ErrorCode firelink::platform::WinSocket::initialize_threadpool(DWORD threads_min, DWORD threads_max, WSCK_THREADPOOL_ROLLBACK* rollback, 
                                              PTP_CALLBACK_ENVIRON environment, PTP_CLEANUP_GROUP* cleanup_group, PTP_POOL* threadpool)
{
  InitializeThreadpoolEnvironment(environment);
  *rollback = WSCK_ROLLBACK_INIT_ENVIRON;
  *threadpool = CreateThreadpool(nullptr);
  if (*threadpool != nullptr)
  {
    *rollback = WSCK_ROLLBACK_CREATE_THREADPOOL;
    SetThreadpoolThreadMaximum(*threadpool, threads_max);

    BOOL res = SetThreadpoolThreadMinimum(*threadpool, threads_min);

    if (res != FALSE)
    {
      *cleanup_group = CreateThreadpoolCleanupGroup();
      if (*cleanup_group != nullptr)
      {
        *rollback = WSCK_ROLLBACK_CREATE_CLEANUP_GROUP;
        SetThreadpoolCallbackPool(environment, *threadpool);
        SetThreadpoolCallbackCleanupGroup(environment, *cleanup_group, nullptr);
      }
      else
      {
        return static_cast<ErrorCode>(static_cast<int>(GetLastError()));
      }
    }
    else
    {
      return static_cast<ErrorCode>(static_cast<int>(GetLastError()));
    }
  }
  else
  {
    return static_cast<ErrorCode>(static_cast<int>(GetLastError()));
  }

  return ErrorCode::Success;
}

/*
 * Releases both threadpool resources and cleans up winsock.
 */
firelink::ErrorCode firelink::platform::WinSocket::release_shared_resources()
{
  ErrorCode result = release_threadpool(CALLBACK_THREADPOOL_CLEANUP_TIMEOUT_MS, &callback_rollback_,
                                        &callback_threadpool_environ_, callback_cleanup_group_, callback_threadpool_);
 
  if (result != ErrorCode::Success)
    return result;

  result = release_threadpool(IO_THREADPOOL_CLEANUP_TIMEOUT_MS, &io_rollback_,
                              &io_threadpool_environ_, io_cleanup_group_, io_threadpool_);
  
  if (result != ErrorCode::Success)
    return result;

  if (WSACleanup() == SOCKET_ERROR)
    return static_cast<ErrorCode>(WSAGetLastError());

  return result;
}

/*
 * Releases all threadpool related resources.
 */
firelink::ErrorCode firelink::platform::WinSocket::release_threadpool(DWORD close_cleanup_members_timeout_ms, WSCK_THREADPOOL_ROLLBACK* rollback,
                                           PTP_CALLBACK_ENVIRON environment, PTP_CLEANUP_GROUP cleanup_group, PTP_POOL threadpool)
{
  ErrorCode return_val = ErrorCode::Success;

  // Ignore missing default case warning, not needed
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wswitch-default"
  switch (*rollback)
  {
    case WSCK_ROLLBACK_CREATE_CLEANUP_GROUP:
    {
      /*
       * When releasing gracefully, we need to create a thread for calling CloseThreadpoolCleanupGroupMembers,
       * because we need the ability to add a timeout to the cleanup process. It is possible that the
       * CloseThreadpoolCleanupGroupMembers function blocks if the threadpool has a thread in a blocking
       * state (for example calling fgets function in a threadpool thread would cause this).
       */
      HANDLE cleanup_thread = CreateThread(nullptr, 0,
                                          [](LPVOID param) -> DWORD
                                          {
                                            PTP_CLEANUP_GROUP grp = static_cast<PTP_CLEANUP_GROUP>(param);
                                            CloseThreadpoolCleanupGroupMembers(grp, TRUE, nullptr);
                                            return 0;

                                          }, cleanup_group, 0, nullptr);

      if (cleanup_thread != nullptr)
      {
        DWORD res = WaitForSingleObject(cleanup_thread, close_cleanup_members_timeout_ms);
        CloseHandle(cleanup_thread);

        // success, lets not do anything
        if (res == WAIT_OBJECT_0) {}
        // timeout occurred, is user using blocking functions?
        else if (res == WAIT_TIMEOUT)
        {

        }
        // error occurred
        else if (res == WAIT_FAILED)
        {
          return_val = static_cast<ErrorCode>(static_cast<int>(GetLastError()));
        }
        // this should not happen
        else
        {
          return_val = ErrorCode::SystemError;
        }
      }
      else
      {
        return_val = static_cast<ErrorCode>(static_cast<int>(GetLastError()));
      }

      CloseThreadpoolCleanupGroup(cleanup_group);
      cleanup_group = nullptr;
      *rollback = WSCK_ROLLBACK_CREATE_THREADPOOL;

      [[fallthrough]];
    }
    case WSCK_ROLLBACK_CREATE_THREADPOOL:
    {
      CloseThreadpool(threadpool);
      threadpool = nullptr;
      *rollback = WSCK_ROLLBACK_INIT_ENVIRON;

      [[fallthrough]];
    }
    case WSCK_ROLLBACK_INIT_ENVIRON:
    {
      DestroyThreadpoolEnvironment(environment);
      *rollback = WSCK_ROLLBACK_NONE;

      [[fallthrough]];
    }
    case WSCK_ROLLBACK_NONE:
    {

    }
  }
  #pragma clang diagnostic pop
  
  *rollback = WSCK_ROLLBACK_NONE;
  return return_val;
}

/*
 * Gets the pointers to the extended socket functions using WSAIoctl call if they are nullptr.
 */
firelink::ErrorCode firelink::platform::WinSocket::get_extended_socket_functions()
{
  // create a dummy socket
  SOCKET dummy_socket = WSASocketW(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
  if (dummy_socket == INVALID_SOCKET)
  {
    return static_cast<ErrorCode>(WSAGetLastError());
  }

  GUID guid;
  DWORD dw_bytes_returned;

  // retrieve pointer to the AcceptEx function
  if (lpfn_accept_ex_ == nullptr)
  {
    guid = WSAID_ACCEPTEX;
    dw_bytes_returned = 0;
    if (WSAIoctl(dummy_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                 &lpfn_accept_ex_, sizeof(lpfn_accept_ex_), &dw_bytes_returned, nullptr, nullptr) != 0)
    {
      ErrorCode result = static_cast<ErrorCode>(WSAGetLastError());
      closesocket(dummy_socket);
      return result;
    }
  }

  // retrieve pointer to the GetAcceptExSockaddrs function
  if (lpfn_get_accept_ex_sockaddrs_ == nullptr)
  {
    guid = WSAID_GETACCEPTEXSOCKADDRS;
    dw_bytes_returned = 0;
    if (WSAIoctl(dummy_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                 &lpfn_get_accept_ex_sockaddrs_, sizeof(lpfn_get_accept_ex_sockaddrs_), &dw_bytes_returned, nullptr, nullptr) != 0)
    {
      ErrorCode result = static_cast<ErrorCode>(WSAGetLastError());
      closesocket(dummy_socket);
      return result;
    }
  }

  // retrieve pointer to the ConnectEx function
  if (lpfn_connect_ex_ == nullptr)
  {
    guid = WSAID_CONNECTEX;
    dw_bytes_returned = 0;
    if (WSAIoctl(dummy_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                 &lpfn_connect_ex_, sizeof(lpfn_connect_ex_), &dw_bytes_returned, nullptr, nullptr) != 0)
    {
      ErrorCode result = static_cast<ErrorCode>(WSAGetLastError());
      closesocket(dummy_socket);
      return result;
    }
  }

  // retrieve pointer to the DisconnectEx function
  if (lpfn_disconnect_ex_ == nullptr)
  {
    guid = WSAID_DISCONNECTEX;
    dw_bytes_returned = 0;
    if (WSAIoctl(dummy_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                 &lpfn_disconnect_ex_, sizeof(lpfn_disconnect_ex_), &dw_bytes_returned, nullptr, nullptr) != 0)
    {
      ErrorCode result = static_cast<ErrorCode>(WSAGetLastError());
      closesocket(dummy_socket);
      return result;
    }
  }

  if (closesocket(dummy_socket) != 0)
  {
    return static_cast<ErrorCode>(WSAGetLastError());
  }

  return ErrorCode::Success;
}

/*
 * A helper that converts a SOCKADDR_STORAGE into a firelink::Endpoint
 */
firelink::ErrorCode firelink::platform::WinSocket::sockaddr_to_endpoint(SOCKADDR_STORAGE& addr,
                                                                        Endpoint& endpoint)
{
  PSOCKADDR addr_ptr = reinterpret_cast<PSOCKADDR>(&addr);
  if(addr.ss_family == AF_INET)
  {
    PSOCKADDR_IN addr4_ptr = reinterpret_cast<PSOCKADDR_IN>(addr_ptr);
    
    IPv4Address ipv4_addr{};
    ipv4_addr.port = ntohs(addr4_ptr->sin_port);

    std::int32_t* sin_addr_ptr = reinterpret_cast<std::int32_t*>(&addr4_ptr->sin_addr);
    ipv4_addr.bytes[3] = (*sin_addr_ptr >> 24) & 0XFF;
    ipv4_addr.bytes[2] = (*sin_addr_ptr >> 16) & 0XFF;
    ipv4_addr.bytes[1] = (*sin_addr_ptr >> 8)  & 0XFF;
    ipv4_addr.bytes[0] = (*sin_addr_ptr >> 0)  & 0XFF;
    
    endpoint = Endpoint(ipv4_addr);
  }
  else if(addr.ss_family == AF_INET6)
  {
    PSOCKADDR_IN6 addr6_ptr = reinterpret_cast<PSOCKADDR_IN6>(addr_ptr);

    IPv6Address ipv6_addr{};
    ipv6_addr.port = ntohs(addr6_ptr->sin6_port);
    CopyMemory(ipv6_addr.bytes.data(), &addr6_ptr->sin6_addr, 16);
    endpoint = Endpoint(ipv6_addr);
  }
  else
  {
    return ErrorCode::AddressFamilyNotSupported;
  }

  return ErrorCode::Success;
}

/*
 * A helper that converts a firelink::Endpoint into a SOCKADDR_STORAGE
 */
firelink::ErrorCode firelink::platform::WinSocket::endpoint_to_sockaddr(AddressFamily family,
                                                                        const Endpoint& endpoint,
                                                                        SOCKADDR_STORAGE& addr)
{
  ZeroMemory(&addr, sizeof(addr));
  PSOCKADDR addr_ptr = reinterpret_cast<PSOCKADDR>(&addr);
  
  if(family == AddressFamily::IPv4)
  {
    PSOCKADDR_IN addr4_ptr = reinterpret_cast<PSOCKADDR_IN>(addr_ptr);
    addr4_ptr->sin_family = static_cast<ADDRESS_FAMILY>(static_cast<int>(family));
    addr4_ptr->sin_port = htons(endpoint.ipv4().port);
    CopyMemory(&addr4_ptr->sin_addr.s_addr, endpoint.ipv4().bytes.data(), 4);
  }
  else if(family == AddressFamily::IPv6)
  {
    PSOCKADDR_IN6 addr6_ptr = reinterpret_cast<PSOCKADDR_IN6>(addr_ptr);
    addr6_ptr->sin6_family = static_cast<ADDRESS_FAMILY>(static_cast<int>(family));
    CopyMemory(&addr6_ptr->sin6_addr, endpoint.ipv6().bytes.data(), 16);
    addr6_ptr->sin6_port = htons(endpoint.ipv6().port);
  }
  else
  {
    return ErrorCode::AddressFamilyNotSupported;
  }

  return ErrorCode::Success;
}


/*
 * Converts the LPSOCKADDR_STORAGE addr into a string
 */
std::string firelink::platform::WinSocket::addr_to_str(LPSOCKADDR_STORAGE addr)
{
  if (addr == nullptr)
  {
    return {};
  }

  std::array<char, INET6_ADDRSTRLEN> buffer{};
  const char* res = nullptr;

  if (addr->ss_family == AF_INET)
  {
    res = InetNtopA(AF_INET, &(reinterpret_cast<LPSOCKADDR_IN>(addr))->sin_addr, buffer.data(), buffer.size());
  }
  else if (addr->ss_family == AF_INET6)
 {
    res = InetNtopA(AF_INET6, &(reinterpret_cast<LPSOCKADDR_IN6>(addr))->sin6_addr, buffer.data(), buffer.size());
  }
  else
  {
    return {};
  }

  if(res == nullptr)
  {
    return {};
  }

  return std::string(res);
}

std::string firelink::platform::WinSocket::port_to_str(unsigned short port)
{
  std::array<char, 10> port_buffer{};
  if (sprintf_s(port_buffer.data(), port_buffer.size(), "%u", ntohs(port)) < 0)
  {
    return {};
  }

  return std::string(port_buffer.data());
}

/*
 * Converts the LPSOCKADDR_STORAGE addr into a string pointed to by addr_str and
 * the port contained within addr into a string pointed to by port_str
 */
firelink::ErrorCode firelink::platform::WinSocket::addr_and_port_to_str(LPSOCKADDR_STORAGE addr, std::string& addr_str, std::string& port_str)
{
  if (addr == nullptr)
    return ErrorCode::InvalidArgument;

  std::array<char, INET6_ADDRSTRLEN> addr_buffer{};
  std::array<char, 10> port_buffer{};

  if (addr->ss_family == AF_INET)
  {
    if (InetNtopA(AF_INET, &(reinterpret_cast<LPSOCKADDR_IN>(addr))->sin_addr, addr_buffer.data(), addr_buffer.size()) == nullptr)
    {
      return static_cast<ErrorCode>(WSAGetLastError());
    }

    if (sprintf_s(port_buffer.data(), port_buffer.size(), "%u", ntohs((reinterpret_cast<LPSOCKADDR_IN>(addr))->sin_port)) < 0)
    {
      return ErrorCode::SystemError;
    }		
  }
  else if (addr->ss_family == AF_INET6)
  {
    if (InetNtopA(AF_INET6, &(reinterpret_cast<LPSOCKADDR_IN6>(addr))->sin6_addr, addr_buffer.data(), addr_buffer.size()) == nullptr)
    {
      return static_cast<ErrorCode>(WSAGetLastError());
    }

    if (sprintf_s(port_buffer.data(), port_buffer.size(), "%u", ntohs((reinterpret_cast<LPSOCKADDR_IN6>(addr))->sin6_port)) < 0)
    {
      return ErrorCode::SystemError;
    }
  }
  else
  {
    return ErrorCode::AddressFamilyNotSupported;
  }

  addr_str = std::string(addr_buffer.data());
  port_str = std::string(port_buffer.data());
  
  return ErrorCode::Success;
}

/*
 * Converts the addr_str and port_str into a SOCKADDR_IN address pointed to by LPSOCKADDR_IN addr.
 */
firelink::ErrorCode firelink::platform::WinSocket::str_to_addr4(std::string_view addr_str, std::string_view port_str, LPSOCKADDR_IN addr)
{
  if (addr == nullptr)
    return ErrorCode::InvalidArgument;

  int res = InetPtonA(AF_INET, addr_str.data(), &addr->sin_addr);

  // success
  if (res == 1)
  {
    long int port64 = strtol(port_str.data(), nullptr, 10);
    if (port64 > USHRT_MAX || port64 < 0)
    {
      return ErrorCode::InvalidArgument;
    }
    else if (port64 == 0 && strcmp(port_str.data(), "0") != 0)
    {
      return ErrorCode::SystemError;
    }

    addr->sin_port = htons(static_cast<u_short>(port64));
    addr->sin_family = AF_INET;
    return ErrorCode::Success;
  }
  // not a valid IPv4 dotted - decimal string
  else if (res == 0)
  {
    return ErrorCode::InvalidArgument;
  }
  // other error occurred
  else
  {
    return static_cast<ErrorCode>(WSAGetLastError());
  }

  // Unreachable
}

/*
 * Converts the src_addr_str and  src_port_str into a SOCKADDR_IN6 address pointed to by 
 * LPSOCKADDR_IN6 dstAddr.
 */
firelink::ErrorCode firelink::platform::WinSocket::str_to_addr6(firelink::AddressFamily src_addr_family, std::string_view src_addr_str, std::string_view src_port_str, 
                                                 LPSOCKADDR_IN6 dst_addr)
{
  if (dst_addr == nullptr)
    return ErrorCode::InvalidArgument;

  // user wants to convert from IPv4 to IPv6
  if (src_addr_family == firelink::AddressFamily::IPv4)
  {
    // convert IPv4 address from string to SOCKADDR_IN
    SOCKADDR_IN tempIPv4Addr;
    ZeroMemory(&tempIPv4Addr, sizeof(tempIPv4Addr));
    
    ErrorCode err = str_to_addr4(src_addr_str, src_port_str, &tempIPv4Addr);
    if (err != ErrorCode::Success)
      return err;

    // map the IPv4 address into an IPv4-mapped IPv6 address
    IN6ADDR_SETV4MAPPED(dst_addr, &tempIPv4Addr.sin_addr, INETADDR_SCOPE_ID(reinterpret_cast<PSOCKADDR>(&tempIPv4Addr)),
			tempIPv4Addr.sin_port);

    return ErrorCode::Success;
  }
  // this is a regular IPv6 string to addr conversion
  else
  {
    int res = InetPtonA(AF_INET6, src_addr_str.data(), &dst_addr->sin6_addr);

    // success
    if (res == 1)
    {
      long int port64 = strtol(src_port_str.data(), nullptr, 10);
      if (port64 > USHRT_MAX || port64 < 0)
      {
        return ErrorCode::InvalidArgument;
      }
      else if (port64 == 0 && strcmp(src_port_str.data(), "0") != 0)
      {
        return ErrorCode::SystemError;
      }

      dst_addr->sin6_port = htons(static_cast<u_short>(port64));
      dst_addr->sin6_family = AF_INET6;
      return ErrorCode::Success;
    }
    // not a valid IPv6 string
    else if (res == 0)
    {
      return ErrorCode::InvalidArgument;
    }
    // other error occurred
    else
    {
      return ErrorCode::SystemError;
    }
  }

  // Unreachable
}
