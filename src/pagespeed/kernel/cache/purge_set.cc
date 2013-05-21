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

#include "pagespeed/kernel/cache/purge_set.h"

namespace net_instaweb {

PurgeSet::PurgeSet(size_t max_size)
    : invalidation_timestamp_ms_(0),
      helper_(this),
      lru_(max_size, &helper_) {
}

PurgeSet::~PurgeSet() {
}

void PurgeSet::Merge(const PurgeSet& src) {
  // Note that this can almost be implemented using simple ordered
  // loop through src's list, putting values will into this.  But that
  // will get the ordering wrong, and thus get the wrong answer for
  // PurgeSetTest.Merge.
  //
  // Note that ordering matters for evicting the oldest invalidation records.
  //
  // Note also that std::merge could do the right thing, except that we
  // don't have an OutputIterator for our Lru object which must keep two
  // data structures (list & map) synced.  And it's a hard to provide
  // all the STL output-iterator semantics needed for std::merge to work.
  //
  // So instead we have a simple merge-sort into 'target', which we
  // than swap with this->lru_.  This is O(this->size + src.size()).
  // We might be able to contrive something more sophisticated that's
  // just O(src.size() + log(this->size())), but then we'd have to
  // re-do the keep-2-data-structures-in-sync code in LRUCacheBase
  // using two maps instead of a list and a map.
  //
  // However, the intended usage for this involves re-reading the entire
  // contents from a file on any change in order to keep multiple processes
  // in sync, so quibbling about an extra O(n) walk thorugh the in-memory
  // data does not seem worthwhile.

  PurgeSet target(lru_.max_bytes_in_cache());
  Lru::Iterator src_iter = src.lru_.Begin(), src_end = src.lru_.End();
  Lru::Iterator this_iter = lru_.Begin(), this_end = lru_.End();
  while ((src_iter != src_end) && (this_iter != this_end)) {
    if (src_iter.Value() < this_iter.Value()) {
      target.Put(src_iter.Key(), src_iter.Value());
      ++src_iter;
    } else {
      target.Put(this_iter.Key(), this_iter.Value());
      ++this_iter;
    }
  }

  for (; src_iter != src_end; ++src_iter) {
    target.Put(src_iter.Key(), src_iter.Value());
  }
  for (; this_iter != this_end; ++this_iter) {
    target.Put(this_iter.Key(), this_iter.Value());
  }
  target.lru_.SwapData(&lru_);
  lru_.MergeStats(src.lru_);
  UpdateInvalidationTimestampMs(src.invalidation_timestamp_ms_);
}

bool PurgeSet::IsValid(const GoogleString& key, int64 timestamp_ms) const {
  if (timestamp_ms <= invalidation_timestamp_ms_) {
    return false;
  }
  int64* purge_timestamp_ms = lru_.GetNoFreshen(key);
  return ((purge_timestamp_ms == NULL) || (timestamp_ms > *purge_timestamp_ms));
}

}  // namespace net_instaweb
