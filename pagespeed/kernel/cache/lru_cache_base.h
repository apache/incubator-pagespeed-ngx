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

#ifndef PAGESPEED_KERNEL_CACHE_LRU_CACHE_BASE_H_
#define PAGESPEED_KERNEL_CACHE_LRU_CACHE_BASE_H_

#include <cstddef>
#include <list>
#include <utility>  // for pair

#include "base/logging.h"
#include "strings/stringpiece_utils.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/rde_hash_map.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_hash.h"
#include "pagespeed/kernel/base/string_util.h"


namespace net_instaweb {

// Implements a general purpose in-memory least-recently used (LRU)
// cache, using strings as keys and arbitrary values as objects.
// This implementation is not thread-safe, and must be combined with
// an externally managed mutex to make it so.
//
// This is templated on ValueType, which holds the value, and on ValueHelper,
// which must define:
//
//   // Computes the size of a value.  Used to track resource consumption.
//   size_t size(const ValueType&) const;
//
//   // Determines whether two values are equal.
//   bool Equal(const ValueType& a, const ValueType& b) const;
//
//   // Called when objects are evicted.  Note that destruction does
//   // not imply eviction.
//   void EvictNotify(const ValueType&);
//
//   // Determines whether a new_value should supercede old_value on a Put.
//   bool ShouldReplace(const ValueType& old_value,
//                      const ValueType& new_value) const;
//
// ValueType must support copy-construction and assign-by-value.
template<class ValueType, class ValueHelper>
class LRUCacheBase {
  typedef std::pair<GoogleString, ValueType> KeyValuePair;
  typedef std::list<KeyValuePair*> EntryList;
  // STL guarantees lifetime of list iterators as long as the node is in list.
  typedef typename EntryList::iterator ListNode;

  typedef rde::hash_map<GoogleString, ListNode, CasePreserveStringHash> Map;

 public:
  class Iterator {
   public:
    explicit Iterator(const typename EntryList::const_reverse_iterator& iter)
        : iter_(iter) {
    }

    void operator++() { ++iter_; }
    bool operator==(const Iterator& src) const { return iter_ == src.iter_; }
    bool operator!=(const Iterator& src) const { return iter_ != src.iter_; }

    const GoogleString& Key() const {
      const KeyValuePair* key_value_pair = *iter_;
      return key_value_pair->first;
    }
    const ValueType& Value() const {
      const KeyValuePair* key_value_pair = *iter_;
      return key_value_pair->second;
    }

   private:
    typename EntryList::const_reverse_iterator iter_;

    // Implicit copy and assign are OK.
  };

  LRUCacheBase(size_t max_size, ValueHelper* value_helper)
      : max_bytes_in_cache_(max_size),
        current_bytes_in_cache_(0),
        value_helper_(value_helper) {
    ClearStats();
  }
  ~LRUCacheBase() {
    Clear();
  }

  // Resets the max size in the cache.  This does not take effect immediately;
  // e.g. if you are shrinking the cache size, this call will not evict
  // anything.  Only when something new is put into the cache will we evict.
  void set_max_bytes_in_cache(size_t max_size) {
    max_bytes_in_cache_ = max_size;
  }

  // Returns a pointer to the stored value, or NULL if not found, freshening
  // the entry in the lru-list.  Note: this pointer is safe to use until the
  // next call to Put or Delete in the cache.
  ValueType* GetFreshen(const GoogleString& key) {
    ValueType* value = NULL;
    typename Map::iterator p = map_.find(key);
    if (p != map_.end()) {
      ListNode cell = p->second;
      KeyValuePair* key_value = *cell;
      p->second = Freshen(cell);
      // Note: it's safe to assume the list iterator will remain valid so that
      // the caller can do what's necessary with the pointer.
      // http://stackoverflow.com/questions/759274/
      // what-is-the-lifetime-and-validity-of-c-iterators
      value = &key_value->second;
      ++num_hits_;
    } else {
      ++num_misses_;
    }
    return value;
  }

  ValueType* GetNoFreshen(const GoogleString& key) const {
    ValueType* value = NULL;
    typename Map::const_iterator p = map_.find(key);
    if (p != map_.end()) {
      ListNode cell = p->second;
      KeyValuePair* key_value = *cell;
      // See the above comment about stl::list::iterator lifetime.
      value = &key_value->second;
      ++num_hits_;
    } else {
      ++num_misses_;
    }
    return value;
  }

  // Puts an object into the cache.  The value is copied using the assignment
  // operator.
  void Put(const GoogleString& key, const ValueType& new_value) {
    // Just do one map operation, calling the awkward 'insert' which returns
    // a pair.  The bool indicates whether a new value was inserted, and the
    // iterator provides access to the element, whether it's new or old.
    //
    // If the key is already in the map, this will give us access to the value
    // cell, and the uninitialized cell will not be used.
    ListNode cell;
    std::pair<typename Map::iterator, bool> iter_found =
        map_.insert(typename Map::value_type(key, cell));
    bool found = !iter_found.second;
    typename Map::iterator map_iter = iter_found.first;
    bool need_to_insert = true;
    if (found) {
      cell = map_iter->second;
      KeyValuePair* key_value = *cell;

      // Protect the element that we are rewriting by erasing
      // it from the entry_list prior to calling EvictIfNecessary,
      // which can't find it if it isn't in the list.
      if (!value_helper_->ShouldReplace(key_value->second, new_value)) {
        need_to_insert = false;
      } else {
        if (value_helper_->Equal(new_value, key_value->second)) {
          map_iter->second = Freshen(cell);
          need_to_insert = false;
          ++num_identical_reinserts_;
        } else {
          ++num_deletes_;
          CHECK_GE(current_bytes_in_cache_, EntrySize(key_value));
          current_bytes_in_cache_ -= EntrySize(key_value);
          delete key_value;
          lru_ordered_list_.erase(cell);
        }
      }
    }

    if (need_to_insert) {
      // At this point, if we were doing a replacement, then the value
      // is removed from the list, so we can treat replacements and new
      // insertions the same way.  In both cases, the new key is in the map
      // as a result of the call to map_.insert above.

      if (EvictIfNecessary(key.size() + value_helper_->size(new_value))) {
        // The new value fits.  Put it in the LRU-list.
        KeyValuePair* kvp = new KeyValuePair(map_iter->first, new_value);
        lru_ordered_list_.push_front(kvp);
        map_iter->second = lru_ordered_list_.begin();
        ++num_inserts_;
      } else {
        // The new value was too big to fit.  Remove it from the map.
        // it's already removed from the list.  We have failed.  We
        // could potentially log this somewhere or keep a stat.
        map_.erase(map_iter);
      }
    }
  }

  void Delete(const GoogleString& key) {
    typename Map::iterator p = map_.find(key);
    if (p != map_.end()) {
      DeleteAt(p);
    } else {
      // TODO(jmarantz): count number of misses on a 'delete' request?
    }
  }

  // Deletes all objects whose key starts with prefix.
  // Note: this takes time proportional to the size of the map, and is only
  // meant for test use.
  void DeleteWithPrefixForTesting(StringPiece prefix) {
    typename Map::iterator p = map_.begin();
    while (p != map_.end()) {
      if (strings::StartsWith(p->first, prefix)) {
        typename Map::iterator next = p;
        ++next;
        DeleteAt(p);
        p = next;
      } else {
        ++p;
      }
    }
  }

  void MergeStats(const LRUCacheBase& src) {
    current_bytes_in_cache_ += src.current_bytes_in_cache_;
    num_evictions_ += src.num_evictions_;
    num_hits_ += src.num_hits_;
    num_misses_ += src.num_misses_;
    num_inserts_ += src.num_inserts_;
    num_identical_reinserts_ += src.num_identical_reinserts_;
    num_deletes_ += src.num_deletes_;
  }

  // Total size in bytes of keys and values stored.
  size_t size_bytes() const { return current_bytes_in_cache_; }

  // Maximum capacity.
  size_t max_bytes_in_cache() const { return max_bytes_in_cache_; }

  // Number of elements stored
  size_t num_elements() const { return map_.size(); }

  size_t num_evictions() const { return num_evictions_; }
  size_t num_hits() const { return num_hits_; }
  size_t num_misses() const { return num_misses_; }
  size_t num_inserts() const { return num_inserts_; }
  size_t num_identical_reinserts() const { return num_identical_reinserts_; }
  size_t num_deletes() const { return num_deletes_; }

  // Sanity check the cache data structures.
  void SanityCheck() {
    CHECK_EQ(static_cast<size_t>(map_.size()), lru_ordered_list_.size());
    size_t count = 0;
    size_t bytes_used = 0;

    // Walk forward through the list, making sure the map and list elements
    // point to each other correctly.
    for (ListNode cell = lru_ordered_list_.begin(), e = lru_ordered_list_.end();
         cell != e; ++cell, ++count) {
      KeyValuePair* key_value = *cell;
      typename Map::iterator map_iter = map_.find(key_value->first);
      CHECK(map_iter != map_.end());
      CHECK(map_iter->first == key_value->first);
      CHECK(map_iter->second == cell);
      bytes_used += EntrySize(key_value);
    }
    CHECK_EQ(count, static_cast<size_t>(map_.size()));
    CHECK_EQ(current_bytes_in_cache_, bytes_used);
    CHECK_LE(current_bytes_in_cache_, max_bytes_in_cache_);

    // Walk backward through the list, making sure it's coherent as well.
    count = 0;
    for (typename EntryList::reverse_iterator cell = lru_ordered_list_.rbegin(),
             e = lru_ordered_list_.rend(); cell != e; ++cell, ++count) {
    }
    CHECK_EQ(count, static_cast<size_t>(map_.size()));
  }

  // Clear the entire cache.  Used primarily for testing.  Note that this
  // will not clear the stats, however it will update current_bytes_in_cache_.
  void Clear() {
    current_bytes_in_cache_ = 0;

    for (ListNode p = lru_ordered_list_.begin(), e = lru_ordered_list_.end();
         p != e; ++p) {
      KeyValuePair* key_value  = *p;
      delete key_value;
    }
    lru_ordered_list_.clear();
    map_.clear();
  }

  // Clear the stats -- note that this will not clear the content.
  void ClearStats() {
    num_evictions_ = 0;
    num_hits_ = 0;
    num_misses_ = 0;
    num_inserts_ = 0;
    num_identical_reinserts_ = 0;
    num_deletes_ = 0;
  }

  // Iterators for walking cache entries from oldest to youngest.
  Iterator Begin() const { return Iterator(lru_ordered_list_.rbegin()); }
  Iterator End() const { return Iterator(lru_ordered_list_.rend()); }

 private:
  // TODO(jmarantz): consider accounting for overhead for list cells, map
  // cells.
  size_t EntrySize(KeyValuePair* kvp) const {
    return kvp->first.size() + value_helper_->size(kvp->second);
  }

  ListNode Freshen(ListNode cell) {
    if (cell != lru_ordered_list_.begin()) {
      lru_ordered_list_.splice(lru_ordered_list_.begin(),
                               lru_ordered_list_,
                               cell);
    }

    return lru_ordered_list_.begin();
  }

  void DeleteAt(typename Map::iterator p) {
    ListNode cell = p->second;
    KeyValuePair* key_value = *cell;
    lru_ordered_list_.erase(cell);
    CHECK_GE(current_bytes_in_cache_, EntrySize(key_value));
    current_bytes_in_cache_ -= EntrySize(key_value);
    map_.erase(p);
    delete key_value;
    ++num_deletes_;
  }

  bool EvictIfNecessary(size_t bytes_needed) {
    bool ret = false;
    if (bytes_needed < max_bytes_in_cache_) {
      while (bytes_needed + current_bytes_in_cache_ > max_bytes_in_cache_) {
        KeyValuePair* key_value = lru_ordered_list_.back();
        lru_ordered_list_.pop_back();
        CHECK_GE(current_bytes_in_cache_, EntrySize(key_value));
        current_bytes_in_cache_ -= EntrySize(key_value);
        value_helper_->EvictNotify(key_value->second);
        map_.erase(key_value->first);
        delete key_value;
        ++num_evictions_;
      }
      current_bytes_in_cache_ += bytes_needed;
      ret = true;
    }
    return ret;
  }

  // TODO(jmarantz): convert most of these to 'int'.
  size_t max_bytes_in_cache_;
  size_t current_bytes_in_cache_;
  size_t num_evictions_;
  mutable size_t num_hits_;
  mutable size_t num_misses_;
  size_t num_inserts_;
  size_t num_identical_reinserts_;
  size_t num_deletes_;
  EntryList lru_ordered_list_;
  Map map_;
  ValueHelper* value_helper_;

  DISALLOW_COPY_AND_ASSIGN(LRUCacheBase);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_CACHE_LRU_CACHE_BASE_H_
