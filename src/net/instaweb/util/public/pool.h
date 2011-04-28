// Copyright 2011 Google Inc.
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
// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_POOL_H_
#define NET_INSTAWEB_UTIL_PUBLIC_POOL_H_
#include <cstddef>
#include <list>
#include "base/logging.h"  // for DCHECK
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/pool_element.h"
#include "net/instaweb/util/public/stl_util.h"

namespace net_instaweb {

// A pool holds references to objects of type T that provide a pool_position
// method (which they can do by extending PoolElement in pool_element.h, or by
// providing a getter method of type PoolElement::Position* pool_position() (an
// abstract settable pointer for a PoolElement::Position).  Pool objects are
// maintained in insertion order.
//
// We can insert and remove references from a pool, ask for an arbitrary
// reference contained in a pool, and clear or delete all pool elements.  By
// default on destruction we delete pool elements using DeleteAll(); if a pool
// does not acquire object ownership, we should instead .Clear() it before
// destruction.
template<class T>
class Pool {
 public:
  // We can iterate over a pool using this iterator type.
  typedef typename PoolElement<T>::Position iterator;
  typedef typename std::list<T*>::const_iterator const_iterator;

  Pool() { }

  ~Pool() {
    DeleteAll();
  }

  // Is pool empty?
  bool empty() const {
    return contents_.empty();
  }

  // Size of pool
  size_t size() const {
    return contents_.size();
  }

  // Iterator pointing to beginning of pool
  iterator begin() {
    return contents_.begin();
  }

  // const Iterator pointing to beginning of pool
  const_iterator begin() const {
    return contents_.begin();
  }

  // Iterator pointing just past end of pool
  iterator end() {
    return contents_.end();
  }

  // Iterator pointing just past end of pool
  const_iterator end() const {
    return contents_.end();
  }

  // Add object to pool.  The object must not currently reside in a pool.
  void Add(T* object) {
    iterator* position = object->pool_position();
    contents_.push_back(object);
    // We need to get an iterator to the last element.  We locally bind to avoid
    // potential compiler trouble.
    iterator back_iter = contents_.end();
    --back_iter;
    *position = back_iter;
  }

  // Remove specified object from pool.  The object must have previously been
  // inserted into this pool.  Returns object.
  T* Remove(T* object) {
    iterator* position = object->pool_position();
    DCHECK(**position == object);
    contents_.erase(*position);
    *position = contents_.end();
    return object;
  }

  // Return oldest object in pool, or NULL.
  T* oldest() const {
    T* result = NULL;
    if (!contents_.empty()) {
      result = contents_.front();
    }
    return result;
  }

  // Remove the least-recently-inserted object from the pool.  Potentially
  // cheaper than Remove(Oldest()).
  T* RemoveOldest() {
    T* result = NULL;
    if (!contents_.empty()) {
      result = contents_.front();
      iterator* position = result->pool_position();
      DCHECK(*position == contents_.begin());
      contents_.pop_front();
      *position = contents_.end();
    }
    return result;
  }

  // DeleteAll: delete all elements of pool
  void DeleteAll() {
    STLDeleteElements(&contents_);
  }

  // Clear: clear pool without deleting elements
  void Clear() {
    contents_.clear();
  }

 private:
  std::list<T*> contents_;

  DISALLOW_COPY_AND_ASSIGN(Pool);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_POOL_H_
