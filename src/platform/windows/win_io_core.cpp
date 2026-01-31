#include "firelink/platform/windows/win_io_core.hpp"
#include "firelink/platform/windows/win_socket.hpp"
#include <iostream>

firelink::platform::WinIOCore::WinIOCore(const IOCoreConfig& config) :
  conf_(config)
{
  
}

firelink::platform::WinIOCore::~WinIOCore()
{
  
}

firelink::ErrorCode firelink::platform::WinIOCore::initialize()
{
  WSADATA wsa_data{};
  WORD winsock_version_requested = MAKEWORD(FIRELINK_SUPPORTED_WINSOCK_MAJOR_VERSION,
                                            FIRELINK_SUPPORTED_WINSOCK_MINOR_VERSION);

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
  
  ErrorCode err = get_extended_socket_functions();
  if (err != ErrorCode::Success)
  {
    // release_shared_resources();
    return err;
  }

  err = initialize_threadpool(conf_.io_threadpool_min_threads_, conf_.io_threadpool_max_threads_,
                              &io_rollback_, &io_threadpool_environ_, &io_cleanup_group_, &io_threadpool_);
  
  if (err != ErrorCode::Success)
    return err;

  err = initialize_threadpool(conf_.user_threadpool_min_threads_, conf_.user_threadpool_max_threads_,
                              &user_rollback_, &user_threadpool_environ_, &user_cleanup_group_, &user_threadpool_);
  
  if (err != ErrorCode::Success)
    return err;

  return ErrorCode::Success;
}

firelink::ErrorCode firelink::platform::WinIOCore::release()
{
  ErrorCode result = release_threadpool(FIRELINK_USER_THREADPOOL_CLEANUP_TIMEOUT_MS, &user_rollback_,
                                        &user_threadpool_environ_, user_cleanup_group_, user_threadpool_);
 
  if (result != ErrorCode::Success)
    return result;

  result = release_threadpool(FIRELINK_IO_THREADPOOL_CLEANUP_TIMEOUT_MS, &io_rollback_,
                              &io_threadpool_environ_, io_cleanup_group_, io_threadpool_);
  
  if (result != ErrorCode::Success)
    return result;

  if (WSACleanup() == SOCKET_ERROR)
    return static_cast<ErrorCode>(WSAGetLastError());
 
  return ErrorCode::Success;
}

firelink::ErrorCode firelink::platform::WinIOCore::post_io_work(std::move_only_function<void()>&& func)
{
  auto* heap_func = new std::move_only_function<void()>(std::move(func));

  BOOL success = TrySubmitThreadpoolCallback(
    [](PTP_CALLBACK_INSTANCE instance, PVOID context) noexcept
    {
      UNREFERENCED_PARAMETER(instance);
      auto* f = static_cast<std::move_only_function<void()>*>(context);
      std::invoke(*f);
      delete f;
      
    }, heap_func, &io_threadpool_environ_
    );

  if (!success)
  {
    // Very important: clean up on failure!
    delete heap_func;
    return static_cast<ErrorCode>(GetLastError());
  }

  return ErrorCode::Success;
}

firelink::ErrorCode firelink::platform::WinIOCore::post_user_work(std::move_only_function<void()>&& func)
{
  auto* heap_func = new std::move_only_function<void()>(std::move(func));

  BOOL success = TrySubmitThreadpoolCallback(
    [](PTP_CALLBACK_INSTANCE instance, PVOID context) noexcept
    {
      UNREFERENCED_PARAMETER(instance);
      auto* f = static_cast<std::move_only_function<void()>*>(context);
      std::invoke(*f);
      delete f;
      
    }, heap_func, &user_threadpool_environ_
    );

  if (!success)
  {
    // Very important: clean up on failure!
    delete heap_func;
    return static_cast<ErrorCode>(GetLastError());
  }

  return ErrorCode::Success;
}

void firelink::platform::WinIOCore::run()
{
  
}

void firelink::platform::WinIOCore::stop()
{
  
}

PTP_IO firelink::platform::WinIOCore::associate_handle(NativeHandle handle, PTP_WIN32_IO_CALLBACK io_routine)
{
  return CreateThreadpoolIo(reinterpret_cast<HANDLE>(handle), io_routine, nullptr, &io_threadpool_environ_);
}

firelink::ErrorCode firelink::platform::WinIOCore::initialize_threadpool(DWORD threads_min, DWORD threads_max, ThreadpoolRollback* rollback, 
                                                                         PTP_CALLBACK_ENVIRON environment, PTP_CLEANUP_GROUP* cleanup_group, PTP_POOL* threadpool)
{
  InitializeThreadpoolEnvironment(environment);
  *rollback = ThreadpoolRollback::InitEnviron;
  *threadpool = ::CreateThreadpool(nullptr);
  if (*threadpool != nullptr)
  {
    *rollback = ThreadpoolRollback::CreateThreadpool;
    SetThreadpoolThreadMaximum(*threadpool, threads_max);

    BOOL res = SetThreadpoolThreadMinimum(*threadpool, threads_min);

    if (res != FALSE)
    {
      *cleanup_group = CreateThreadpoolCleanupGroup();
      if (*cleanup_group != nullptr)
      {
        *rollback = ThreadpoolRollback::CreateCleanupGroup;
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

firelink::ErrorCode firelink::platform::WinIOCore::release_threadpool(DWORD close_cleanup_members_timeout_ms, ThreadpoolRollback* rollback,
                                                                      PTP_CALLBACK_ENVIRON environment, PTP_CLEANUP_GROUP cleanup_group, PTP_POOL threadpool)
{
  ErrorCode return_val = ErrorCode::Success;

  // Ignore missing default case warning, not needed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-default"
  switch (*rollback)
  {
    case ThreadpoolRollback::CreateCleanupGroup:
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
      *rollback = ThreadpoolRollback::CreateThreadpool;

      [[fallthrough]];
    }
    case ThreadpoolRollback::CreateThreadpool:
    {
      CloseThreadpool(threadpool);
      threadpool = nullptr;
      *rollback = ThreadpoolRollback::InitEnviron;

      [[fallthrough]];
    }
    case ThreadpoolRollback::InitEnviron:
    {
      DestroyThreadpoolEnvironment(environment);
      *rollback = ThreadpoolRollback::None;

      [[fallthrough]];
    }
    case ThreadpoolRollback::None:
    {

    }
  }
#pragma clang diagnostic pop
  
  *rollback = ThreadpoolRollback::None;
  return return_val;
}

firelink::ErrorCode firelink::platform::WinIOCore::get_extended_socket_functions()
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
  if (WinSocket::lpfn_accept_ex_ == nullptr)
  {
    guid = WSAID_ACCEPTEX;
    dw_bytes_returned = 0;
    if (WSAIoctl(dummy_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                 &WinSocket::lpfn_accept_ex_, sizeof(WinSocket::lpfn_accept_ex_),
                 &dw_bytes_returned, nullptr, nullptr) != 0)
    {
      ErrorCode result = static_cast<ErrorCode>(WSAGetLastError());
      closesocket(dummy_socket);
      return result;
    }
  }

  // retrieve pointer to the GetAcceptExSockaddrs function
  if (WinSocket::lpfn_get_accept_ex_sockaddrs_ == nullptr)
  {
    guid = WSAID_GETACCEPTEXSOCKADDRS;
    dw_bytes_returned = 0;
    if (WSAIoctl(dummy_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                 &WinSocket::lpfn_get_accept_ex_sockaddrs_, sizeof(WinSocket::lpfn_get_accept_ex_sockaddrs_),
                 &dw_bytes_returned, nullptr, nullptr) != 0)
    {
      ErrorCode result = static_cast<ErrorCode>(WSAGetLastError());
      closesocket(dummy_socket);
      return result;
    }
  }

  // retrieve pointer to the ConnectEx function
  if (WinSocket::lpfn_connect_ex_ == nullptr)
  {
    guid = WSAID_CONNECTEX;
    dw_bytes_returned = 0;
    if (WSAIoctl(dummy_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                 &WinSocket::lpfn_connect_ex_, sizeof(WinSocket::lpfn_connect_ex_),
                 &dw_bytes_returned, nullptr, nullptr) != 0)
    {
      ErrorCode result = static_cast<ErrorCode>(WSAGetLastError());
      closesocket(dummy_socket);
      return result;
    }
  }

  // retrieve pointer to the DisconnectEx function
  if (WinSocket::lpfn_disconnect_ex_ == nullptr)
  {
    guid = WSAID_DISCONNECTEX;
    dw_bytes_returned = 0;
    if (WSAIoctl(dummy_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                 &WinSocket::lpfn_disconnect_ex_, sizeof(WinSocket::lpfn_disconnect_ex_),
                 &dw_bytes_returned, nullptr, nullptr) != 0)
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
