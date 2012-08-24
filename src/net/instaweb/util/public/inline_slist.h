// Copyright 2012 Google Inc.
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
// Author: morlovich@google.com (Maksim Orlovich)
//
// This contains a simple linked list that's optmized for memory usage,
// cheap appends and traversals (including removals). Links
// are stored within elements rather than externally.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_INLINE_SLIST_H_
#define NET_INSTAWEB_UTIL_PUBLIC_INLINE_SLIST_H_

#include <cstddef>

#include "base/logging.h"  // for DCHECK
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

template<class T> class InlineSList;

// A helper base class for things that would get stored in the list.
// You don't have to inherit this, and can implement next() and set_next()
// directly.
template<class T>
class InlineSListElement {
 protected:
  InlineSListElement() : next_(NULL) {}

 private:
  friend class InlineSList<T>;
  T* next() { return next_; }
  void set_next(T* new_next) { next_ = new_next; }

  T* next_;
  DISALLOW_COPY_AND_ASSIGN(InlineSListElement);
};

// A simple linked list that's optimized for memory usage,
// cheap appends and traversals (including removals). Links
// are stored within elements rather than externally.
//
// To permit that, the type T must provide next() and set_next() methods,
// accessible to InlineSList<T>. Easy way to do that is by inheriting off
// InlineSListElement<T>.
//
// Note that while this results in a list object that's just one pointer wide,
// iterators are two pointers wide.
//
// Representation: circular linked list with a pointer to tail. Iterators
// store pointers to nodes before the one they're conceptually targeting.
template<class T>
class InlineSList {
 private:
  // (Unfortunately, this private class has to be above the public: section
  //  since public classes inherit off it).
  //
  // We represent an iterator by keeping a pointer to the node before
  // the one it represents, which makes it easy to delete things.
  //
  // We also keep a pointer to the containing list, so we can
  // detect when we walk past the end of the list, at which point we
  // turn the node_ pointer into NULL. (Which also means the begin
  // iterator for an empty list is a one-past-end iterator, as expected).
  class IterBase {
   protected:
    IterBase(const InlineSList<T>* list, T* node)
        : list_(list), node_(node) {
    }

    bool AtEnd() const {
      return (node_ == NULL);
    }

    void Advance() {
      DCHECK(!AtEnd());
      node_ = node_->next();
      // If we travel to the tail node (as opposed to start pointing to it),
      // we have reached the end, and became one-past-the-end iterator.
      if (node_ == list_->tail_) {
        node_ = NULL;
      }
    }

    T* Data() {
      return node_->next();
    }

    bool Equals(const IterBase& other) const {
      return (node_ == other.node_) && (list_ == other.list_);
    }

   private:
    friend class InlineSList<T>;
    const InlineSList<T>* list_;
    T* node_;
  };

 public:
  // Iterator interface to the list contents. You may use this both for simple
  // enumeration and for deletion. Iteration works the same as with any STL
  // container.
  //
  // If you want to remove things, make sure not to call the operator ++
  // when you do, as after deletion the iterator will be pointing at the next
  // element already (or past the end!). An example of doing it right:
  //
  // InlineSList<Type>::iterator iter(list.begin());
  // while (iter != list.end()) {
  //   if (ShouldErase(*iter)) {
  //     list.Erase(&iter);
  //   } else {
  //     ++iter;
  //   }
  // }
  //
  // This type does not make general guarantees of iterators staying valid on
  // operations --- only the iterator passed to Erase() will be fixed, not
  // any others; and Append operations should not be done concurrent with
  // iteration.
  class Iterator : public IterBase {
   public:
    Iterator& operator++() {
      this->Advance();
      return *this;
    }

    T* Get() { return this->Data(); }
    T* operator->() { return this->Data(); }
    T& operator*() { return *this->Data(); }
    bool operator==(const Iterator& other) const { return this->Equals(other); }
    bool operator!=(const Iterator& other) const {
      return !this->Equals(other);
    }
    // default copy op, dtor are OK.

   private:
    friend class InlineSList<T>;
    Iterator(const InlineSList<T>* list, T* prev) : IterBase(list, prev) {}
  };

  typedef Iterator iterator;

  // Read-only iterator type; cannot be used for deletion or to modify
  // the contained items.
  class ConstIterator : public IterBase {
   public:
    ConstIterator& operator++() {
      this->Advance();
      return *this;
    }

    const T* Get() { return this->Data(); }
    const T* operator->() { return this->Data(); }
    const T& operator*() { return *this->Data(); }
    bool operator==(const ConstIterator& other) const {
      return this->Equals(other);
    }
    bool operator!=(const ConstIterator& other) const {
      return !this->Equals(other);
    }
    // default copy op, dtor are OK.

   private:
    friend class InlineSList<T>;
    ConstIterator(const InlineSList<T>* list, T* prev) : IterBase(list, prev) {}
  };

  typedef ConstIterator const_iterator;

  InlineSList() : tail_(NULL) {
  }

  // The destructor deletes all the nodes in the list.
  ~InlineSList();

  bool IsEmpty() const {
    return (tail_ == NULL);
  }

  void Append(T* node);

  // Removes the item pointed to by the iterator, and updates the iterator
  // to point after it. Note that this means that it is now effectively
  // advanced (potentially past the end of the list) and that you should not
  // call ++ if you just want to consume one item.
  // See the iterator docs for example of proper use.
  void Erase(Iterator* iter);

  // Returns last item.
  T* Last() {
    DCHECK(!IsEmpty());
    return tail_;
  }

  const T* Last() const {
    DCHECK(!IsEmpty());
    return tail_;
  }

  // Iterator interface.

  // Note that all of these pass tail_ since iterator implementation internally
  // keeps track of the /previous/ node to the one pointed at.
  iterator begin() { return Iterator(this, tail_); }
  const_iterator begin() const { return ConstIterator(this, tail_); }

  // End iterators have their position at NULL.
  iterator end() { return Iterator(this, NULL); }
  const_iterator end() const { return ConstIterator(this, NULL); }

 private:
  // The representation we chose here is a circular linked list where we point
  // at the tail. This is because that permits us to append in O(1) to the end,
  // yet still have easy front-to-end traversal.
  T* tail_;

  DISALLOW_COPY_AND_ASSIGN(InlineSList);
};

template<class T>
inline InlineSList<T>::~InlineSList() {
  if (tail_ != NULL) {
    T* node = tail_->next();  // start at head node.
    while (true) {
      T* next = node->next();
      delete node;
      if (node == tail_) {  // stop when we deleted tail.
        break;
      } else {
        node = next;
      }
    }
  }
  tail_ = NULL;
}

template<class T>
inline void InlineSList<T>::Append(T* node) {
  if (tail_ == NULL) {
    tail_ = node;
    node->set_next(node);
  } else {
    node->set_next(tail_->next());
    tail_->set_next(node);
    tail_ = node;
  }
}

template<class T>
inline void InlineSList<T>::Erase(Iterator* iter) {
  DCHECK(!iter->AtEnd());

  T* iter_node = iter->node_;
  T* target_node = iter_node->next();

  if (iter_node == target_node) {
    // Only 1 element before the call, 0 now.
    tail_ = NULL;
    iter->node_ = NULL;
  } else {
    iter_node->set_next(target_node->next());
    if (target_node == tail_) {
      // Removed tail.. need to point it earlier.
      tail_ = iter_node;
      // Iterator is now one-past-end
      iter->node_ = NULL;
    }
  }
  delete target_node;
}

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_INLINE_SLIST_H_
