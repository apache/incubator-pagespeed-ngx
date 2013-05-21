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

#include "pagespeed/kernel/base/basictypes.h"
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
// we exceed that, we discard old invalidation records, and flush the
// predating the flushed invalidations.
class PurgeSet {
 public:
  explicit PurgeSet(size_t max_size);
  ~PurgeSet();

  // Flushes any item in the cache older than timestamp_ms.
  void UpdateInvalidationTimestampMs(int64 timestamp_ms) {
    invalidation_timestamp_ms_ = std::max(timestamp_ms,
                                          invalidation_timestamp_ms_);
  }

  // Adds a new cache purge record to the set.  If we spill over our
  // invalidation limit, we will reset the global cache purge-point based
  // on the evicted node.
  void Put(const GoogleString& key, int64 timestamp_ms) {
    lru_.Put(key, &timestamp_ms);
  }

  // Merge two invalidation records.
  void Merge(const PurgeSet& src);

  // Validates a key against specific invalidation records for that
  // key, and against the overall invalidation timestamp/
  bool IsValid(const GoogleString& key, int64 timestamp_ms) const;

  int64 invalidation_timestamp_ms() const { return invalidation_timestamp_ms_; }

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
      purge_set_->UpdateInvalidationTimestampMs(evicted_record_timestamp_ms);
    }

    // Only replace purge records if the new one is newer.
    bool ShouldReplace(int64 old_timestamp_ms, int64 new_timestamp_ms) const {
      return new_timestamp_ms > old_timestamp_ms;
    }

   private:
    PurgeSet* purge_set_;
  };

  typedef LRUCacheBase<int64, InvalidationTimestampHelper> Lru;
  int64 invalidation_timestamp_ms_;
  InvalidationTimestampHelper helper_;
  Lru lru_;

  DISALLOW_COPY_AND_ASSIGN(PurgeSet);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_PURGE_SET_H_
