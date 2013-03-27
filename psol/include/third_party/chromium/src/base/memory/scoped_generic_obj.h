// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SCOPED_GENERIC_OBJ_H_
#define BASE_MEMORY_SCOPED_GENERIC_OBJ_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"

// ScopedGenericObj<> is patterned after scoped_ptr_malloc<>, except
// that it assumes the template argument is typedef'ed to a pointer
// type. It does not support retain/release semantics. It takes as its
// second template argument a functor which frees the object.
//
// Example (Mac-specific):
//
// class ScopedDestroyRendererInfo {
//  public:
//   void operator()(CGLRendererInfoObj x) const {
//     CGLDestroyRendererInfo(x);
//   }
// };
//
// ...
//
//   CGLRendererInfoObj renderer_info = NULL;
//   ...
//   ScopedGenericObj<CGLRendererInfoObj, ScopedDestroyRendererInfo>
//       scoper(renderer_info);

template<class C, class FreeProc>
class ScopedGenericObj {
 public:

  // The element type
  typedef C element_type;

  // Constructor.  Defaults to initializing with NULL.
  // There is no way to create an uninitialized ScopedGenericObj.
  // The input parameter must be allocated with an allocator that matches the
  // Free functor.
  explicit ScopedGenericObj(C p = C()): obj_(p) {}

  // Destructor.  If there is a C object, call the Free functor.
  ~ScopedGenericObj() {
    reset();
  }

  // Reset.  Calls the Free functor on the current owned object, if any.
  // Then takes ownership of a new object, if given.
  // this->reset(this->get()) works.
  void reset(C p = C()) {
    if (obj_ != p) {
      FreeProc free_proc;
      free_proc(obj_);
      obj_ = p;
    }
  }

  operator C() const {
    return obj_;
  }

  C get() const {
    return obj_;
  }

  // Comparison operators.
  // These return whether a ScopedGenericObj and a plain pointer refer
  // to the same object, not just to two different but equal objects.
  // For compatibility with the boost-derived implementation, these
  // take non-const arguments.
  bool operator==(C p) const {
    return obj_ == p;
  }

  bool operator!=(C p) const {
    return obj_ != p;
  }

  // Swap two ScopedGenericObjs.
  void swap(ScopedGenericObj& b) {
    C tmp = b.obj_;
    b.obj_ = obj_;
    obj_ = tmp;
  }

  // Release a pointer.
  // The return value is the current pointer held by this object.
  // If this object holds a NULL pointer, the return value is NULL.
  // After this operation, this object will hold a NULL pointer,
  // and will not own the object any more.
  C release() WARN_UNUSED_RESULT {
    C tmp = obj_;
    obj_ = NULL;
    return tmp;
  }

 private:
  C obj_;

  // no reason to use these: each ScopedGenericObj should have its own object.
  template <class C2, class GP>
  bool operator==(ScopedGenericObj<C2, GP> const& p) const;
  template <class C2, class GP>
  bool operator!=(ScopedGenericObj<C2, GP> const& p) const;

  // Disallow evil constructors.
  ScopedGenericObj(const ScopedGenericObj&);
  void operator=(const ScopedGenericObj&);
};

template<class C, class FP> inline
void swap(ScopedGenericObj<C, FP>& a, ScopedGenericObj<C, FP>& b) {
  a.swap(b);
}

template<class C, class FP> inline
bool operator==(C* p, const ScopedGenericObj<C, FP>& b) {
  return p == b.get();
}

template<class C, class FP> inline
bool operator!=(C* p, const ScopedGenericObj<C, FP>& b) {
  return p != b.get();
}

#endif  // BASE_MEMORY_SCOPED_GENERIC_OBJ_H_
