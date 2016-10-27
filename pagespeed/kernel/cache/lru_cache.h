/*
 * Copyright 2010 Google Inc.
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

#ifndef PAGESPEED_KERNEL_CACHE_LRU_CACHE_H_
#define PAGESPEED_KERNEL_CACHE_LRU_CACHE_H_

#include <cstddef>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/cache/lru_cache_base.h"

namespace net_instaweb {

// Simple C++ implementation of an in-memory least-recently used (LRU)
// cache.  This implementation is not thread-safe, and must be
// combined with a mutex to make it so.
//
// The purpose of this implementation is as a default implementation,
// or an local shadow for memcached.
//
// Also of note: the Get interface allows for streaming.  To get into
// a GoogleString, use a StringWriter.
//
// TODO(jmarantz): The Put interface does not currently stream, but this
// should be added.
class LRUCache : public CacheInterface {
 public:
  explicit LRUCache(size_t max_size);
  virtual ~LRUCache();

  virtual void Get(const GoogleString& key, Callback* callback);

  // Puts an object into the cache, sharing the bytes.
  //
  // TODO(jmarantz): currently if the caller mutates the
  // SharedString after having called Put, it will actually
  // modify the value in the cache.  We should change
  // SharedString to Copy-On-Write semantics.
  virtual void Put(const GoogleString& key, const SharedString& new_value);
  virtual void Delete(const GoogleString& key);

  // Deletes all objects whose key starts with prefix.
  // Not part of cache interface. Exported for testing only.
  void DeleteWithPrefixForTesting(StringPiece prefix);

  // Total size in bytes of keys and values stored.
  size_t size_bytes() const { return base_.size_bytes(); }

  // Maximum capacity.
  size_t max_bytes_in_cache() const { return base_.max_bytes_in_cache(); }

  // Number of elements stored
  size_t num_elements() const { return base_.num_elements(); }

  size_t num_evictions() const { return base_.num_evictions(); }
  size_t num_hits() const { return base_.num_hits(); }
  size_t num_misses() const { return base_.num_misses(); }
  size_t num_inserts() const { return base_.num_inserts(); }
  size_t num_identical_reinserts() const {
    return base_.num_identical_reinserts();
  }
  size_t num_deletes() const { return base_.num_deletes(); }

  // Sanity check the cache data structures.
  void SanityCheck() { base_.SanityCheck(); }

  // Clear the entire cache.  Used primarily for testing.  Note that this
  // will not clear the stats, however it will update current_bytes_in_cache_.
  void Clear() { base_.Clear(); }

  // Clear the stats -- note that this will not clear the content.
  void ClearStats() { base_.ClearStats(); }

  static GoogleString FormatName() { return "LRUCache"; }
  virtual GoogleString Name() const { return FormatName(); }
  virtual bool IsBlocking() const { return true; }
  virtual bool IsHealthy() const { return is_healthy_; }
  virtual void ShutDown() { set_is_healthy(false); }

  void set_is_healthy(bool x) { is_healthy_ = x; }

 private:
  struct SharedStringHelper {
    size_t size(const SharedString& ss) const {
      return ss.size();
    }
    bool Equal(const SharedString& a, const SharedString& b) const {
      return a.Value() == b.Value();
    }
    void EvictNotify(const SharedString& a) {}
    bool ShouldReplace(const SharedString& old_value,
                       const SharedString& new_value) const {
      return true;
    }
  };
  typedef LRUCacheBase<SharedString, SharedStringHelper> Base;

  Base base_;
  bool is_healthy_;
  SharedStringHelper value_helper_;

  DISALLOW_COPY_AND_ASSIGN(LRUCache);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_CACHE_LRU_CACHE_H_
