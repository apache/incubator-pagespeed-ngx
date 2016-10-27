#ifndef PAGESPEED_KERNEL_THREAD_BLOCKING_CALLBACK_H_
#define PAGESPEED_KERNEL_THREAD_BLOCKING_CALLBACK_H_

#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/thread/worker_test_base.h"

namespace net_instaweb {

// Helper that blocks for async cache lookups. Used in tests.
class BlockingCallback : public CacheInterface::Callback {
 public:
  explicit BlockingCallback(ThreadSystem* threads)
      : sync_(threads), result_(CacheInterface::kNotFound) {}

  CacheInterface::KeyState result() const { return result_; }
  GoogleString value() const { return value_; }

  void Block() {
    sync_.Wait();
  }

 protected:
  virtual void Done(CacheInterface::KeyState state) {
    result_ = state;
    CacheInterface::Callback::value().Value().CopyToString(&value_);
    sync_.Notify();
  }

 private:
  WorkerTestBase::SyncPoint sync_;
  CacheInterface::KeyState result_;
  GoogleString value_;
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_THREAD_BLOCKING_CALLBACK_H_
