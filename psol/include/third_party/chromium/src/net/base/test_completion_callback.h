// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TEST_COMPLETION_CALLBACK_H_
#define NET_BASE_TEST_COMPLETION_CALLBACK_H_
#pragma once

#include "base/callback_old.h"
#include "base/tuple.h"

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
class TestCompletionCallback : public CallbackRunner< Tuple1<int> > {
 public:
  TestCompletionCallback();
  virtual ~TestCompletionCallback();

  int WaitForResult();

  int GetResult(int result);

  bool have_result() const { return have_result_; }

  virtual void RunWithParams(const Tuple1<int>& params);

 private:
  int result_;
  bool have_result_;
  bool waiting_for_result_;
};

#endif  // NET_BASE_TEST_COMPLETION_CALLBACK_H_
