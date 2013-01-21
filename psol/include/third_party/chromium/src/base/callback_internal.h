// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions and classes that help the
// implementation, and management of the Callback objects.

#ifndef BASE_CALLBACK_INTERNAL_H_
#define BASE_CALLBACK_INTERNAL_H_
#pragma once

#include <stddef.h>

#include "base/base_api.h"
#include "base/memory/ref_counted.h"

namespace base {
namespace internal {

// InvokerStorageBase is used to provide an opaque handle that the Callback
// class can use to represent a function object with bound arguments.  It
// behaves as an existential type that is used by a corresponding
// DoInvoke function to perform the function execution.  This allows
// us to shield the Callback class from the types of the bound argument via
// "type erasure."
class InvokerStorageBase : public RefCountedThreadSafe<InvokerStorageBase> {
 protected:
  friend class RefCountedThreadSafe<InvokerStorageBase>;
  virtual ~InvokerStorageBase() {}
};

// This structure exists purely to pass the returned |invoker_storage_| from
// Bind() to Callback while avoiding an extra AddRef/Release() pair.
//
// To do this, the constructor of Callback<> must take a const-ref.  The
// reference must be to a const object otherwise the compiler will emit a
// warning about taking a reference to a temporary.
//
// Unfortunately, this means that the internal |invoker_storage_| field must
// be made mutable.
template <typename T>
struct InvokerStorageHolder {
  explicit InvokerStorageHolder(T* invoker_storage)
      : invoker_storage_(invoker_storage) {
  }

  mutable scoped_refptr<InvokerStorageBase> invoker_storage_;
};

template <typename T>
InvokerStorageHolder<T> MakeInvokerStorageHolder(T* o) {
  return InvokerStorageHolder<T>(o);
}

// Holds the Callback methods that don't require specialization to reduce
// template bloat.
class BASE_API CallbackBase {
 public:
  // Returns true if Callback is null (doesn't refer to anything).
  bool is_null() const;

  // Returns the Callback into an uninitalized state.
  void Reset();

  bool Equals(const CallbackBase& other) const;

 protected:
  // In C++, it is safe to cast function pointers to function pointers of
  // another type. It is not okay to use void*. We create a InvokeFuncStorage
  // that that can store our function pointer, and then cast it back to
  // the original type on usage.
  typedef void(*InvokeFuncStorage)(void);

  CallbackBase(InvokeFuncStorage polymorphic_invoke,
               scoped_refptr<InvokerStorageBase>* invoker_storage);

  // Force the destructor to be instaniated inside this translation unit so
  // that our subclasses will not get inlined versions.  Avoids more template
  // bloat.
  ~CallbackBase();

  scoped_refptr<InvokerStorageBase> invoker_storage_;
  InvokeFuncStorage polymorphic_invoke_;
};

// This is a typetraits object that's used to take an argument type, and
// extract a suitable type for storing and forwarding arguments.
//
// In particular, it strips off references, and converts arrays to
// pointers for storage; and it avoids accidentally trying to create a
// "reference of a reference" if the argument is a reference type.
//
// This array type becomes an issue for storage because we are passing bound
// parameters by const reference. In this case, we end up passing an actual
// array type in the initializer list which C++ does not allow.  This will
// break passing of C-string literals.
template <typename T>
struct ParamTraits {
  typedef const T& ForwardType;
  typedef T StorageType;
};

// The Storage should almost be impossible to trigger unless someone manually
// specifies type of the bind parameters.  However, in case they do,
// this will guard against us accidentally storing a reference parameter.
//
// The ForwardType should only be used for unbound arguments.
template <typename T>
struct ParamTraits<T&> {
  typedef T& ForwardType;
  typedef T StorageType;
};

// Note that for array types, we implicitly add a const in the conversion. This
// means that it is not possible to bind array arguments to functions that take
// a non-const pointer. Trying to specialize the template based on a "const
// T[n]" does not seem to match correctly, so we are stuck with this
// restriction.
template <typename T, size_t n>
struct ParamTraits<T[n]> {
  typedef const T* ForwardType;
  typedef const T* StorageType;
};

// See comment for ParamTraits<T[n]>.
template <typename T>
struct ParamTraits<T[]> {
  typedef const T* ForwardType;
  typedef const T* StorageType;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_CALLBACK_INTERNAL_H_
