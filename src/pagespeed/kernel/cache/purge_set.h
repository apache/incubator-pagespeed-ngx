/*
 * Copyright 2013 Google Inc.
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_PURGE_SET_H_
#define NET_INSTAWEB_UTIL_PUBLIC_PURGE_SET_H_

#include <algorithm>
#include <cstddef>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/cache/lru_cache_base.h"

namespace net_instaweb {

// Maintains a bounded collection of cache-purge records.  These can
// be used to validate data read from a cache.
//
// The entire cache can be flushed as of a certain point in time by
// calling UpdateInvalidationTimestampMs.
//
// We bound the cache-purge data to a certain number of bytes.  When
// we exceed that, we discard old invalidation records, and bump up
// the global invalidation timestamp to cover the evicted purges.
class PurgeSet {
  class InvalidationTimestampHelper;
  typedef LRUCacheBase<int64, InvalidationTimestampHelper> Lru;

 public:
  typedef Lru::Iterator Iterator;

  explicit PurgeSet(size_t max_size);
  ~PurgeSet();

  // Flushes any item in the cache older than timestamp_ms.
  void UpdateGlobalInvalidationTimestampMs(int64 timestamp_ms) {
    global_invalidation_timestamp_ms_ =
        std::max(timestamp_ms, global_invalidation_timestamp_ms_);
  }

  // Adds a new cache purge record to the set.  If we spill over our
  // invalidation limit, we will reset the global cache purge-point based
  // on the evicted node.
  void Put(const GoogleString& key, int64 timestamp_ms);

  // Merge two invalidation records.
  void Merge(const PurgeSet& src);

  // Validates a key against specific invalidation records for that
  // key, and against the overall invalidation timestamp/
  bool IsValid(const GoogleString& key, int64 timestamp_ms) const;

  int64 global_invalidation_timestamp_ms() const {
    return global_invalidation_timestamp_ms_;
  }

  Iterator Begin() const { return lru_->Begin(); }
  Iterator End() const { return lru_->End(); }

  int num_elements() const { return lru_->num_elements(); }
  void Clear();
  void Swap(PurgeSet* that);

 private:
  class InvalidationTimestampHelper {
   public:
    explicit InvalidationTimestampHelper(PurgeSet* purge_set)
        : purge_set_(purge_set) {
    }

    size_t size(int64 value) const {
      return sizeof(value);
    }
    bool Equal(int64 a, int64 b) const {
      return a == b;
    }

    // Update global invalidation timestamp whenever a purge record is
    // evicted to guarantee that that resource remains purged.
    void EvictNotify(int64 evicted_record_timestamp_ms) {
      purge_set_->UpdateGlobalInvalidationTimestampMs(
          evicted_record_timestamp_ms);
    }

    // Only replace purge records if the new one is newer.
    bool ShouldReplace(int64 old_timestamp_ms, int64 new_timestamp_ms) const {
      return new_timestamp_ms > old_timestamp_ms;
    }

    void Swap(InvalidationTimestampHelper* that) {
      std::swap(purge_set_, that->purge_set_);
    }

   private:
    PurgeSet* purge_set_;
  };

  // Global invalidation timestamp value. Anything with a timestamp older than
  // this is considered purged already.
  int64 global_invalidation_timestamp_ms_;
  InvalidationTimestampHelper helper_;
  scoped_ptr<Lru> lru_;

  DISALLOW_COPY_AND_ASSIGN(PurgeSet);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_PURGE_SET_H_
