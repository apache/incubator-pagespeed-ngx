/*
 * Copyright 2016 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
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
