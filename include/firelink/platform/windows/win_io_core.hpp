#ifndef WIN_IO_CORE_H
#define WIN_IO_CORE_H

#include "firelink/io_core.hpp"

#include <WinSock2.h>

// version of winsock that firelink supports. winsock is initialized to this version
static constexpr DWORD FIRELINK_SUPPORTED_WINSOCK_MINOR_VERSION = 2;
static constexpr DWORD FIRELINK_SUPPORTED_WINSOCK_MAJOR_VERSION = 2;

namespace firelink
{
  namespace platform
  {
    enum ThreadpoolRollback
    {
      None,
      InitEnviron,
      CreateThreadpool,
      CreateCleanupGroup
    };
    
    class WinIOCore : public IOCore
    {
      public:
      WinIOCore(const IOCoreConfig& config);
      ~WinIOCore() override;
      
      ErrorCode initialize() override;
      ErrorCode release() override;

      void post_io_work(std::move_only_function<void()>&&) override;
      void post_user_work(std::move_only_function<void()>&&) override;

      void run() override;
      void stop() override;

      // Socket association
      ErrorCode associate_handle(std::uint32_t handle) override;

      private:
      static ErrorCode initialize_threadpool(DWORD threads_min, DWORD threads_max, ThreadpoolRollback* rollback, 
                                             PTP_CALLBACK_ENVIRON environment, PTP_CLEANUP_GROUP* cleanup_group, PTP_POOL* threadpool);

      static ErrorCode release_threadpool(DWORD close_cleanup_members_timeout_ms, ThreadpoolRollback* rollback,
                                          PTP_CALLBACK_ENVIRON environment, PTP_CLEANUP_GROUP cleanup_group, PTP_POOL threadpool);

      static ErrorCode get_extended_socket_functions();

      PTP_WIN32_IO_CALLBACK io_routine_;

      TP_CALLBACK_ENVIRON io_threadpool_environ_;
      PTP_CLEANUP_GROUP io_cleanup_group_;
      PTP_POOL io_threadpool_;
      ThreadpoolRollback io_rollback_;

      TP_CALLBACK_ENVIRON user_threadpool_environ_;
      PTP_CLEANUP_GROUP user_cleanup_group_;
      PTP_POOL user_threadpool_;
      ThreadpoolRollback user_rollback_;  
    };
  }
}

#endif /* WIN_IO_CORE_H */
