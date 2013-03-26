// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SCOPED_NSOBJECT_H_
#define BASE_MEMORY_SCOPED_NSOBJECT_H_

#import <Foundation/Foundation.h>
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_policy.h"

// scoped_nsobject<> is patterned after scoped_ptr<>, but maintains ownership
// of an NSObject subclass object.  Style deviations here are solely for
// compatibility with scoped_ptr<>'s interface, with which everyone is already
// familiar.
//
// By default, scoped_nsobject<> takes ownership of an object (in the
// constructor or in reset()) by taking over the caller's existing ownership
// claim.  The caller must own the object it gives to scoped_nsobject<>, and
// relinquishes an ownership claim to that object.  scoped_nsobject<> does not
// call -retain. This behavior is parametrized by the |OwnershipPolicy| enum.
// If the value |RETAIN| is passed (in the constructor or in reset()), then
// scoped_nsobject<> will call -retain on the object, and the initial
// ownership is not changed.
//
// scoped_nsprotocol<> has the same behavior as scoped_nsobject, but can be used
// with protocols.
//
// scoped_nsobject<> is not to be used for NSAutoreleasePools. For
// NSAutoreleasePools use ScopedNSAutoreleasePool from
// scoped_nsautorelease_pool.h instead.
// We check for bad uses of scoped_nsobject and NSAutoreleasePool at compile
// time with a template specialization (see below).

template<typename NST>
class scoped_nsprotocol {
 public:
  explicit scoped_nsprotocol(
      NST object = nil,
      base::scoped_policy::OwnershipPolicy policy = base::scoped_policy::ASSUME)
      : object_(object) {
    if (policy == base::scoped_policy::RETAIN)
      [object retain];
  }

  scoped_nsprotocol(const scoped_nsprotocol<NST>& that)
      : object_([that.object_ retain]) {
  }

  ~scoped_nsprotocol() {
    [object_ release];
  }

  scoped_nsprotocol& operator=(const scoped_nsprotocol<NST>& that) {
    reset(that.get(), base::scoped_policy::RETAIN);
    return *this;
  }

  void reset(NST object = nil,
             base::scoped_policy::OwnershipPolicy policy =
                base::scoped_policy::ASSUME) {
    if (policy == base::scoped_policy::RETAIN)
      [object retain];
    // We intentionally do not check that object != object_ as the caller must
    // either already have an ownership claim over whatever it passes to this
    // method, or call it with the |RETAIN| policy which will have ensured that
    // the object is retained once more when reaching this point.
    [object_ release];
    object_ = object;
  }

  bool operator==(NST that) const { return object_ == that; }
  bool operator!=(NST that) const { return object_ != that; }

  operator NST() const {
    return object_;
  }

  NST get() const {
    return object_;
  }

  void swap(scoped_nsprotocol& that) {
    NST temp = that.object_;
    that.object_ = object_;
    object_ = temp;
  }

  // scoped_nsprotocol<>::release() is like scoped_ptr<>::release.  It is NOT a
  // wrapper for [object_ release].  To force a scoped_nsprotocol<> to call
  // [object_ release], use scoped_nsprotocol<>::reset().
  NST release() WARN_UNUSED_RESULT {
    NST temp = object_;
    object_ = nil;
    return temp;
  }

 private:
  NST object_;
};

// Free functions
template <class C>
void swap(scoped_nsprotocol<C>& p1, scoped_nsprotocol<C>& p2) {
  p1.swap(p2);
}

template <class C>
bool operator==(C p1, const scoped_nsprotocol<C>& p2) {
  return p1 == p2.get();
}

template <class C>
bool operator!=(C p1, const scoped_nsprotocol<C>& p2) {
  return p1 != p2.get();
}

template<typename NST>
class scoped_nsobject : public scoped_nsprotocol<NST*> {
 public:
  explicit scoped_nsobject(
      NST* object = nil,
      base::scoped_policy::OwnershipPolicy policy = base::scoped_policy::ASSUME)
      : scoped_nsprotocol<NST*>(object, policy) {
  }

  scoped_nsobject(const scoped_nsobject<NST>& that)
      : scoped_nsprotocol<NST*>(that) {
  }

  scoped_nsobject& operator=(const scoped_nsobject<NST>& that) {
    scoped_nsprotocol<NST*>::operator=(that);
    return *this;
  }
};

// Specialization to make scoped_nsobject<id> work.
template<>
class scoped_nsobject<id> : public scoped_nsprotocol<id> {
 public:
  explicit scoped_nsobject(
      id object = nil,
      base::scoped_policy::OwnershipPolicy policy = base::scoped_policy::ASSUME)
      : scoped_nsprotocol<id>(object, policy) {
  }

  scoped_nsobject(const scoped_nsobject<id>& that)
      : scoped_nsprotocol<id>(that) {
  }

  scoped_nsobject& operator=(const scoped_nsobject<id>& that) {
    scoped_nsprotocol<id>::operator=(that);
    return *this;
  }
};

// Do not use scoped_nsobject for NSAutoreleasePools, use
// ScopedNSAutoreleasePool instead. This is a compile time check. See details
// at top of header.
template<>
class scoped_nsobject<NSAutoreleasePool> {
 private:
  explicit scoped_nsobject(NSAutoreleasePool* object = nil,
                           base::scoped_policy::OwnershipPolicy policy =
                               base::scoped_policy::ASSUME);
  DISALLOW_COPY_AND_ASSIGN(scoped_nsobject);
};
#endif  // BASE_MEMORY_SCOPED_NSOBJECT_H_
