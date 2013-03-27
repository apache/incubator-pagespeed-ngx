// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_RUNNER_UTIL_H_
#define BASE_TASK_RUNNER_UTIL_H_

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_internal.h"
#include "base/logging.h"
#include "base/task_runner.h"

namespace base {

namespace internal {

// Helper class for TaskRunner::PostTaskAndReplyWithResult.
template <typename ReturnType>
void ReturnAsParamAdapter(const Callback<ReturnType(void)>& func,
                          ReturnType* result) {
  if (!func.is_null())
    *result = func.Run();
}

// Helper class for TaskRunner::PostTaskAndReplyWithResult.
template <typename ReturnType>
Closure ReturnAsParam(const Callback<ReturnType(void)>& func,
                      ReturnType* result) {
  DCHECK(result);
  return Bind(&ReturnAsParamAdapter<ReturnType>, func, result);
}

// Helper class for TaskRunner::PostTaskAndReplyWithResult.
template <typename ReturnType>
void ReplyAdapter(const Callback<void(ReturnType)>& callback,
                  ReturnType* result) {
  DCHECK(result);
  if(!callback.is_null())
    callback.Run(CallbackForward(*result));
}

// Helper class for TaskRunner::PostTaskAndReplyWithResult.
template <typename ReturnType, typename OwnedType>
Closure ReplyHelper(const Callback<void(ReturnType)>& callback,
                    OwnedType result) {
  return Bind(&ReplyAdapter<ReturnType>, callback, result);
}

}  // namespace internal

// When you have these methods
//
//   R DoWorkAndReturn();
//   void Callback(const R& result);
//
// and want to call them in a PostTaskAndReply kind of fashion where the
// result of DoWorkAndReturn is passed to the Callback, you can use
// PostTaskAndReplyWithResult as in this example:
//
// PostTaskAndReplyWithResult(
//     target_thread_.message_loop_proxy(),
//     FROM_HERE,
//     Bind(&DoWorkAndReturn),
//     Bind(&Callback));
template <typename ReturnType>
bool PostTaskAndReplyWithResult(
    TaskRunner* task_runner,
    const tracked_objects::Location& from_here,
    const Callback<ReturnType(void)>& task,
    const Callback<void(ReturnType)>& reply) {
  ReturnType* result = new ReturnType;
  return task_runner->PostTaskAndReply(
      from_here,
      internal::ReturnAsParam<ReturnType>(task, result),
      internal::ReplyHelper(reply, Owned(result)));
}

}  // namespace base

#endif  // BASE_TASK_RUNNER_UTIL_H_
