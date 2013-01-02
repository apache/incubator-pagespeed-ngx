// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ID_MAP_H_
#define BASE_ID_MAP_H_
#pragma once

#include <set>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/threading/non_thread_safe.h"

// Ownership semantics - own pointer means the pointer is deleted in Remove()
// & during destruction
enum IDMapOwnershipSemantics {
  IDMapExternalPointer,
  IDMapOwnPointer
};

// This object maintains a list of IDs that can be quickly converted to
// pointers to objects. It is implemented as a hash table, optimized for
// relatively small data sets (in the common case, there will be exactly one
// item in the list).
//
// Items can be inserted into the container with arbitrary ID, but the caller
// must ensure they are unique. Inserting IDs and relying on automatically
// generated ones is not allowed because they can collide.
//
// This class does not have a virtual destructor, do not inherit from it when
// ownership semantics are set to own because pointers will leak.
template<typename T, IDMapOwnershipSemantics OS = IDMapExternalPointer>
class IDMap : public base::NonThreadSafe {
 private:
  typedef int32 KeyType;
  typedef base::hash_map<KeyType, T*> HashTable;

 public:
  IDMap() : iteration_depth_(0), next_id_(1), check_on_null_data_(false) {
    // A number of consumers of IDMap create it on one thread but always access
    // it from a different, but consitent, thread post-construction.
    DetachFromThread();
  }

  ~IDMap() {
    // Many IDMap's are static, and hence will be destroyed on the main thread.
    // However, all the accesses may take place on another thread, such as the
    // IO thread. Detaching again to clean this up.
    DetachFromThread();
    Releaser<OS, 0>::release_all(&data_);
  }

  // Sets whether Add should CHECK if passed in NULL data. Default is false.
  void set_check_on_null_data(bool value) { check_on_null_data_ = value; }

  // Adds a view with an automatically generated unique ID. See AddWithID.
  KeyType Add(T* data) {
    DCHECK(CalledOnValidThread());
    CHECK(!check_on_null_data_ || data);
    KeyType this_id = next_id_;
    DCHECK(data_.find(this_id) == data_.end()) << "Inserting duplicate item";
    data_[this_id] = data;
    next_id_++;
    return this_id;
  }

  // Adds a new data member with the specified ID. The ID must not be in
  // the list. The caller either must generate all unique IDs itself and use
  // this function, or allow this object to generate IDs and call Add. These
  // two methods may not be mixed, or duplicate IDs may be generated
  void AddWithID(T* data, KeyType id) {
    DCHECK(CalledOnValidThread());
    CHECK(!check_on_null_data_ || data);
    DCHECK(data_.find(id) == data_.end()) << "Inserting duplicate item";
    data_[id] = data;
  }

  void Remove(KeyType id) {
    DCHECK(CalledOnValidThread());
    typename HashTable::iterator i = data_.find(id);
    if (i == data_.end()) {
      NOTREACHED() << "Attempting to remove an item not in the list";
      return;
    }

    if (iteration_depth_ == 0) {
      Releaser<OS, 0>::release(i->second);
      data_.erase(i);
    } else {
      removed_ids_.insert(id);
    }
  }

  bool IsEmpty() const {
    DCHECK(CalledOnValidThread());
    return size() == 0u;
  }

  T* Lookup(KeyType id) const {
    DCHECK(CalledOnValidThread());
    typename HashTable::const_iterator i = data_.find(id);
    if (i == data_.end())
      return NULL;
    return i->second;
  }

  size_t size() const {
    DCHECK(CalledOnValidThread());
    return data_.size() - removed_ids_.size();
  }

  // It is safe to remove elements from the map during iteration. All iterators
  // will remain valid.
  template<class ReturnType>
  class Iterator {
   public:
    Iterator(IDMap<T, OS>* map)
        : map_(map),
          iter_(map_->data_.begin()) {
      DCHECK(map->CalledOnValidThread());
      ++map_->iteration_depth_;
      SkipRemovedEntries();
    }

    ~Iterator() {
      DCHECK(map_->CalledOnValidThread());
      if (--map_->iteration_depth_ == 0)
        map_->Compact();
    }

    bool IsAtEnd() const {
      DCHECK(map_->CalledOnValidThread());
      return iter_ == map_->data_.end();
    }

    KeyType GetCurrentKey() const {
      DCHECK(map_->CalledOnValidThread());
      return iter_->first;
    }

    ReturnType* GetCurrentValue() const {
      DCHECK(map_->CalledOnValidThread());
      return iter_->second;
    }

    void Advance() {
      DCHECK(map_->CalledOnValidThread());
      ++iter_;
      SkipRemovedEntries();
    }

   private:
    void SkipRemovedEntries() {
      while (iter_ != map_->data_.end() &&
             map_->removed_ids_.find(iter_->first) !=
             map_->removed_ids_.end()) {
        ++iter_;
      }
    }

    IDMap<T, OS>* map_;
    typename HashTable::const_iterator iter_;
  };

  typedef Iterator<T> iterator;
  typedef Iterator<const T> const_iterator;

 private:

  // The dummy parameter is there because C++ standard does not allow
  // explicitly specialized templates inside classes
  template<IDMapOwnershipSemantics OI, int dummy> struct Releaser {
    static inline void release(T* ptr) {}
    static inline void release_all(HashTable* table) {}
  };

  template<int dummy> struct Releaser<IDMapOwnPointer, dummy> {
    static inline void release(T* ptr) { delete ptr;}
    static inline void release_all(HashTable* table) {
      for (typename HashTable::iterator i = table->begin();
           i != table->end(); ++i) {
        delete i->second;
      }
      table->clear();
    }
  };

  void Compact() {
    DCHECK_EQ(0, iteration_depth_);
    for (std::set<KeyType>::const_iterator i = removed_ids_.begin();
         i != removed_ids_.end(); ++i) {
      Remove(*i);
    }
    removed_ids_.clear();
  }

  // Keep track of how many iterators are currently iterating on us to safely
  // handle removing items during iteration.
  int iteration_depth_;

  // Keep set of IDs that should be removed after the outermost iteration has
  // finished. This way we manage to not invalidate the iterator when an element
  // is removed.
  std::set<KeyType> removed_ids_;

  // The next ID that we will return from Add()
  KeyType next_id_;

  HashTable data_;

  // See description above setter.
  bool check_on_null_data_;

  DISALLOW_COPY_AND_ASSIGN(IDMap);
};

#endif  // BASE_ID_MAP_H_
