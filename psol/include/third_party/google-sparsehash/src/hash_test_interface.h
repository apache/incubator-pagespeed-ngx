// Copyright (c) 2010, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ----
// Author: Craig Silverstein
//
// This implements a uniform interface for all 6 hash implementations:
//    dense_hashtable, dense_hash_map, dense_hash_set
//    sparse_hashtable, sparse_hash_map, sparse_hash_set
// This is intended to be used for testing, to provide a single routine
// that can easily test all 6 implementations.
//
// The main reasons to specialize are to (1) provide dummy
// implementations for methods that are only needed for some of the
// implementations (for instance, set_empty_key()), and (2) provide a
// uniform interface to just the keys -- for instance, we provide
// wrappers around the iterators that define it.key, which gives the
// "key" part of the bucket (*it or it->first, depending on the class).

#ifndef UTIL_GTL_HASH_TEST_INTERFACE_H_
#define UTIL_GTL_HASH_TEST_INTERFACE_H_

#include "config.h"
#include HASH_MAP_H    // for hash<>
#include <functional>  // for equal_to<>
#include <google/sparsehash/sparsehashtable.h>
#include <google/sparse_hash_map>
#include <google/sparse_hash_set>
#include <google/sparsehash/densehashtable.h>
#include <google/dense_hash_map>
#include <google/dense_hash_set>

_START_GOOGLE_NAMESPACE_

// This is the "default" interface, which just passes everything
// through to the underlying hashtable.  You'll need to subclass it to
// specialize behavior for an individual hashtable.
template <class HT>
class BaseHashtableInterface {
 public:
  virtual ~BaseHashtableInterface() {}

  typedef typename HT::key_type key_type;
  typedef typename HT::value_type value_type;
  typedef typename HT::hasher hasher;
  typedef typename HT::key_equal key_equal;
  typedef typename HT::allocator_type allocator_type;

  typedef typename HT::size_type size_type;
  typedef typename HT::difference_type difference_type;
  typedef typename HT::pointer pointer;
  typedef typename HT::const_pointer const_pointer;
  typedef typename HT::reference reference;
  typedef typename HT::const_reference const_reference;

  class const_iterator;

  class iterator : public HT::iterator {
   public:
    iterator() : parent_(NULL) { }   // this allows code like "iterator it;"
    iterator(typename HT::iterator it,
             const BaseHashtableInterface* parent)
        : HT::iterator(it), parent_(parent) { }
    key_type key() { return parent_->it_to_key(*this); }
   private:
    friend class BaseHashtableInterface::const_iterator;  // for its ctor
    const BaseHashtableInterface* parent_;
  };

  class const_iterator : public HT::const_iterator {
   public:
    const_iterator() : parent_(NULL) { }
    const_iterator(typename HT::const_iterator it,
                   const BaseHashtableInterface* parent)
        : HT::const_iterator(it), parent_(parent) { }
    const_iterator(typename HT::iterator it,
                   BaseHashtableInterface* parent)
        : HT::const_iterator(it), parent_(parent) { }
    // The parameter type here *should* just be "iterator", but MSVC
    // gets confused by that, so I'm overly specific.
    const_iterator(typename BaseHashtableInterface<HT>::iterator it)
        : HT::const_iterator(it), parent_(it.parent_) { }
    key_type key() { return parent_->it_to_key(*this); }
   private:
    const BaseHashtableInterface* parent_;
  };

  class const_local_iterator;

  class local_iterator : public HT::local_iterator {
   public:
    local_iterator() : parent_(NULL) { }
    local_iterator(typename HT::local_iterator it,
                   const BaseHashtableInterface* parent)
        : HT::local_iterator(it), parent_(parent) { }
    key_type key() { return parent_->it_to_key(*this); }
   private:
    friend class BaseHashtableInterface::const_local_iterator;  // for its ctor
    const BaseHashtableInterface* parent_;
  };

  class const_local_iterator : public HT::const_local_iterator {
   public:
    const_local_iterator() : parent_(NULL) { }
    const_local_iterator(typename HT::const_local_iterator it,
                         const BaseHashtableInterface* parent)
        : HT::const_local_iterator(it), parent_(parent) { }
    const_local_iterator(typename HT::local_iterator it,
                         BaseHashtableInterface* parent)
        : HT::const_local_iterator(it), parent_(parent) { }
    const_local_iterator(local_iterator it)
        : HT::const_local_iterator(it), parent_(it.parent_) { }
    key_type key() { return parent_->it_to_key(*this); }
   private:
    const BaseHashtableInterface* parent_;
  };

  iterator begin() {
    return iterator(ht_.begin(), this);
  }
  iterator end() {
    return iterator(ht_.end(), this);
  }
  const_iterator begin() const {
    return const_iterator(ht_.begin(), this);
  }
  const_iterator end() const   {
    return const_iterator(ht_.end(), this);
  }
  local_iterator begin(size_type i) {
    return local_iterator(ht_.begin(i), this);
  }
  local_iterator end(size_type i) {
    return local_iterator(ht_.end(i), this);
  }
  const_local_iterator begin(size_type i) const  {
    return const_local_iterator(ht_.begin(i), this);
  }
  const_local_iterator end(size_type i) const    {
    return const_local_iterator(ht_.end(i), this);
  }

  hasher hash_funct() const { return ht_.hash_funct(); }
  hasher hash_function() const { return ht_.hash_function(); }
  key_equal key_eq() const { return ht_.key_eq(); }
  allocator_type get_allocator() const { return ht_.get_allocator(); }

  BaseHashtableInterface(size_type expected_max_items_in_table,
                         const hasher& hf,
                         const key_equal& eql,
                         const allocator_type& alloc)
      : ht_(expected_max_items_in_table, hf, eql, alloc) { }

  // Not all ht_'s support this constructor: you should only call it
  // from a subclass if you know your ht supports it.  Otherwise call
  // the previous constructor, followed by 'insert(f, l);'.
  template <class InputIterator>
  BaseHashtableInterface(InputIterator f, InputIterator l,
                         size_type expected_max_items_in_table,
                         const hasher& hf,
                         const key_equal& eql,
                         const allocator_type& alloc)
      : ht_(f, l, expected_max_items_in_table, hf, eql, alloc) {
  }

  // This is the version of the constructor used by dense_*, which
  // requires an empty key in the constructor.
  template <class InputIterator>
  BaseHashtableInterface(InputIterator f, InputIterator l, key_type empty_k,
                         size_type expected_max_items_in_table,
                         const hasher& hf,
                         const key_equal& eql,
                         const allocator_type& alloc)
      : ht_(f, l, empty_k, expected_max_items_in_table, hf, eql, alloc) {
  }

  // This is the constructor appropriate for {dense,sparse}hashtable.
  template <class ExtractKey, class SetKey>
  BaseHashtableInterface(size_type expected_max_items_in_table,
                         const hasher& hf,
                         const key_equal& eql,
                         const ExtractKey& ek,
                         const SetKey& sk,
                         const allocator_type& alloc)
      : ht_(expected_max_items_in_table, hf, eql, ek, sk, alloc) { }


  void clear() { ht_.clear(); }
  void swap(BaseHashtableInterface& other) { ht_.swap(other.ht_); }

  // Only part of the API for some hashtable implementations.
  void clear_no_resize() { clear(); }

  size_type size() const             { return ht_.size(); }
  size_type max_size() const         { return ht_.max_size(); }
  bool empty() const                 { return ht_.empty(); }
  size_type bucket_count() const     { return ht_.bucket_count(); }
  size_type max_bucket_count() const { return ht_.max_bucket_count(); }

  size_type bucket_size(size_type i) const {
    return ht_.bucket_size(i);
  }
  size_type bucket(const key_type& key) const {
    return ht_.bucket(key);
  }

  float load_factor() const { return ht_.load_factor(); }
  float max_load_factor() const { return ht_.max_load_factor(); }
  void max_load_factor(float grow) { ht_.max_load_factor(grow); }
  float min_load_factor() const { return ht_.min_load_factor(); }
  void min_load_factor(float shrink) { ht_.min_load_factor(shrink); }
  void set_resizing_parameters(float shrink, float grow) {
    ht_.set_resizing_parameters(shrink, grow);
  }

  void resize(size_type hint)    { ht_.resize(hint); }
  void rehash(size_type hint)    { ht_.rehash(hint); }

  iterator find(const key_type& key) {
    return iterator(ht_.find(key), this);
  }
  const_iterator find(const key_type& key) const {
    return const_iterator(ht_.find(key), this);
  }

  // Rather than try to implement operator[], which doesn't make much
  // sense for set types, we implement two methods: bracket_equal and
  // bracket_assign.  By default, bracket_equal(a, b) returns true if
  // ht[a] == b, and false otherwise.  (Note that this follows
  // operator[] semantics exactly, including inserting a if it's not
  // already in the hashtable, before doing the equality test.)  For
  // sets, which have no operator[], b is ignored, and bracket_equal
  // returns true if key is in the set and false otherwise.
  // bracket_assign(a, b) is equivalent to ht[a] = b.  For sets, b is
  // ignored, and bracket_assign is equivalent to ht.insert(a).
  template<typename AssignValue>
  bool bracket_equal(const key_type& key, const AssignValue& expected) {
    return ht_[key] == expected;
  }

  template<typename AssignValue>
  void bracket_assign(const key_type& key, const AssignValue& value) {
    ht_[key] = value;
  }

  size_type count(const key_type& key) const { return ht_.count(key); }

  pair<iterator, iterator> equal_range(const key_type& key) {
    pair<typename HT::iterator, typename HT::iterator> r
        = ht_.equal_range(key);
    return pair<iterator, iterator>(iterator(r.first, this),
                                    iterator(r.second, this));
  }
  pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
    pair<typename HT::const_iterator, typename HT::const_iterator> r
        = ht_.equal_range(key);
    return pair<const_iterator, const_iterator>(const_iterator(r.first, this),
                                                const_iterator(r.second, this));
  }

  const_iterator random_element(class ACMRandom* r) const {
    return const_iterator(ht_.random_element(r), this);
  }
  iterator random_element(class ACMRandom* r)  {
    return iterator(ht_.random_element(r), this);
  }

  pair<iterator, bool> insert(const value_type& obj) {
    pair<typename HT::iterator, bool> r = ht_.insert(obj);
    return pair<iterator, bool>(iterator(r.first, this), r.second);
  }
  template <class InputIterator>
  void insert(InputIterator f, InputIterator l) {
    ht_.insert(f, l);
  }
  void insert(typename HT::const_iterator f, typename HT::const_iterator l) {
    ht_.insert(f, l);
  }
  iterator insert(typename HT::iterator, const value_type& obj) {
    return iterator(insert(obj).first, this);
  }

  // These will commonly need to be overridden by the child.
  void set_empty_key(const key_type& k) { ht_.set_empty_key(k); }
  void clear_empty_key() { ht_.clear_empty_key(); }
  key_type empty_key() const { return ht_.empty_key(); }

  void set_deleted_key(const key_type& k) { ht_.set_deleted_key(k); }
  void clear_deleted_key() { ht_.clear_deleted_key(); }
  key_type deleted_key() const { return ht_.deleted_key(); }

  size_type erase(const key_type& key)   { return ht_.erase(key); }
  void erase(typename HT::iterator it)   { ht_.erase(it); }
  void erase(typename HT::iterator f, typename HT::iterator l) {
    ht_.erase(f, l);
  }

  bool operator==(const BaseHashtableInterface& other) const {
    return ht_ == other.ht_;
  }
  bool operator!=(const BaseHashtableInterface& other) const {
    return ht_ != other.ht_;
  }

  template <typename OUTPUT>
  bool write_metadata(OUTPUT *fp) {
    return ht_.write_metadata(fp);
  }
  template <typename INPUT>
  bool read_metadata(INPUT *fp) {
    return ht_.read_metadata(fp);
  }
  template <typename OUTPUT>
  bool write_nopointer_data(OUTPUT *fp) {
    return ht_.write_nopointer_data(fp);
  }
  template <typename INPUT>
  bool read_nopointer_data(INPUT *fp) {
    return ht_.read_nopointer_data(fp);
  }

  // low-level stats
  int num_table_copies() const { return ht_.num_table_copies(); }

  // Not part of the hashtable API, but is provided to make testing easier.
  virtual key_type get_key(const value_type& value) const = 0;
  // All subclasses should define get_data(value_type) as well.  I don't
  // provide an abstract-virtual definition here, because the return type
  // differs between subclasses (not all subclasses define data_type).
  //virtual data_type get_data(const value_type& value) const = 0;
  //virtual data_type default_data() const = 0;

  // These allow introspection into the interface.  "Supports" means
  // that the implementation of this functionality isn't a noop.
  virtual bool supports_clear_no_resize() const = 0;
  virtual bool supports_empty_key() const = 0;
  virtual bool supports_deleted_key() const = 0;
  virtual bool supports_brackets() const = 0;     // has a 'real' operator[]
  virtual bool supports_readwrite() const = 0;
  virtual bool supports_num_table_copies() const = 0;

 protected:
  HT ht_;

  // These are what subclasses have to define to get class-specific behavior
  virtual key_type it_to_key(const iterator& it) const = 0;
  virtual key_type it_to_key(const const_iterator& it) const = 0;
  virtual key_type it_to_key(const local_iterator& it) const = 0;
  virtual key_type it_to_key(const const_local_iterator& it) const = 0;
};

// ---------------------------------------------------------------------

template <class Key, class T,
          class HashFcn = HASH_NAMESPACE::hash<Key>,
          class EqualKey = STL_NAMESPACE::equal_to<Key>,
          class Alloc = libc_allocator_with_realloc<pair<const Key, T> > >
class HashtableInterface_SparseHashMap
    : public BaseHashtableInterface< sparse_hash_map<Key, T, HashFcn,
                                                     EqualKey, Alloc> > {
 private:
  typedef sparse_hash_map<Key, T, HashFcn, EqualKey, Alloc> ht;
  typedef BaseHashtableInterface<ht> p;  // parent

 public:
  explicit HashtableInterface_SparseHashMap(
      typename p::size_type expected_max_items = 0,
      const typename p::hasher& hf = typename p::hasher(),
      const typename p::key_equal& eql = typename p::key_equal(),
      const typename p::allocator_type& alloc = typename p::allocator_type())
      : BaseHashtableInterface<ht>(expected_max_items, hf, eql, alloc) { }

  template <class InputIterator>
  HashtableInterface_SparseHashMap(
      InputIterator f, InputIterator l,
      typename p::size_type expected_max_items = 0,
      const typename p::hasher& hf = typename p::hasher(),
      const typename p::key_equal& eql = typename p::key_equal(),
      const typename p::allocator_type& alloc = typename p::allocator_type())
      : BaseHashtableInterface<ht>(f, l, expected_max_items, hf, eql, alloc) { }

  typename p::key_type get_key(const typename p::value_type& value) const {
    return value.first;
  }
  typename ht::data_type get_data(const typename p::value_type& value) const {
    return value.second;
  }
  typename ht::data_type default_data() const {
    return typename ht::data_type();
  }

  bool supports_clear_no_resize() const { return false; }
  bool supports_empty_key() const { return false; }
  bool supports_deleted_key() const { return true; }
  bool supports_brackets() const { return true; }
  bool supports_readwrite() const { return true; }
  bool supports_num_table_copies() const { return false; }

  void set_empty_key(const typename p::key_type& k) { }
  void clear_empty_key() { }
  typename p::key_type empty_key() const { return typename p::key_type(); }
  int num_table_copies() const { return 0; }

 protected:
  template <class K2, class T2, class H2, class E2, class A2>
  friend void swap(HashtableInterface_SparseHashMap<K2,T2,H2,E2,A2>& a,
                   HashtableInterface_SparseHashMap<K2,T2,H2,E2,A2>& b);

  typename p::key_type it_to_key(const typename p::iterator& it) const {
    return it->first;
  }
  typename p::key_type it_to_key(const typename p::const_iterator& it) const {
    return it->first;
  }
  typename p::key_type it_to_key(const typename p::local_iterator& it) const {
    return it->first;
  }
  typename p::key_type it_to_key(const typename p::const_local_iterator& it)
      const {
    return it->first;
  }
};

template <class K, class T, class H, class E, class A>
void swap(HashtableInterface_SparseHashMap<K,T,H,E,A>& a,
          HashtableInterface_SparseHashMap<K,T,H,E,A>& b) {
  swap(a.ht_, b.ht_);
}

// ---------------------------------------------------------------------

template <class Value,
          class HashFcn = HASH_NAMESPACE::hash<Value>,
          class EqualKey = STL_NAMESPACE::equal_to<Value>,
          class Alloc = libc_allocator_with_realloc<Value> >
class HashtableInterface_SparseHashSet
    : public BaseHashtableInterface< sparse_hash_set<Value, HashFcn,
                                                     EqualKey, Alloc> > {
 private:
  typedef sparse_hash_set<Value, HashFcn, EqualKey, Alloc> ht;
  typedef BaseHashtableInterface<ht> p;  // parent

 public:
  // Bizarrely, MSVC 8.0 has trouble with the (perfectly fine)
  // typename's in this constructor, and this constructor alone, out
  // of all the ones in the file.  So for MSVC, we take some typenames
  // out, which is technically invalid C++, but MSVC doesn't seem to
  // mind.
#ifdef _MSC_VER
  explicit HashtableInterface_SparseHashSet(
      typename p::size_type expected_max_items = 0,
      const typename p::hasher& hf = p::hasher(),
      const typename p::key_equal& eql = p::key_equal(),
      const typename p::allocator_type& alloc = p::allocator_type())
      : BaseHashtableInterface<ht>(expected_max_items, hf, eql, alloc) { }
#else
  explicit HashtableInterface_SparseHashSet(
      typename p::size_type expected_max_items = 0,
      const typename p::hasher& hf = typename p::hasher(),
      const typename p::key_equal& eql = typename p::key_equal(),
      const typename p::allocator_type& alloc = typename p::allocator_type())
      : BaseHashtableInterface<ht>(expected_max_items, hf, eql, alloc) { }
#endif

  template <class InputIterator>
  HashtableInterface_SparseHashSet(
      InputIterator f, InputIterator l,
      typename p::size_type expected_max_items = 0,
      const typename p::hasher& hf = typename p::hasher(),
      const typename p::key_equal& eql = typename p::key_equal(),
      const typename p::allocator_type& alloc = typename p::allocator_type())
      : BaseHashtableInterface<ht>(f, l, expected_max_items, hf, eql, alloc) { }

  template<typename AssignValue>
  bool bracket_equal(const typename p::key_type& key, const AssignValue&) {
    return this->ht_.find(key) != this->ht_.end();
  }

  template<typename AssignValue>
  void bracket_assign(const typename p::key_type& key, const AssignValue&) {
    this->ht_.insert(key);
  }

  typename p::key_type get_key(const typename p::value_type& value) const {
    return value;
  }
  // For sets, the only 'data' is that an item is actually inserted.
  bool get_data(const typename p::value_type&) const {
    return true;
  }
  bool default_data() const {
    return true;
  }

  bool supports_clear_no_resize() const { return false; }
  bool supports_empty_key() const { return false; }
  bool supports_deleted_key() const { return true; }
  bool supports_brackets() const { return false; }
  bool supports_readwrite() const { return true; }
  bool supports_num_table_copies() const { return false; }

  void set_empty_key(const typename p::key_type& k) { }
  void clear_empty_key() { }
  typename p::key_type empty_key() const { return typename p::key_type(); }
  int num_table_copies() const { return 0; }

 protected:
  template <class K2, class H2, class E2, class A2>
  friend void swap(HashtableInterface_SparseHashSet<K2,H2,E2,A2>& a,
                   HashtableInterface_SparseHashSet<K2,H2,E2,A2>& b);

  typename p::key_type it_to_key(const typename p::iterator& it) const {
    return *it;
  }
  typename p::key_type it_to_key(const typename p::const_iterator& it) const {
    return *it;
  }
  typename p::key_type it_to_key(const typename p::local_iterator& it) const {
    return *it;
  }
  typename p::key_type it_to_key(const typename p::const_local_iterator& it)
      const {
    return *it;
  }
};

template <class K, class H, class E, class A>
void swap(HashtableInterface_SparseHashSet<K,H,E,A>& a,
          HashtableInterface_SparseHashSet<K,H,E,A>& b) {
  swap(a.ht_, b.ht_);
}

// ---------------------------------------------------------------------

template <class Value, class Key, class HashFcn, class ExtractKey,
          class SetKey, class EqualKey, class Alloc>
class HashtableInterface_SparseHashtable
    : public BaseHashtableInterface< sparse_hashtable<Value, Key, HashFcn,
                                                      ExtractKey, SetKey,
                                                      EqualKey, Alloc> > {
 private:
  typedef sparse_hashtable<Value, Key, HashFcn, ExtractKey, SetKey,
                           EqualKey, Alloc> ht;
  typedef BaseHashtableInterface<ht> p;  // parent

 public:
  explicit HashtableInterface_SparseHashtable(
      typename p::size_type expected_max_items = 0,
      const typename p::hasher& hf = typename p::hasher(),
      const typename p::key_equal& eql = typename p::key_equal(),
      const typename p::allocator_type& alloc = typename p::allocator_type())
      : BaseHashtableInterface<ht>(expected_max_items, hf, eql,
                                   ExtractKey(), SetKey(), alloc) { }

  template <class InputIterator>
  HashtableInterface_SparseHashtable(
      InputIterator f, InputIterator l,
      typename p::size_type expected_max_items = 0,
      const typename p::hasher& hf = typename p::hasher(),
      const typename p::key_equal& eql = typename p::key_equal(),
      const typename p::allocator_type& alloc = typename p::allocator_type())
      : BaseHashtableInterface<ht>(expected_max_items, hf, eql,
                                   ExtractKey(), SetKey(), alloc) {
    this->insert(f, l);
  }

  float max_load_factor() const {
    float shrink, grow;
    this->ht_.get_resizing_parameters(&shrink, &grow);
    return grow;
  }
  void max_load_factor(float new_grow) {
    float shrink, grow;
    this->ht_.get_resizing_parameters(&shrink, &grow);
    this->ht_.set_resizing_parameters(shrink, new_grow);
  }
  float min_load_factor() const {
    float shrink, grow;
    this->ht_.get_resizing_parameters(&shrink, &grow);
    return shrink;
  }
  void min_load_factor(float new_shrink) {
    float shrink, grow;
    this->ht_.get_resizing_parameters(&shrink, &grow);
    this->ht_.set_resizing_parameters(new_shrink, grow);
  }

  template<typename AssignValue>
  bool bracket_equal(const typename p::key_type&, const AssignValue&) {
    return false;
  }

  template<typename AssignValue>
  void bracket_assign(const typename p::key_type&, const AssignValue&) {
  }

  typename p::key_type get_key(const typename p::value_type& value) const {
    return extract_key(value);
  }
  typename p::value_type get_data(const typename p::value_type& value) const {
    return value;
  }
  typename p::value_type default_data() const {
    return typename p::value_type();
  }

  bool supports_clear_no_resize() const { return false; }
  bool supports_empty_key() const { return false; }
  bool supports_deleted_key() const { return true; }
  bool supports_brackets() const { return false; }
  bool supports_readwrite() const { return true; }
  bool supports_num_table_copies() const { return true; }

  void set_empty_key(const typename p::key_type& k) { }
  void clear_empty_key() { }
  typename p::key_type empty_key() const { return typename p::key_type(); }

  // These tr1 names aren't defined for sparse_hashtable.
  typename p::hasher hash_function() { return this->hash_funct(); }
  void rehash(typename p::size_type hint) { this->resize(hint); }

  // TODO(csilvers): also support/test destructive_begin()/destructive_end()?

 protected:
  template <class V2, class K2, class HF2, class EK2, class SK2, class Eq2,
            class A2>
  friend void swap(
      HashtableInterface_SparseHashtable<V2,K2,HF2,EK2,SK2,Eq2,A2>& a,
      HashtableInterface_SparseHashtable<V2,K2,HF2,EK2,SK2,Eq2,A2>& b);

  typename p::key_type it_to_key(const typename p::iterator& it) const {
    return extract_key(*it);
  }
  typename p::key_type it_to_key(const typename p::const_iterator& it) const {
    return extract_key(*it);
  }
  typename p::key_type it_to_key(const typename p::local_iterator& it) const {
    return extract_key(*it);
  }
  typename p::key_type it_to_key(const typename p::const_local_iterator& it)
      const {
    return extract_key(*it);
  }

 private:
  ExtractKey extract_key;
};

template <class V, class K, class HF, class EK, class SK, class Eq, class A>
void swap(HashtableInterface_SparseHashtable<V,K,HF,EK,SK,Eq,A>& a,
          HashtableInterface_SparseHashtable<V,K,HF,EK,SK,Eq,A>& b) {
  swap(a.ht_, b.ht_);
}

// ---------------------------------------------------------------------

// Unlike dense_hash_map, the wrapper class takes an extra template
// value saying what the empty key is.

template <class Key, class T, const Key& EMPTY_KEY,
          class HashFcn = HASH_NAMESPACE::hash<Key>,
          class EqualKey = STL_NAMESPACE::equal_to<Key>,
          class Alloc = libc_allocator_with_realloc<pair<const Key, T> > >
class HashtableInterface_DenseHashMap
    : public BaseHashtableInterface< dense_hash_map<Key, T, HashFcn,
                                                    EqualKey, Alloc> > {
 private:
  typedef dense_hash_map<Key, T, HashFcn, EqualKey, Alloc> ht;
  typedef BaseHashtableInterface<ht> p;  // parent

 public:
  explicit HashtableInterface_DenseHashMap(
      typename p::size_type expected_max_items = 0,
      const typename p::hasher& hf = typename p::hasher(),
      const typename p::key_equal& eql = typename p::key_equal(),
      const typename p::allocator_type& alloc = typename p::allocator_type())
      : BaseHashtableInterface<ht>(expected_max_items, hf, eql, alloc) {
    this->set_empty_key(EMPTY_KEY);
  }

  template <class InputIterator>
  HashtableInterface_DenseHashMap(
      InputIterator f, InputIterator l,
      typename p::size_type expected_max_items = 0,
      const typename p::hasher& hf = typename p::hasher(),
      const typename p::key_equal& eql = typename p::key_equal(),
      const typename p::allocator_type& alloc = typename p::allocator_type())
      : BaseHashtableInterface<ht>(f, l, EMPTY_KEY,
                                   expected_max_items, hf, eql, alloc) {
  }

  void clear_no_resize() { this->ht_.clear_no_resize(); }

  typename p::key_type get_key(const typename p::value_type& value) const {
    return value.first;
  }
  typename ht::data_type get_data(const typename p::value_type& value) const {
    return value.second;
  }
  typename ht::data_type default_data() const {
    return typename ht::data_type();
  }

  bool supports_clear_no_resize() const { return true; }
  bool supports_empty_key() const { return true; }
  bool supports_deleted_key() const { return true; }
  bool supports_brackets() const { return true; }
  bool supports_readwrite() const { return false; }
  bool supports_num_table_copies() const { return false; }

  template <typename OUTPUT> bool write_metadata(OUTPUT *fp) { return false; }
  template <typename INPUT> bool read_metadata(INPUT *fp) { return false; }
  template <typename OUTPUT> bool write_nopointer_data(OUTPUT *) {
    return false;
  }
  template <typename INPUT> bool read_nopointer_data(INPUT *) {
    return false;
  }
  int num_table_copies() const { return 0; }

 protected:
  template <class K2, class T2, const K2& Empty2, class H2, class E2, class A2>
  friend void swap(HashtableInterface_DenseHashMap<K2,T2,Empty2,H2,E2,A2>& a,
                   HashtableInterface_DenseHashMap<K2,T2,Empty2,H2,E2,A2>& b);

  typename p::key_type it_to_key(const typename p::iterator& it) const {
    return it->first;
  }
  typename p::key_type it_to_key(const typename p::const_iterator& it) const {
    return it->first;
  }
  typename p::key_type it_to_key(const typename p::local_iterator& it) const {
    return it->first;
  }
  typename p::key_type it_to_key(const typename p::const_local_iterator& it)
      const {
    return it->first;
  }
};

template <class K, class T, const K& Empty, class H, class E, class A>
void swap(HashtableInterface_DenseHashMap<K,T,Empty,H,E,A>& a,
          HashtableInterface_DenseHashMap<K,T,Empty,H,E,A>& b) {
  swap(a.ht_, b.ht_);
}

// ---------------------------------------------------------------------

// Unlike dense_hash_set, the wrapper class takes an extra template
// value saying what the empty key is.

template <class Value, const Value& EMPTY_KEY,
          class HashFcn = HASH_NAMESPACE::hash<Value>,
          class EqualKey = STL_NAMESPACE::equal_to<Value>,
          class Alloc = libc_allocator_with_realloc<Value> >
class HashtableInterface_DenseHashSet
    : public BaseHashtableInterface< dense_hash_set<Value, HashFcn,
                                                     EqualKey, Alloc> > {
 private:
  typedef dense_hash_set<Value, HashFcn, EqualKey, Alloc> ht;
  typedef BaseHashtableInterface<ht> p;  // parent

 public:
  explicit HashtableInterface_DenseHashSet(
      typename p::size_type expected_max_items = 0,
      const typename p::hasher& hf = typename p::hasher(),
      const typename p::key_equal& eql = typename p::key_equal(),
      const typename p::allocator_type& alloc = typename p::allocator_type())
      : BaseHashtableInterface<ht>(expected_max_items, hf, eql, alloc) {
    this->set_empty_key(EMPTY_KEY);
  }

  template <class InputIterator>
  HashtableInterface_DenseHashSet(
      InputIterator f, InputIterator l,
      typename p::size_type expected_max_items = 0,
      const typename p::hasher& hf = typename p::hasher(),
      const typename p::key_equal& eql = typename p::key_equal(),
      const typename p::allocator_type& alloc = typename p::allocator_type())
      : BaseHashtableInterface<ht>(f, l, EMPTY_KEY,
                                   expected_max_items, hf, eql, alloc) {
  }

  void clear_no_resize() { this->ht_.clear_no_resize(); }

  template<typename AssignValue>
  bool bracket_equal(const typename p::key_type& key, const AssignValue&) {
    return this->ht_.find(key) != this->ht_.end();
  }

  template<typename AssignValue>
  void bracket_assign(const typename p::key_type& key, const AssignValue&) {
    this->ht_.insert(key);
  }

  typename p::key_type get_key(const typename p::value_type& value) const {
    return value;
  }
  bool get_data(const typename p::value_type&) const {
    return true;
  }
  bool default_data() const {
    return true;
  }

  bool supports_clear_no_resize() const { return true; }
  bool supports_empty_key() const { return true; }
  bool supports_deleted_key() const { return true; }
  bool supports_brackets() const { return false; }
  bool supports_readwrite() const { return false; }
  bool supports_num_table_copies() const { return false; }

  template <typename OUTPUT> bool write_metadata(OUTPUT *fp) { return false; }
  template <typename INPUT> bool read_metadata(INPUT *fp) { return false; }
  template <typename OUTPUT> bool write_nopointer_data(OUTPUT *) {
    return false;
  }
  template <typename INPUT> bool read_nopointer_data(INPUT *) {
    return false;
  }
  int num_table_copies() const { return 0; }

 protected:
  template <class K2, const K2& Empty2, class H2, class E2, class A2>
  friend void swap(HashtableInterface_DenseHashSet<K2,Empty2,H2,E2,A2>& a,
                   HashtableInterface_DenseHashSet<K2,Empty2,H2,E2,A2>& b);

  typename p::key_type it_to_key(const typename p::iterator& it) const {
    return *it;
  }
  typename p::key_type it_to_key(const typename p::const_iterator& it) const {
    return *it;
  }
  typename p::key_type it_to_key(const typename p::local_iterator& it) const {
    return *it;
  }
  typename p::key_type it_to_key(const typename p::const_local_iterator& it)
      const {
    return *it;
  }
};

template <class K, const K& Empty, class H, class E, class A>
void swap(HashtableInterface_DenseHashSet<K,Empty,H,E,A>& a,
          HashtableInterface_DenseHashSet<K,Empty,H,E,A>& b) {
  swap(a.ht_, b.ht_);
}

// ---------------------------------------------------------------------

// Unlike dense_hashtable, the wrapper class takes an extra template
// value saying what the empty key is.

template <class Value, class Key, const Key& EMPTY_KEY, class HashFcn,
          class ExtractKey, class SetKey, class EqualKey, class Alloc>
class HashtableInterface_DenseHashtable
    : public BaseHashtableInterface< dense_hashtable<Value, Key, HashFcn,
                                                     ExtractKey, SetKey,
                                                     EqualKey, Alloc> > {
 private:
  typedef dense_hashtable<Value, Key, HashFcn, ExtractKey, SetKey,
                           EqualKey, Alloc> ht;
  typedef BaseHashtableInterface<ht> p;  // parent

 public:
  explicit HashtableInterface_DenseHashtable(
      typename p::size_type expected_max_items = 0,
      const typename p::hasher& hf = typename p::hasher(),
      const typename p::key_equal& eql = typename p::key_equal(),
      const typename p::allocator_type& alloc = typename p::allocator_type())
      : BaseHashtableInterface<ht>(expected_max_items, hf, eql,
                                   ExtractKey(), SetKey(), alloc) {
    this->set_empty_key(EMPTY_KEY);
  }

  template <class InputIterator>
  HashtableInterface_DenseHashtable(
      InputIterator f, InputIterator l,
      typename p::size_type expected_max_items = 0,
      const typename p::hasher& hf = typename p::hasher(),
      const typename p::key_equal& eql = typename p::key_equal(),
      const typename p::allocator_type& alloc = typename p::allocator_type())
      : BaseHashtableInterface<ht>(expected_max_items, hf, eql,
                                   ExtractKey(), SetKey(), alloc) {
    this->set_empty_key(EMPTY_KEY);
    this->insert(f, l);
  }

  void clear_no_resize() { this->ht_.clear_no_resize(); }

  float max_load_factor() const {
    float shrink, grow;
    this->ht_.get_resizing_parameters(&shrink, &grow);
    return grow;
  }
  void max_load_factor(float new_grow) {
    float shrink, grow;
    this->ht_.get_resizing_parameters(&shrink, &grow);
    this->ht_.set_resizing_parameters(shrink, new_grow);
  }
  float min_load_factor() const {
    float shrink, grow;
    this->ht_.get_resizing_parameters(&shrink, &grow);
    return shrink;
  }
  void min_load_factor(float new_shrink) {
    float shrink, grow;
    this->ht_.get_resizing_parameters(&shrink, &grow);
    this->ht_.set_resizing_parameters(new_shrink, grow);
  }

  template<typename AssignValue>
  bool bracket_equal(const typename p::key_type&, const AssignValue&) {
    return false;
  }

  template<typename AssignValue>
  void bracket_assign(const typename p::key_type&, const AssignValue&) {
  }

  typename p::key_type get_key(const typename p::value_type& value) const {
    return extract_key(value);
  }
  typename p::value_type get_data(const typename p::value_type& value) const {
    return value;
  }
  typename p::value_type default_data() const {
    return typename p::value_type();
  }

  bool supports_clear_no_resize() const { return true; }
  bool supports_empty_key() const { return true; }
  bool supports_deleted_key() const { return true; }
  bool supports_brackets() const { return false; }
  bool supports_readwrite() const { return false; }
  bool supports_num_table_copies() const { return true; }

  template <typename OUTPUT> bool write_metadata(OUTPUT *fp) { return false; }
  template <typename INPUT> bool read_metadata(INPUT *fp) { return false; }
  template <typename OUTPUT> bool write_nopointer_data(OUTPUT *) {
    return false;
  }
  template <typename INPUT> bool read_nopointer_data(INPUT *) {
    return false;
  }

  // These tr1 names aren't defined for dense_hashtable.
  typename p::hasher hash_function() { return this->hash_funct(); }
  void rehash(typename p::size_type hint) { this->resize(hint); }

 protected:
  template <class V2, class K2, const K2& Empty2,
            class HF2, class EK2, class SK2, class Eq2, class A2>
  friend void swap(
      HashtableInterface_DenseHashtable<V2,K2,Empty2,HF2,EK2,SK2,Eq2,A2>& a,
      HashtableInterface_DenseHashtable<V2,K2,Empty2,HF2,EK2,SK2,Eq2,A2>& b);

  typename p::key_type it_to_key(const typename p::iterator& it) const {
    return extract_key(*it);
  }
  typename p::key_type it_to_key(const typename p::const_iterator& it) const {
    return extract_key(*it);
  }
  typename p::key_type it_to_key(const typename p::local_iterator& it) const {
    return extract_key(*it);
  }
  typename p::key_type it_to_key(const typename p::const_local_iterator& it)
      const {
    return extract_key(*it);
  }

 private:
  ExtractKey extract_key;
};

template <class V, class K, const K& Empty,
          class HF, class EK, class SK, class Eq, class A>
void swap(HashtableInterface_DenseHashtable<V,K,Empty,HF,EK,SK,Eq,A>& a,
          HashtableInterface_DenseHashtable<V,K,Empty,HF,EK,SK,Eq,A>& b) {
  swap(a.ht_, b.ht_);
}

_END_GOOGLE_NAMESPACE_

#endif  // UTIL_GTL_HASH_TEST_INTERFACE_H_
