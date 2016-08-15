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
#include "pagespeed/kernel/base/timer.h"
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

  // Used for sanity checking timestamps read from the cache.flush file,
  // allowing for small skew and system clock adjustments.  Setting this
  // to 10 minutes means that we can prevent any cache entries from
  // being valid for 10 minutes, disabling whatever functionality is
  // dependent on that.
  static const int64 kClockSkewAllowanceMs = 10 * Timer::kMinuteMs;

  // Initial value used for the global timestamp.  This means there
  // is no valid timestamp.
  static const int64 kInitialTimestampMs = -1;

  // The default constructor makes a 1-byte invalidation set.  Use
  // set_max_size after construction.  The default constructor is
  // needed for CopyOnWrite.
  PurgeSet();

  explicit PurgeSet(size_t max_size);
  PurgeSet(const PurgeSet& src);
  ~PurgeSet();

  // Call this immediately after construction.
  void set_max_size(size_t x) { lru_->set_max_bytes_in_cache(x); }

  PurgeSet& operator=(const PurgeSet& src);

  // Flushes any item in the cache older than timestamp_ms.
  //
  //
  // Returns false if this request represents an excessive warp back in
  // time.
  bool UpdateGlobalInvalidationTimestampMs(int64 timestamp_ms);

  // Adds a new cache purge record to the set.  If we spill over our
  // invalidation limit, we will reset the global cache purge-point based
  // on the evicted node.
  //
  // Returns false if this request represents an excessive warp back in
  // time.
  bool Put(const GoogleString& key, int64 timestamp_ms);

  // Merge two invalidation records.
  void Merge(const PurgeSet& src);

  // Validates a key against specific invalidation records for that
  // key, and against the overall invalidation timestamp/
  bool IsValid(const GoogleString& key, int64 timestamp_ms) const;

  int64 global_invalidation_timestamp_ms() const {
    return global_invalidation_timestamp_ms_;
  }

  bool has_global_invalidation_timestamp_ms() const {
    return global_invalidation_timestamp_ms_ != kInitialTimestampMs;
  }

  Iterator Begin() const { return lru_->Begin(); }
  Iterator End() const { return lru_->End(); }

  int num_elements() const { return lru_->num_elements(); }
  void Clear();
  void Swap(PurgeSet* that);

  bool Equals(const PurgeSet& that) const;
  bool empty() const;

  GoogleString ToString() const;

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
      purge_set_->EvictNotify(evicted_record_timestamp_ms);
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

  friend class InvalidationTimestampHelper;

  void EvictNotify(int64 evicted_record_timestamp_ms);

  // Determines whether this timestamp is monotonically increasing from
  // previous ones encountered.  Small amounts of time-reversal are handled
  // by setting them to a recently observed time.  Large amounts of
  // time-reversal cause false to be returned.
  //
  // Here several scenarios:
  //   1. Time goes backward by a few minutes or less:
  //      a. On purge requests, force monotonically increasing time.
  //      b. IsValid: we may report false negatives, disabling PageSpeed
  //         for a few minutes.
  //   2. Time moves backward by a large amount (>10 minutes):
  //      a. Purge requests: rejected until time is corrected.  The only
  //         sure-fire remedy is to delete all caches, flush data on external
  //         cache servers (e.g. restart memcached), restart pagespeed servers.
  //      b. IsValid: returns false, disabling PageSpeed until the situation
  //         is corrected.  We view it as unacceptable to bring purged cache
  //         entries back from the dead.
  // TODO(jmarantz): add a statistic that gets bumped from IsValid when
  // a far-future expires is detected.
  bool SanitizeTimestamp(int64* timestamp_ms);

  // Global invalidation timestamp value. Anything with a timestamp older than
  // this is considered purged already.
  int64 global_invalidation_timestamp_ms_;

  // last_invalidation_timestamp_ms is used to keep the data structure invariant
  // in the face of time jumping backwards.  That can happen if someone resets
  // the system-clock or there is a correction due to NTP sync, etc.
  int64 last_invalidation_timestamp_ms_;

  InvalidationTimestampHelper helper_;
  scoped_ptr<Lru> lru_;

  // Explicit copy-constructor and assign-operator are provided so
  // this class can be used for CopyOnWrite.
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_PURGE_SET_H_
