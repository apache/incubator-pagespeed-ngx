// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_THREAD_CHECKER_H_
#define BASE_THREADING_THREAD_CHECKER_H_
#pragma once

#ifndef NDEBUG
#include "base/threading/thread_checker_impl.h"
#endif

namespace base {

// Do nothing implementation, for use in release mode.
//
// Note: You should almost always use the ThreadChecker class to get the
// right version for your build configuration.
class ThreadCheckerDoNothing {
 public:
  bool CalledOnValidThread() const {
    return true;
  }

  void DetachFromThread() {}
};

// Before using this class, please consider using NonThreadSafe as it
// makes it much easier to determine the nature of your class.
//
// ThreadChecker is a helper class used to help verify that some methods of a
// class are called from the same thread.  One can inherit from this class and
// use CalledOnValidThread() to verify.
//
// Inheriting from class indicates that one must be careful when using the
// class with multiple threads. However, it is up to the class document to
// indicate how it can be used with threads.
//
// Example:
// class MyClass : public ThreadChecker {
//  public:
//   void Foo() {
//     DCHECK(CalledOnValidThread());
//     ... (do stuff) ...
//   }
// }
//
// In Release mode, CalledOnValidThread will always return true.
#ifndef NDEBUG
class ThreadChecker : public ThreadCheckerImpl {
};
#else
class ThreadChecker : public ThreadCheckerDoNothing {
};
#endif  // NDEBUG

}  // namespace base

#endif  // BASE_THREADING_THREAD_CHECKER_H_
