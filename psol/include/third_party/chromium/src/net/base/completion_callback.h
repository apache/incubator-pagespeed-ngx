// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_COMPLETION_CALLBACK_H__
#define NET_BASE_COMPLETION_CALLBACK_H__
#pragma once

#include "base/callback_old.h"

namespace net {

// A callback specialization that takes a single int parameter.  Usually this
// is used to report a byte count or network error code.
typedef Callback1<int>::Type CompletionCallback;

// Used to implement a CompletionCallback.
template <class T>
class CompletionCallbackImpl :
    public CallbackImpl< T, void (T::*)(int), Tuple1<int> > {
 public:
  CompletionCallbackImpl(T* obj, void (T::* meth)(int))
    : CallbackImpl< T, void (T::*)(int),
                    Tuple1<int> >::CallbackImpl(obj, meth) {
  }
};

// CancelableCompletionCallback is used for completion callbacks
// which may outlive the target for the method dispatch. In such a case, the
// provider of the callback calls Cancel() to mark the callback as
// "canceled". When the canceled callback is eventually run it does nothing
// other than to decrement the refcount to 0 and free the memory.
template <class T>
class CancelableCompletionCallback :
    public CompletionCallbackImpl<T>,
    public base::RefCounted<CancelableCompletionCallback<T> > {
 public:
  CancelableCompletionCallback(T* obj, void (T::* meth)(int))
    : CompletionCallbackImpl<T>(obj, meth), is_canceled_(false) {
  }

  void Cancel() {
    is_canceled_ = true;
  }

  virtual void RunWithParams(const Tuple1<int>& params) {
    if (is_canceled_) {
      base::RefCounted<CancelableCompletionCallback<T> >::Release();
    } else {
      CompletionCallbackImpl<T>::RunWithParams(params);
    }
  }

 private:
  bool is_canceled_;
};

}  // namespace net

#endif  // NET_BASE_COMPLETION_CALLBACK_H__
