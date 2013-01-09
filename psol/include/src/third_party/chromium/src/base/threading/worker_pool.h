// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_WORKER_POOL_H_
#define BASE_THREADING_WORKER_POOL_H_
#pragma once

#include "base/base_api.h"
#include "base/tracked.h"

class Task;

namespace base {

// This is a facility that runs tasks that don't require a specific thread or
// a message loop.
//
// WARNING: This shouldn't be used unless absolutely necessary. We don't wait
// for the worker pool threads to finish on shutdown, so the tasks running
// inside the pool must be extremely careful about other objects they access
// (MessageLoops, Singletons, etc). During shutdown these object may no longer
// exist.
class BASE_API WorkerPool {
 public:
  // This function posts |task| to run on a worker thread.  |task_is_slow|
  // should be used for tasks that will take a long time to execute.  Returns
  // false if |task| could not be posted to a worker thread.  Regardless of
  // return value, ownership of |task| is transferred to the worker pool.
  static bool PostTask(const tracked_objects::Location& from_here,
                       Task* task, bool task_is_slow);
};

}  // namespace base

#endif  // BASE_THREADING_WORKER_POOL_H_
