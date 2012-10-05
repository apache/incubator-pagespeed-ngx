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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_FALLBACK_CACHE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_FALLBACK_CACHE_H_

#include <cstddef>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class MessageHandler;
class SharedString;

// Provides a mechanism to handle small objects with one cache, and large
// objects with another cache.  This is not a write-through cache; the
// objects stored in small_object_cache are not stored in large_object_cache,
// though objects stored in large_object_cache require a flag in
// small_object_cache that guides Get to look in the large one.
class FallbackCache : public CacheInterface {
 public:
  // FallbackCache does not take ownership of either cache that's passed in.
  //
  // The threshold is compared against the key-size + value-size on put.
  FallbackCache(CacheInterface* small_object_cache,
                CacheInterface* large_object_cache,
                int threshold_bytes,
                MessageHandler* handler);
  virtual ~FallbackCache();

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, SharedString* value);
  virtual void Delete(const GoogleString& key);
  virtual void MultiGet(MultiGetRequest* request);
  virtual const char* Name() const { return name_.c_str(); }
  virtual bool IsBlocking() const {
    // We can fulfill our guarantee only if both caches block.
    return (small_object_cache_->IsBlocking() &&
            large_object_cache_->IsBlocking());
  }
  virtual bool IsMachineLocal() const {
    // We can fulfill our guarantee only if both caches are machine local.
    return (small_object_cache_->IsMachineLocal() &&
            large_object_cache_->IsMachineLocal());
  }

 private:
  void DecodeValueMatchingKeyAndCallCallback(
      const GoogleString& key, const char* data, size_t data_len,
      Callback* callback);

  CacheInterface* small_object_cache_;
  CacheInterface* large_object_cache_;
  int threshold_bytes_;
  MessageHandler* message_handler_;
  GoogleString name_;

  DISALLOW_COPY_AND_ASSIGN(FallbackCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_FALLBACK_CACHE_H_
