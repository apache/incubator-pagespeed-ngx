// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREAD_TASK_RUNNER_HANDLE_H_
#define BASE_THREAD_TASK_RUNNER_HANDLE_H_

#include "base/base_export.h"
#include "base/memory/ref_counted.h"

namespace base {

class SingleThreadTaskRunner;

// ThreadTaskRunnerHandle stores a reference to the main task runner
// for each thread. Not more than one of these objects can be created
// per thread. After an instance of this object is created the Get()
// function will return the |task_runner| specified in the
// constructor.
class BASE_EXPORT ThreadTaskRunnerHandle {
 public:
  // Gets the SingleThreadTaskRunner for the current thread.
  static scoped_refptr<SingleThreadTaskRunner> Get();

  ThreadTaskRunnerHandle(
      const scoped_refptr<SingleThreadTaskRunner>& task_runner);
  ~ThreadTaskRunnerHandle();

 private:
  scoped_refptr<SingleThreadTaskRunner> task_runner_;
};

}  // namespace base

#endif  // BASE_THREAD_TASK_RUNNER_HANDLE_H_
