#ifndef FIRELINK_IO_CORE_H
#define FIRELINK_IO_CORE_H

#include "firelink/export.hpp"
#include "firelink/error_codes.hpp"
#include "types.hpp"

#include <atomic>
#include <memory>
#include <functional>
#include <expected>

namespace firelink
{
  struct IOCoreConfig
  {
    std::uint32_t io_threadpool_min_threads_;
    std::uint32_t io_threadpool_max_threads_;
    std::uint32_t user_threadpool_min_threads_;
    std::uint32_t user_threadpool_max_threads_;
  };
  
  class FIRELINK_CLASS_API IOCore
  {
    public:
    virtual ~IOCore() = default;

    IOCore(const IOCore&) = delete;
    IOCore& operator=(const IOCore&) = delete;

    static std::expected<std::unique_ptr<IOCore>, ErrorCode>
    create(const IOCoreConfig& config = {});

    virtual ErrorCode initialize() = 0;
    virtual ErrorCode release() = 0;

    virtual ErrorCode post_io_work(std::move_only_function<void()>&& func) = 0;
    virtual ErrorCode post_user_work(std::move_only_function<void()>&& func) = 0;

    virtual void run() = 0;
    virtual void stop() = 0;

    // Socket association
    virtual ErrorCode associate_handle(NativeHandle handle) = 0;

    protected:
    IOCore() = default;
  };
}

#endif /* FIRELINK_IO_CORE_H */
