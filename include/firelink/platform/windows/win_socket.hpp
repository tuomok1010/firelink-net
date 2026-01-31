#ifndef WIN_SOCKET_H
#define WIN_SOCKET_H

#include "firelink/socket.hpp"
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <WinSock2.h>
#include <string>
#include <array> 
#include <variant>

static constexpr DWORD ACCEPTEX_BUF_LEN = 512;

namespace firelink
{
  namespace platform
  { 
    struct IOData
    {
      OVERLAPPED overlapped_{};

      std::span<std::byte> user_buffer_;
      std::array<std::byte, ACCEPTEX_BUF_LEN> accept_address_buffer_{};

      SOCKADDR_STORAGE local_win_addr_{};
      SOCKADDR_STORAGE peer_win_addr_{};

      std::shared_ptr<Socket> socket_;
      std::shared_ptr<Socket> accept_socket_;

      std::variant<
        AcceptHandler,
        ConnectHandler,
        ReadHandler,
        WriteHandler,
        DisconnectHandler
        > user_handler_;

      ErrorCode error_code_ = ErrorCode::Success;
      std::int32_t bytes_transferred_ = 0;
    };

    class WinSocket : public Socket, public std::enable_shared_from_this<WinSocket>
    {
      public:
      WinSocket(std::shared_ptr<firelink::IOCore> io_core);
      ~WinSocket() override;
      
      // Synchronous API
      ErrorCode socket(AddressFamily addr_family, SocketType sock_type, Protocol protocol) override;
      ErrorCode bind(const Endpoint& endpoint) override;
      ErrorCode listen(std::int32_t backlog) override;
      ErrorCode shutdown(ShutdownHow how) override;
      ErrorCode close() override;

      ErrorCode set_socket_option(SocketOptionLevel level, SocketOption option,
                                          std::span<const std::byte> value) override;

      ErrorCode get_socket_option(SocketOptionLevel level, SocketOption option,
                                          std::span<std::byte> value,
                                          std::size_t& value_size_out) override;

      ErrorCode get_sock_name(Endpoint& ep) override;
      ErrorCode get_peer_name(Endpoint& ep) override;

      bool is_valid() const override { return socket_ == INVALID_SOCKET ? false : true; }

      ErrorCode accept(std::shared_ptr<firelink::Socket> accept_socket) override;
      ErrorCode connect(const Endpoint& dst) override;
      std::int32_t recv(std::span<std::byte> buffer) override;
      std::int32_t recv_from(std::span<std::byte> buffer, Endpoint& peer) override;
      std::int32_t send(std::span<std::byte> data) override;
      std::int32_t send_to(std::span<std::byte> data, const Endpoint& dst) override;
      ErrorCode disconnect(int timeout_ms) override;

      private:
      static ErrorCode sockaddr_to_endpoint(SOCKADDR_STORAGE& addr, Endpoint& endpoint);
      static ErrorCode endpoint_to_sockaddr(AddressFamily family, const Endpoint& endpoint, SOCKADDR_STORAGE& addr);
      
      static std::string addr_to_str(LPSOCKADDR_STORAGE addr);
      static std::string port_to_str(unsigned short port);
      static ErrorCode addr_and_port_to_str(LPSOCKADDR_STORAGE addr, std::string& addr_str, std::string& port_str);
      static ErrorCode str_to_addr4(std::string_view addr_str, std::string_view port_str, LPSOCKADDR_IN addr);
      static ErrorCode str_to_addr6(firelink::AddressFamily src_addr_family, std::string_view src_addr_str, std::string_view src_port_str, 
                               LPSOCKADDR_IN6 dst_addr);

      public:
      // Asynchronous API
      ErrorCode start_accept(std::shared_ptr<firelink::Socket> accept_socket, AcceptHandler handler = AcceptHandler{}) override;
      ErrorCode start_connect(const Endpoint& dst, ConnectHandler handler = ConnectHandler{}) override;
      ErrorCode start_recv(std::span<std::byte> buffer, ReadHandler handler = ReadHandler{}) override;
      ErrorCode start_recv_from(std::span<std::byte> buffer, ReadHandler handler = ReadHandler{}) override;
      ErrorCode start_send(std::span<std::byte> data, WriteHandler handler = WriteHandler{}) override;
      ErrorCode start_send_to(std::span<std::byte> data, const Endpoint& dst, WriteHandler handler = WriteHandler{}) override;
      ErrorCode start_disconnect(bool reuse_socket, DisconnectHandler handler = DisconnectHandler{}) override;

      static LPFN_ACCEPTEX lpfn_accept_ex_;
      static LPFN_GETACCEPTEXSOCKADDRS lpfn_get_accept_ex_sockaddrs_;
      static LPFN_CONNECTEX lpfn_connect_ex_;
      static LPFN_DISCONNECTEX lpfn_disconnect_ex_;
    
      private:
      static ErrorCode get_acceptex_sockaddrs(PVOID buffer, LPSOCKADDR_STORAGE local_addr, LPSOCKADDR_STORAGE remote_addr,
                                         DWORD local_addr_len, DWORD remote_addr_len);
      static ErrorCode update_accept_socket_context(WinSocket* listen_socket, WinSocket* accept_socket);
      static ErrorCode update_connect_socket_context(WinSocket* connect_socket);

      static VOID CALLBACK socket_io_routine(PTP_CALLBACK_INSTANCE, PVOID context, PVOID overlapped,
                                             ULONG io_result, ULONG_PTR n_bytes_transferred, PTP_IO io);

      static VOID CALLBACK user_callback(PTP_CALLBACK_INSTANCE instance, PVOID context);

      static PTP_WIN32_IO_CALLBACK io_routine_;
      PTP_IO socket_io_handle_;
    };
  }
}
#endif /* WIN_SOCKET_H */
