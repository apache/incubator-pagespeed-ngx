// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines a set of argument wrappers and related factory methods that
// can be used specify the refcounting and reference semantics of arguments
// that are bound by the Bind() function in base/bind.h.
//
// The public functions are base::Unretained() and base::ConstRef().
// Unretained() allows Bind() to bind a non-refcounted class.
// ConstRef() allows binding a constant reference to an argument rather
// than a copy.
//
//
// EXAMPLE OF Unretained():
//
//   class Foo {
//    public:
//     void func() { cout << "Foo:f" << endl;
//   };
//
//   // In some function somewhere.
//   Foo foo;
//   Callback<void(void)> foo_callback =
//       Bind(&Foo::func, Unretained(&foo));
//   foo_callback.Run();  // Prints "Foo:f".
//
// Without the Unretained() wrapper on |&foo|, the above call would fail
// to compile because Foo does not support the AddRef() and Release() methods.
//
//
// EXAMPLE OF ConstRef();
//   void foo(int arg) { cout << arg << endl }
//
//   int n = 1;
//   Callback<void(void)> no_ref = Bind(&foo, n);
//   Callback<void(void)> has_ref = Bind(&foo, ConstRef(n));
//
//   no_ref.Run();  // Prints "1"
//   has_ref.Run();  // Prints "1"
//
//   n = 2;
//   no_ref.Run();  // Prints "1"
//   has_ref.Run();  // Prints "2"
//
// Note that because ConstRef() takes a reference on |n|, |n| must outlive all
// its bound callbacks.
//

#ifndef BASE_BIND_HELPERS_H_
#define BASE_BIND_HELPERS_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/template_util.h"

namespace base {
namespace internal {

// Use the Substitution Failure Is Not An Error (SFINAE) trick to inspect T
// for the existence of AddRef() and Release() functions of the correct
// signature.
//
// http://en.wikipedia.org/wiki/Substitution_failure_is_not_an_error
// http://stackoverflow.com/questions/257288/is-it-possible-to-write-a-c-template-to-check-for-a-functions-existence
// http://stackoverflow.com/questions/4358584/sfinae-approach-comparison
// http://stackoverflow.com/questions/1966362/sfinae-to-check-for-inherited-member-functions
//
// The last link in particular show the method used below.
//
// For SFINAE to work with inherited methods, we need to pull some extra tricks
// with multiple inheritance.  In the more standard formulation, the overloads
// of Check would be:
//
//   template <typename C>
//   Yes NotTheCheckWeWant(Helper<&C::TargetFunc>*);
//
//   template <typename C>
//   No NotTheCheckWeWant(...);
//
//   static const bool value = sizeof(NotTheCheckWeWant<T>(0)) == sizeof(Yes);
//
// The problem here is that template resolution will not match
// C::TargetFunc if TargetFunc does not exist directly in C.  That is, if
// TargetFunc in inherited from an ancestor, &C::TargetFunc will not match,
// |value| will be false.  This formulation only checks for whether or
// not TargetFunc exist directly in the class being introspected.
//
// To get around this, we play a dirty trick with multiple inheritance.
// First, We create a class BaseMixin that declares each function that we
// want to probe for.  Then we create a class Base that inherits from both T
// (the class we wish to probe) and BaseMixin.  Note that the function
// signature in BaseMixin does not need to match the signature of the function
// we are probing for; thus it's easiest to just use void(void).
//
// Now, if TargetFunc exists somewhere in T, then &Base::TargetFunc has an
// ambiguous resolution between BaseMixin and T.  This lets us write the
// following:
//
//   template <typename C>
//   No GoodCheck(Helper<&C::TargetFunc>*);
//
//   template <typename C>
//   Yes GoodCheck(...);
//
//   static const bool value = sizeof(GoodCheck<Base>(0)) == sizeof(Yes);
//
// Notice here that the variadic version of GoodCheck() returns Yes here
// instead of No like the previous one. Also notice that we calculate |value|
// by specializing GoodCheck() on Base instead of T.
//
// We've reversed the roles of the variadic, and Helper overloads.
// GoodCheck(Helper<&C::TargetFunc>*), when C = Base, fails to be a valid
// substitution if T::TargetFunc exists. Thus GoodCheck<Base>(0) will resolve
// to the variadic version if T has TargetFunc.  If T::TargetFunc does not
// exist, then &C::TargetFunc is not ambiguous, and the overload resolution
// will prefer GoodCheck(Helper<&C::TargetFunc>*).
//
// This method of SFINAE will correctly probe for inherited names, but it cannot
// typecheck those names.  It's still a good enough sanity check though.
//
// Works on gcc-4.2, gcc-4.4, and Visual Studio 2008.
//
// TODO(ajwong): Move to ref_counted.h or template_util.h when we've vetted
// this works well.
//
// TODO(ajwong): Make this check for Release() as well.
// See http://crbug.com/82038.
template <typename T>
class SupportsAddRefAndRelease {
  typedef char Yes[1];
  typedef char No[2];

  struct BaseMixin {
    void AddRef();
  };

// MSVC warns when you try to use Base if T has a private destructor, the
// common pattern for refcounted types. It does this even though no attempt to
// instantiate Base is made.  We disable the warning for this definition.
#if defined(OS_WIN)
#pragma warning(disable:4624)
#endif
  struct Base : public T, public BaseMixin {
  };
#if defined(OS_WIN)
#pragma warning(default:4624)
#endif

  template <void(BaseMixin::*)(void)> struct Helper {};

  template <typename C>
  static No& Check(Helper<&C::AddRef>*);

  template <typename >
  static Yes& Check(...);

 public:
  static const bool value = sizeof(Check<Base>(0)) == sizeof(Yes);
};


// Helpers to assert that arguments of a recounted type are bound with a
// scoped_refptr.
template <bool IsClasstype, typename T>
struct UnsafeBindtoRefCountedArgHelper : false_type {
};

template <typename T>
struct UnsafeBindtoRefCountedArgHelper<true, T>
    : integral_constant<bool, SupportsAddRefAndRelease<T>::value> {
};

template <typename T>
struct UnsafeBindtoRefCountedArg : false_type {
};

template <typename T>
struct UnsafeBindtoRefCountedArg<T*>
    : UnsafeBindtoRefCountedArgHelper<is_class<T>::value, T> {
};


template <typename T>
class UnretainedWrapper {
 public:
  explicit UnretainedWrapper(T* o) : obj_(o) {}
  T* get() { return obj_; }
 private:
  T* obj_;
};

template <typename T>
class ConstRefWrapper {
 public:
  explicit ConstRefWrapper(const T& o) : ptr_(&o) {}
  const T& get() { return *ptr_; }
 private:
  const T* ptr_;
};


// Unwrap the stored parameters for the wrappers above.
template <typename T>
T Unwrap(T o) { return o; }

template <typename T>
T* Unwrap(UnretainedWrapper<T> unretained) { return unretained.get(); }

template <typename T>
const T& Unwrap(ConstRefWrapper<T> const_ref) {
  return const_ref.get();
}


// Utility for handling different refcounting semantics in the Bind()
// function.
template <typename IsMethod, typename T>
struct MaybeRefcount;

template <typename T>
struct MaybeRefcount<base::false_type, T> {
  static void AddRef(const T&) {}
  static void Release(const T&) {}
};

template <typename T, size_t n>
struct MaybeRefcount<base::false_type, T[n]> {
  static void AddRef(const T*) {}
  static void Release(const T*) {}
};

template <typename T>
struct MaybeRefcount<base::true_type, UnretainedWrapper<T> > {
  static void AddRef(const UnretainedWrapper<T>&) {}
  static void Release(const UnretainedWrapper<T>&) {}
};

template <typename T>
struct MaybeRefcount<base::true_type, T*> {
  static void AddRef(T* o) { o->AddRef(); }
  static void Release(T* o) { o->Release(); }
};

template <typename T>
struct MaybeRefcount<base::true_type, const T*> {
  static void AddRef(const T* o) { o->AddRef(); }
  static void Release(const T* o) { o->Release(); }
};

template <typename T>
struct MaybeRefcount<base::true_type, WeakPtr<T> > {
  static void AddRef(const WeakPtr<T>&) {}
  static void Release(const WeakPtr<T>&) {}
};

}  // namespace internal

template <typename T>
inline internal::UnretainedWrapper<T> Unretained(T* o) {
  return internal::UnretainedWrapper<T>(o);
}

template <typename T>
inline internal::ConstRefWrapper<T> ConstRef(const T& o) {
  return internal::ConstRefWrapper<T>(o);
}

}  // namespace base

#endif  // BASE_BIND_HELPERS_H_
