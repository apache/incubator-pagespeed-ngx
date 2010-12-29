/**
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_LRU_CACHE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_LRU_CACHE_H_

#include <list>
#include <map>
#include "base/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include <string>

namespace net_instaweb {

// Simple C++ implementation of an in-memory least-recently used (LRU)
// cache.  This implementation is not thread-safe, and must be
// combined with a mutex to make it so.
//
// The purpose of this implementation is as a default implementation,
// or an local shadow for memcached.
//
// Also of note: the Get interface allows for streaming.  To get into
// a std::string, use a StringWriter.
//
// TODO(jmarantz): The Put interface does not currently stream, but this
// should be added.
class LRUCache : public CacheInterface {
 public:
  explicit LRUCache(size_t max_size)
      : max_bytes_in_cache_(max_size),
        current_bytes_in_cache_(0) {
    ClearStats();
  }
  virtual ~LRUCache();

  virtual bool Get(const std::string& key, SharedString* value);

  // Puts an object into the cache, sharing the bytes.
  //
  // TODO(jmarantz): currently if the caller mutates the
  // SharedString after having called Put, it will actually
  // modify the value in the cache.  We should change
  // SharedString to Copy-On-Write semantics.
  virtual void Put(const std::string& key, SharedString* new_value);
  virtual void Delete(const std::string& key);
  virtual KeyState Query(const std::string& key);

  // Total size in bytes of keys and values stored.
  size_t size_bytes() const { return current_bytes_in_cache_; }

  // Number of elements stored
  size_t num_elements() const { return map_.size(); }

  size_t num_evictions() const { return num_evictions_; }
  size_t num_hits() const { return num_hits_; }
  size_t num_misses() const { return num_misses_; }
  size_t num_inserts() const { return num_inserts_; }
  size_t num_identical_reinserts() const { return num_identical_reinserts_; }
  size_t num_deletes() const { return num_deletes_; }

  // Sanity check the cache data structures.
  void SanityCheck();

  // Clear the entire cache.  Used primarily for testing.  Note that this
  // will not clear the stats, however it will update current_bytes_in_cache_.
  void Clear();

  // Clear the stats -- note that this will not clear the content.
  void ClearStats();

 private:
  typedef std::pair<const std::string*, SharedString> KeyValuePair;
  typedef std::list<KeyValuePair*> EntryList;
  // STL guarantees lifetime of list itererators as long as the node is in list.
  typedef EntryList::iterator ListNode;
  typedef std::map<std::string, ListNode> Map;
  inline int entry_size(KeyValuePair* kvp) const;
  inline ListNode Freshen(KeyValuePair* key_value);
  bool EvictIfNecessary(size_t bytes_needed);

  size_t max_bytes_in_cache_;
  size_t current_bytes_in_cache_;
  size_t num_evictions_;
  size_t num_hits_;
  size_t num_misses_;
  size_t num_inserts_;
  size_t num_identical_reinserts_;
  size_t num_deletes_;
  EntryList lru_ordered_list_;
  Map map_;

  DISALLOW_COPY_AND_ASSIGN(LRUCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_LRU_CACHE_H_
