/*
 * Copyright 2012 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef PAGESPEED_KERNEL_CACHE_FALLBACK_CACHE_H_
#define PAGESPEED_KERNEL_CACHE_FALLBACK_CACHE_H_

#include <cstddef>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/cache/cache_interface.h"

namespace net_instaweb {

class MessageHandler;

// Provides a mechanism to handle small objects with one cache, and large
// objects with another cache.  This is not a write-through cache; the
// objects stored in small_object_cache are not stored in large_object_cache,
// though objects stored in large_object_cache require a flag in
// small_object_cache that guides Get to look in the large one.
class FallbackCache : public CacheInterface {
 public:
  // FallbackCache does not take ownership of either cache that's passed in.
  //
  // The threshold is compared against the value-size + key size (default on,
  // disable via set_account_for_key_size) on put. The threshold is inclusive:
  // up to that many bytes will be stored into small_object_cache.
  FallbackCache(CacheInterface* small_object_cache,
                CacheInterface* large_object_cache,
                int threshold_bytes,
                MessageHandler* handler);
  virtual ~FallbackCache();

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, const SharedString& value);
  virtual void Delete(const GoogleString& key);
  virtual void MultiGet(MultiGetRequest* request);
  virtual bool IsBlocking() const {
    // We can fulfill our guarantee only if both caches block.
    return (small_object_cache_->IsBlocking() &&
            large_object_cache_->IsBlocking());
  }

  virtual bool IsHealthy() const {
    return (small_object_cache_->IsHealthy() &&
            large_object_cache_->IsHealthy());
  }

  virtual void ShutDown() {
    small_object_cache_->ShutDown();
    large_object_cache_->ShutDown();
  }

  virtual GoogleString Name() const {
    return FormatName(small_object_cache_->Name(), large_object_cache_->Name());
  }
  static GoogleString FormatName(StringPiece small, StringPiece large);

  // If true (the default) the space for the key will be added to the
  // value size when checking whether a store exceeds threshold_bytes.
  void set_account_for_key_size(bool x) { account_for_key_size_ = x; }

 private:
  void DecodeValueMatchingKeyAndCallCallback(
      const GoogleString& key, const char* data, size_t data_len,
      Callback* callback);

  CacheInterface* small_object_cache_;
  CacheInterface* large_object_cache_;
  int threshold_bytes_;
  bool account_for_key_size_;
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(FallbackCache);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_CACHE_FALLBACK_CACHE_H_
