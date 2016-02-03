// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: cheesy@google.com (Steve Hill)

#ifndef PAGESPEED_CONTROLLER_PRIORITY_QUEUE_H_
#define PAGESPEED_CONTROLLER_PRIORITY_QUEUE_H_

#include <cstddef>
#include <functional>
#include <utility>
#include <unordered_map>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "pagespeed/kernel/base/basictypes.h"

// Priority queue that supports incrementing the priority of a key.

namespace net_instaweb {

template <typename T,
          typename HashFn = std::hash<T>,
          typename EqFn = std::equal_to<T>>
class PriorityQueue {
 public:
  PriorityQueue() { }
  ~PriorityQueue() { Clear(); }

  // Increase the priority of "key" by amount. If key is not already present,
  // it will be inserted at priority amount. amount may be negative.
  void IncreasePriority(const T& key, int64 amount);

  // Eqivalent to IncreasePriority(key, 1).
  void Increment(const T& key);

  // Remove a given element. Silently succeeds if the element isn't present.
  void Remove(const T& key);

  // Return the key with the highest priority, and its priority.
  const std::pair<const T*, int64>& Top() const;

  // Remove the key with the highest priority from the queue.
  void Pop();

  bool Empty() const { return queue_.empty(); }
  size_t Size() const { return queue_.size(); }

  void Clear();

 private:
  // Functors that wrap the standard templates to handle pointers. index_map_
  // and queue_ must each know the value, so we store it by pointer to avoid
  // having two copies around.
  struct PtrHash {
    size_t operator()(const T* x) const {
      return HashFn()(*x);
    }
  };

  struct PtrEq {
    bool operator()(const T* x, const T* y) const {
      return (x == y || EqFn()(*x, *y));
    }
  };

  // Restore heap property by manipulating queue_, starting at the specified
  // index.
  void Rebalance(size_t pos);  // Bootstraps one of the Push methods.
  void PushDown(size_t pos);
  void PushUp(size_t pos);

  // Swap two elements in queue_, updating index_map_. Safe to call with a == b.
  void SwapElements(size_t a, size_t b);

  // Verifies the keys are correctly synchronised between queue_ and index_map_
  // and that the heap property has not been violated.
  void SanityCheckForTesting() const;

  // Map items onto their position in queue_.
  typedef std::unordered_map<const T*, size_t, PtrHash, PtrEq>
      IndexMap;
  IndexMap index_map_;

  // The actual max-heap. Stores the value so that it can look it back up in
  // index. queue_ is considered to own this pointer.
  typedef std::pair<const T*, int64> QueueEntry;
  std::vector<QueueEntry> queue_;

  friend class PriorityQueueTest;

  DISALLOW_COPY_AND_ASSIGN(PriorityQueue);
};

template <typename T, typename Hash, typename Equal>
void PriorityQueue<T, Hash, Equal>::Clear() {
  index_map_.clear();
  for (const QueueEntry& qe : queue_) {
    delete qe.first;
  }
  queue_.clear();
}

template <typename T, typename Hash, typename Equal>
void PriorityQueue<T, Hash, Equal>::Increment(const T& key) {
  IncreasePriority(key, 1);
}

template <typename T, typename Hash, typename Equal>
void PriorityQueue<T, Hash, Equal>::IncreasePriority(const T& key,
                                                     int64 amount) {
  typename IndexMap::iterator i = index_map_.find(&key);
  // Is key already stored?
  if (i == index_map_.end()) {
    // Insert new entry at the end of the queue.
    size_t queue_pos = queue_.size();
    queue_.emplace_back(new T(key), 0);
    // Now put key in the index.
    const T* created_key = queue_.back().first;
    std::pair<typename IndexMap::iterator, bool> insert_result =
        index_map_.emplace(created_key, queue_pos);
    CHECK(insert_result.second);  // No existing entry.
    i = insert_result.first;
  }
  size_t queue_pos = i->second;
  CHECK(queue_pos < queue_.size());
  queue_[queue_pos].second += amount;
  Rebalance(queue_pos);
}

template <typename T, typename Hash, typename Equal>
void PriorityQueue<T, Hash, Equal>::Remove(const T& key) {
  typename IndexMap::iterator i = index_map_.find(&key);
  if (i == index_map_.end()) {
    // Key not present, do nothing.
    return;
  }

  // Swap the value being removed with the value at the back.
  // If there is only one entry in the queue, this swaps 0 and 0.
  size_t removed_pos = i->second;
  SwapElements(removed_pos, queue_.size() - 1);

  // Remove/delete the old entry.
  const T* removed_key = queue_.back().first;
  queue_.pop_back();
  index_map_.erase(i);
  delete removed_key;

  if (!Empty() && removed_pos < queue_.size()) {
    Rebalance(removed_pos);
  }
}

template <typename T, typename Hash, typename Equal>
void PriorityQueue<T, Hash, Equal>::Rebalance(size_t pos) {
  CHECK_LT(pos, queue_.size());

  size_t parent_pos = (pos >> 1);

  // If the node has a parent and the parent's priority is less than that of
  // the node, we need to start moving up.
  if (pos != 0 && queue_[parent_pos].second < queue_[pos].second) {
    PushUp(pos);
  } else {
    PushDown(pos);
  }
}

template <typename T, typename Hash, typename Equal>
const std::pair<const T*, int64>& PriorityQueue<T, Hash, Equal>::Top() const {
  CHECK(!Empty());
  return queue_.front();
}

template <typename T, typename Hash, typename Equal>
void PriorityQueue<T, Hash, Equal>::Pop() {
  if (!Empty()) {
    // Swap the first and last entries in the queue.
    // If there is only one entry in the queue, this swaps 0 and 0.
    SwapElements(0, queue_.size() - 1);
    // Remove the old top entry off the back of the queue.
    // First find the key for the old top and remove it from the queue.
    const T* removed_key = queue_.back().first;
    queue_.pop_back();
    // Remove the key from index_map_.
    size_t num_deleted = index_map_.erase(removed_key);
    CHECK_EQ(num_deleted, 1);
    // Free the key.
    delete removed_key;
    // Finally, restore heap property by re-balancing the entry we just put
    // into the first position. It is possible that we are empty at this point.
    PushDown(0);
  }
}

template <typename T, typename Hash, typename Equal>
void PriorityQueue<T, Hash, Equal>::SwapElements(size_t a_idx, size_t b_idx) {
  if (a_idx != b_idx) {  // Just an optimization, still works if they are equal.
    const T* a_key = queue_[a_idx].first;
    const T* b_key = queue_[b_idx].first;
    std::swap(index_map_[a_key], index_map_[b_key]);
    std::swap(queue_[a_idx], queue_[b_idx]);
  }
}

template <typename T, typename Hash, typename Equal>
void PriorityQueue<T, Hash, Equal>::PushDown(size_t pos) {
  while (pos * 2 < queue_.size()) {
    size_t child = pos * 2;
    // Find the larger of the children (if two exist).
    if (child + 1 < queue_.size() &&
        queue_[child].second < queue_[child + 1].second) {
      ++child;
    }
    // Now swap if the parent is less than the larger child.
    if (queue_[pos].second < queue_[child].second) {
      SwapElements(pos, child);
      pos = child;
    } else {
      break;
    }
  }
}

template <typename T, typename Hash, typename Equal>
void PriorityQueue<T, Hash, Equal>::PushUp(size_t pos) {
  while (pos != 0 && pos < queue_.size()) {
    size_t parent = (pos >> 1);
    if (queue_[parent].second < queue_[pos].second) {
      SwapElements(pos, parent);
      pos = parent;
    } else {
      break;
    }
  }
}

template <typename T, typename Hash, typename Equal>
void PriorityQueue<T, Hash, Equal>::SanityCheckForTesting() const {
  CHECK_EQ(queue_.size(), index_map_.size());

  for (size_t queue_pos = 0; queue_pos < queue_.size(); ++queue_pos) {
    // Verify queue_ and index_map_ are consistent with each other.
    const T* key = queue_[queue_pos].first;
    // operator[] is not const, so we have to use find.
    typename IndexMap::const_iterator i = index_map_.find(key);
    CHECK(i != index_map_.end());
    size_t indexed_pos = i->second;
    CHECK_EQ(indexed_pos, queue_pos);

    // Verify heap property.
    if (queue_pos > 0) {
      size_t parent_pos = (queue_pos >> 1);
      CHECK_LE(queue_[queue_pos].second, queue_[parent_pos].second);
    }
  }
}

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_PRIORITY_QUEUE_H_
