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

#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/time_util.h"

namespace net_instaweb {

PurgeSet::PurgeSet()
    : global_invalidation_timestamp_ms_(kInitialTimestampMs),
      last_invalidation_timestamp_ms_(0),
      helper_(this),
      lru_(new Lru(1, &helper_)) {  // 1 byte max size till someone sets it.
}

PurgeSet::PurgeSet(size_t max_size)
    : global_invalidation_timestamp_ms_(kInitialTimestampMs),
      last_invalidation_timestamp_ms_(0),
      helper_(this),
      lru_(new Lru(max_size, &helper_)) {
}

PurgeSet::PurgeSet(const PurgeSet& src)
    : global_invalidation_timestamp_ms_(kInitialTimestampMs),
      last_invalidation_timestamp_ms_(0),
      helper_(this),
      lru_(new Lru(src.lru_->max_bytes_in_cache(), &helper_)) {
  Merge(src);
}

PurgeSet::~PurgeSet() {
}

PurgeSet& PurgeSet::operator=(const PurgeSet& src) {
  if (&src != this) {
    Clear();
    lru_->set_max_bytes_in_cache(src.lru_->max_bytes_in_cache());
    Merge(src);
  }
  return *this;
}

void PurgeSet::Clear() {
  lru_->Clear();
  global_invalidation_timestamp_ms_ = kInitialTimestampMs;
}

namespace {

// Maintains temporary storage of keys and values for executing a merge.
// Note that the keys may be of non-trivial size and we'd prefer not
// to copy the keys from the merge source, but we'll need to copy the
// keys from this so we can clear it before re-inserting the data in
// merged order.
class MergeContext {
 public:
  MergeContext(int64 global_invalidation_timestamp_ms,
               int64 this_num_elements,
               int64 src_num_elements)
      : global_invalidation_timestamp_ms_(global_invalidation_timestamp_ms) {
    int total_size = this_num_elements + src_num_elements;
    key_copies_.reserve(this_num_elements);
    keys_.reserve(total_size);
    values_.reserve(total_size);
  }

  void AddPurgeCopyingKey(const GoogleString& key, int64 value) {
    if (value > global_invalidation_timestamp_ms_) {
      key_copies_.push_back(key);
      keys_.push_back(&key_copies_.back());
      values_.push_back(value);
    }
  }

  void AddPurgeSharingKey(const GoogleString& key, int64 value) {
    if (value > global_invalidation_timestamp_ms_) {
      keys_.push_back(&key);
      values_.push_back(value);
    }
  }

  int size() const { return keys_.size(); }
  const GoogleString& key(int index) const { return *keys_[index]; }
  int64 value(int index) const { return values_[index]; }

 private:
  int64 global_invalidation_timestamp_ms_;
  StringVector key_copies_;
  ConstStringStarVector keys_;
  std::vector<int64> values_;

  DISALLOW_COPY_AND_ASSIGN(MergeContext);
};

}  // namespace

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
  // We could use std::merge into the vector but note here how we treat
  // the values from this differently: we need to copy the strings, whereas
  // for src we can just use StringPiece.
  //
  // So instead we have a simple merge-sort into some temp arrays,
  // clear the LRU cache, then re-populate it from the arrays.  This
  // is O(this->size + src.size()).  We might be able to contrive
  // something more sophisticated that's just O(src.size() +
  // log(this->size())), but then we'd have to re-do the
  // keep-2-data-structures-in-sync code in LRUCacheBase using two
  // maps instead of a list and a map.
  //
  // However, the intended usage for this involves re-reading the entire
  // contents from a file on any change in order to keep multiple processes
  // in sync, so quibbling about an extra O(n) walk thorugh the in-memory
  // data does not seem worthwhile.
  global_invalidation_timestamp_ms_ = std::max(
      global_invalidation_timestamp_ms_,
      src.global_invalidation_timestamp_ms_);
  MergeContext merge_context(global_invalidation_timestamp_ms_,
                             lru_->num_elements(),
                             src.lru_->num_elements());
  Lru::Iterator src_iter = src.lru_->Begin(), src_end = src.lru_->End();
  Lru::Iterator this_iter = lru_->Begin(), this_end = lru_->End();
  while ((src_iter != src_end) && (this_iter != this_end)) {
    if (src_iter.Value() < this_iter.Value()) {
      merge_context.AddPurgeSharingKey(src_iter.Key(), src_iter.Value());
      ++src_iter;
    } else {
      merge_context.AddPurgeCopyingKey(this_iter.Key(), this_iter.Value());
      ++this_iter;
    }
  }

  for (; src_iter != src_end; ++src_iter) {
    merge_context.AddPurgeSharingKey(src_iter.Key(), src_iter.Value());
  }
  for (; this_iter != this_end; ++this_iter) {
    merge_context.AddPurgeCopyingKey(this_iter.Key(), this_iter.Value());
  }

  lru_->Clear();
  lru_->ClearStats();
  last_invalidation_timestamp_ms_ = global_invalidation_timestamp_ms_;
  for (int i = 0, n = merge_context.size(); i < n; ++i) {
    CHECK(Put(merge_context.key(i), merge_context.value(i)));
  }
}

bool PurgeSet::UpdateGlobalInvalidationTimestampMs(int64 timestamp_ms) {
  if (!SanitizeTimestamp(&timestamp_ms)) {
    return false;
  }
  global_invalidation_timestamp_ms_ =
      std::max(timestamp_ms, global_invalidation_timestamp_ms_);
  return true;
}

void PurgeSet::EvictNotify(int64 evicted_record_timestamp_ms) {
  // We do not call UpdateGlobalInvalidationTimestampMs because we don't
  // want to silently sanitize the global timestamp to the latest known
  // timestamp.
  DCHECK_GE(last_invalidation_timestamp_ms_, evicted_record_timestamp_ms);
  global_invalidation_timestamp_ms_ =
      std::max(evicted_record_timestamp_ms, global_invalidation_timestamp_ms_);
}

bool PurgeSet::Put(const GoogleString& key, int64 timestamp_ms) {
  if (!SanitizeTimestamp(&timestamp_ms)) {
    return true;
  }
  // Ignore invalidations of individual URLs predating the global
  // invalidation timestamp.
  if (timestamp_ms > global_invalidation_timestamp_ms_) {
    lru_->Put(key, &timestamp_ms);
  }
  return true;
}

bool PurgeSet::SanitizeTimestamp(int64* timestamp_ms) {
  int64 amount_in_past_ms = last_invalidation_timestamp_ms_ - *timestamp_ms;
  if (amount_in_past_ms <= 0) {
    // Time is moving forward.  All is well.
    last_invalidation_timestamp_ms_ = *timestamp_ms;
  } else if (amount_in_past_ms <= kClockSkewAllowanceMs) {
    // A small clock-skew was detected.  Simply clamp it to our last
    // known good timestamp, and accept the fact that this may invalidate
    // a bit into the future.
    *timestamp_ms = last_invalidation_timestamp_ms_;
  } else {
    // Time has moved backward quite a bit.  There is no clear corrective
    // action that doesn't risk leaving far-future invalidations
    // in our system, so reject the request.
    return false;
  }
  return true;
}

bool PurgeSet::IsValid(const GoogleString& key, int64 timestamp_ms) const {
  if (timestamp_ms < global_invalidation_timestamp_ms_) {
    return false;
  }
  int64* purge_timestamp_ms = lru_->GetNoFreshen(key);
  return ((purge_timestamp_ms == NULL) || (timestamp_ms > *purge_timestamp_ms));
}

void PurgeSet::Swap(PurgeSet* that) {
  lru_.swap(that->lru_);  // scoped_ptr::swap
  std::swap(global_invalidation_timestamp_ms_,
            that->global_invalidation_timestamp_ms_);
  helper_.Swap(&that->helper_);
}

bool PurgeSet::Equals(const PurgeSet& that) const {
  if (this == &that) {
    return true;
  }

  if (global_invalidation_timestamp_ms_ !=
      that.global_invalidation_timestamp_ms_) {
    return false;
  }

  if (lru_->num_elements() != that.lru_->num_elements()) {
    return false;
  }

  Lru::Iterator that_iter = that.lru_->Begin(), that_end = that.lru_->End();
  Lru::Iterator this_iter = lru_->Begin(), this_end = lru_->End();
  for (; this_iter != this_end; ++this_iter, ++that_iter) {
    DCHECK(that_iter != that_end);
    if ((that_iter.Value() != this_iter.Value()) ||
        (that_iter.Key() != this_iter.Key())) {
      return false;
    }
  }
  return true;
}

bool PurgeSet::empty() const {
  return ((global_invalidation_timestamp_ms_ == kInitialTimestampMs) &&
          (lru_->num_elements() == 0));
}

GoogleString PurgeSet::ToString() const {
  GoogleString str("Global@");
  GoogleString buf;
  if (ConvertTimeToString(global_invalidation_timestamp_ms_, &buf)) {
    StrAppend(&str, buf);
  } else {
    StrAppend(&str, Integer64ToString(global_invalidation_timestamp_ms_));
  }
  Lru::Iterator this_iter = lru_->Begin(), this_end = lru_->End();
  for (; this_iter != this_end; ++this_iter) {
    StrAppend(&str, "\n", this_iter.Key(), "@");
    buf.clear();
    if (ConvertTimeToString(this_iter.Value(), &buf)) {
      StrAppend(&str, buf);
    } else {
      StrAppend(&str, Integer64ToString(this_iter.Value()));
    }
  }
  return str;
}

}  // namespace net_instaweb
