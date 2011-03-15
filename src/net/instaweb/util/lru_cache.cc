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

#include "net/instaweb/util/public/lru_cache.h"

#include "base/logging.h"
#include "net/instaweb/util/public/shared_string.h"

namespace net_instaweb {

LRUCache::~LRUCache() {
  Clear();
}

// Freshen a key-value pair by putting it in the front of the
// LRU list.  Returns a ListNode to insert into the map so we
// can do fast lookup.
LRUCache::ListNode LRUCache::Freshen(KeyValuePair* key_value) {
  lru_ordered_list_.push_front(key_value);
  return lru_ordered_list_.begin();
}

bool LRUCache::Get(const std::string& key, SharedString* value) {
  Map::iterator p = map_.find(key);
  bool ret = false;
  if (p != map_.end()) {
    ret = true;
    ListNode cell = p->second;
    KeyValuePair* key_value = *cell;
    lru_ordered_list_.erase(cell);
    p->second = Freshen(key_value);
    *value = key_value->second;
    ++num_hits_;
  } else {
    ++num_misses_;
  }
  return ret;
}

void LRUCache::Put(const std::string& key, SharedString* new_value) {
  // Just do one map operation, calling the awkward 'insert' which returns
  // a pair.  The bool indicates whether a new value was inserted, and the
  // iterator provides access to the element, whether it's new or old.
  //
  // If the key is already in the map, this will give us access to the value
  // cell, and the uninitialized cell will not be used.
  ListNode cell;
  std::pair<Map::iterator, bool> iter_found =
      map_.insert(Map::value_type(key, cell));
  bool found = !iter_found.second;
  Map::iterator map_iter = iter_found.first;
  bool need_to_insert = true;
  if (found) {
    cell = map_iter->second;
    KeyValuePair* key_value = *cell;

    // Protect the element that we are rewriting by erasing
    // it from the entry_list prior to calling EvictIfNecessary,
    // which can't find it if it isn't in the list.
    lru_ordered_list_.erase(cell);
    if (**new_value == *(key_value->second)) {
      map_iter->second = Freshen(key_value);
      need_to_insert = false;
      ++num_identical_reinserts_;
    } else {
      ++num_deletes_;
      CHECK_GE(current_bytes_in_cache_, entry_size(key_value));
      current_bytes_in_cache_ -= entry_size(key_value);
      delete key_value;
    }
  }

  if (need_to_insert) {
    // At this point, if we were doing a replacement, then the value
    // is removed from the list, so we can treat replacements and new
    // insertions the same way.  In both cases, the new key is in the map
    // as a result of the call to map_.insert above.

    if (EvictIfNecessary(key.size() + (*new_value)->size())) {
      // The new value fits.  Put it in the LRU-list.
      KeyValuePair* kvp = new KeyValuePair(&map_iter->first, *new_value);
      map_iter->second = Freshen(kvp);
      ++num_inserts_;
    } else {
      // The new value was too big to fit.  Remove it from the map.
      // it's already removed from the list.  We have failed.  We
      // could potentially log this somewhere or keep a stat.
      map_.erase(map_iter);
    }
  }
}

// Evicts enough items from the cache to allow an object of the
// specified bytes-size to be inserted.  If successful, we assumes that
// the item will be inserted and current_bytes_in_cache_ is adjusted
// accordingly.
bool LRUCache::EvictIfNecessary(size_t bytes_needed) {
  bool ret = false;
  if (bytes_needed < max_bytes_in_cache_) {
    while (bytes_needed + current_bytes_in_cache_ > max_bytes_in_cache_) {
      KeyValuePair* key_value = lru_ordered_list_.back();
      lru_ordered_list_.pop_back();
      CHECK_GE(current_bytes_in_cache_, entry_size(key_value));
      current_bytes_in_cache_ -= entry_size(key_value);
      map_.erase(*key_value->first);
      delete key_value;
      ++num_evictions_;
    }
    current_bytes_in_cache_ += bytes_needed;
    ret = true;
  }
  return ret;
}

void LRUCache::Delete(const std::string& key) {
  Map::iterator p = map_.find(key);
  if (p != map_.end()) {
    ListNode cell = p->second;
    KeyValuePair* key_value = *cell;
    lru_ordered_list_.erase(cell);
    CHECK_GE(current_bytes_in_cache_, entry_size(key_value));
    current_bytes_in_cache_ -= entry_size(key_value);
    map_.erase(p);
    delete key_value;
    ++num_deletes_;
  } else {
    // TODO(jmarantz): count number of misses on a 'delete' request?
  }
}

void LRUCache::SanityCheck() {
  CHECK(map_.size() == lru_ordered_list_.size());
  size_t count = 0;
  size_t bytes_used = 0;

  // Walk forward through the list, making sure the map and list elements
  // point to each other correctly.
  for (ListNode cell = lru_ordered_list_.begin(), e = lru_ordered_list_.end();
       cell != e; ++cell, ++count) {
    KeyValuePair* key_value = *cell;
    Map::iterator map_iter = map_.find(*key_value->first);
    CHECK(map_iter != map_.end());
    CHECK(&map_iter->first == key_value->first);
    CHECK(map_iter->second == cell);
    bytes_used += entry_size(key_value);
  }
  CHECK(count == map_.size());
  CHECK(current_bytes_in_cache_ == bytes_used);
  CHECK(current_bytes_in_cache_ <= max_bytes_in_cache_);

  // Walk backward through the list, making sure it's coherent as well.
  count = 0;
  for (EntryList::reverse_iterator cell = lru_ordered_list_.rbegin(),
           e = lru_ordered_list_.rend(); cell != e; ++cell, ++count) {
  }
  CHECK(count == map_.size());
}

CacheInterface::KeyState LRUCache::Query(const std::string& key) {
  Map::iterator p = map_.find(key);
  KeyState state = kNotFound;
  if (p != map_.end()) {
    state = kAvailable;
  }
  return state;
}

// TODO(jmarantz): consider accounting for overhead for list cells, map
// cells, string objects, etc.  Currently we are only accounting for the
// actual characters in the key and value.
size_t LRUCache::entry_size(KeyValuePair* kvp) const {
  return kvp->first->size() + kvp->second->size();
}

void LRUCache::Clear() {
  current_bytes_in_cache_ = 0;

  for (ListNode p = lru_ordered_list_.begin(), e = lru_ordered_list_.end();
       p != e; ++p) {
    KeyValuePair* key_value  = *p;
    delete key_value;
  }
  lru_ordered_list_.clear();
  map_.clear();
}

void LRUCache::ClearStats() {
  num_evictions_ = 0;
  num_hits_ = 0;
  num_misses_ = 0;
  num_inserts_ = 0;
  num_identical_reinserts_ = 0;
  num_deletes_ = 0;
}

}  // namespace net_instaweb
