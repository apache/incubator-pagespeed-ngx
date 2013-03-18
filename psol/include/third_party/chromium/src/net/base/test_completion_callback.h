// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TEST_COMPLETION_CALLBACK_H_
#define NET_BASE_TEST_COMPLETION_CALLBACK_H_

#include "base/compiler_specific.h"
#include "base/tuple.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"

//-----------------------------------------------------------------------------
// completion callback helper

// A helper class for completion callbacks, designed to make it easy to run
// tests involving asynchronous operations.  Just call WaitForResult to wait
// for the asynchronous operation to complete.
//
// NOTE: Since this runs a message loop to wait for the completion callback,
// there could be other side-effects resulting from WaitForResult.  For this
// reason, this class is probably not ideal for a general application.
//

namespace net {

namespace internal {

class TestCompletionCallbackBaseInternal {
 public:
  bool have_result() const { return have_result_; }

 protected:
  TestCompletionCallbackBaseInternal();
  void DidSetResult();
  void WaitForResult();

  bool have_result_;
  bool waiting_for_result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCompletionCallbackBaseInternal);
};

template <typename R>
class TestCompletionCallbackTemplate
    : public TestCompletionCallbackBaseInternal {
 public:
  virtual ~TestCompletionCallbackTemplate() {}

  R WaitForResult() {
    TestCompletionCallbackBaseInternal::WaitForResult();
    return result_;
  }

  R GetResult(R result) {
    if (net::ERR_IO_PENDING != result)
      return result;
    return WaitForResult();
  }

 protected:
  // Override this method to gain control as the callback is running.
  virtual void SetResult(R result) {
    result_ = result;
    DidSetResult();
  }

  TestCompletionCallbackTemplate() : result_(R()) {}
  R result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCompletionCallbackTemplate);
};

}  // namespace internal

// Base class overridden by custom implementations of TestCompletionCallback.
typedef internal::TestCompletionCallbackTemplate<int>
    TestCompletionCallbackBase;

typedef internal::TestCompletionCallbackTemplate<int64>
    TestInt64CompletionCallbackBase;

class TestCompletionCallback : public TestCompletionCallbackBase {
 public:
  TestCompletionCallback();
  virtual ~TestCompletionCallback();

  const CompletionCallback& callback() const { return callback_; }

 private:
  const CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(TestCompletionCallback);
};

class TestInt64CompletionCallback : public TestInt64CompletionCallbackBase {
 public:
  TestInt64CompletionCallback();
  virtual ~TestInt64CompletionCallback();

  const Int64CompletionCallback& callback() const { return callback_; }

 private:
  const Int64CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(TestInt64CompletionCallback);
};

}  // namespace net

#endif  // NET_BASE_TEST_COMPLETION_CALLBACK_H_
