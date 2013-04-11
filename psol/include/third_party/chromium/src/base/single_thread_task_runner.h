// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SINGLE_THREAD_TASK_RUNNER_H_
#define BASE_SINGLE_THREAD_TASK_RUNNER_H_

#include "base/base_export.h"
#include "base/sequenced_task_runner.h"

namespace base {

// A SingleThreadTaskRunner is a SequencedTaskRunner with one more
// guarantee; namely, that all tasks are run on a single dedicated
// thread.  Most use cases require only a SequencedTaskRunner, unless
// there is a specific need to run tasks on only a single dedicated.
//
// Some theoretical implementations of SingleThreadTaskRunner:
//
//   - A SingleThreadTaskRunner that uses a single worker thread to
//     run posted tasks (i.e., a message loop).
//
//   - A SingleThreadTaskRunner that stores the list of posted tasks
//     and has a method Run() that runs each runnable task in FIFO
//     order that must be run only from the thread the
//     SingleThreadTaskRunner was created on.
class BASE_EXPORT SingleThreadTaskRunner : public SequencedTaskRunner {
 public:
  // A more explicit alias to RunsTasksOnCurrentThread().
  bool BelongsToCurrentThread() const {
    return RunsTasksOnCurrentThread();
  }

 protected:
  virtual ~SingleThreadTaskRunner() {}
};

}  // namespace base

#endif  // BASE_SERIAL_TASK_RUNNER_H_
