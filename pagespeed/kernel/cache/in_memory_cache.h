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

// Author: yeputons@google.com (Egor Suvorov)

#ifndef PAGESPEED_KERNEL_CACHE_IN_MEMORY_CACHE_H_
#define PAGESPEED_KERNEL_CACHE_IN_MEMORY_CACHE_H_

#include <unordered_map>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

// Simple in-memory implementation of CacheInterface without automatic purging.
// This implementation is thread-compatible, and must be combined with a mutex
// to make it thread-safe. It also copies all stored keys and values so it's
// safe to change SharedStrings after putting them into the cache (e.g. with
// WriteAt).
//
// The purpose of this implementation is to be fake implementation in tests
// and debugging.
class InMemoryCache : public CacheInterface {
 public:
  InMemoryCache();
  ~InMemoryCache() override {}

  // CacheInterface implementation
  void Get(const GoogleString& key, Callback* callback) override;
  void Put(const GoogleString& key, const SharedString& new_value) override;
  void Delete(const GoogleString& key) override;
  GoogleString Name() const override { return "InMemoryCache"; }
  bool IsBlocking() const override { return true; }
  bool IsHealthy() const override { return !is_shut_down_; }
  void ShutDown() override { is_shut_down_ = true; }

 private:
  std::unordered_map<GoogleString, SharedString> cache_;

  bool is_shut_down_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryCache);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_CACHE_IN_MEMORY_CACHE_H_
