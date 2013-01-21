// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_QUEUE_H_
#define BASE_TASK_QUEUE_H_
#pragma once

#include <deque>

#include "base/base_api.h"
#include "base/task.h"

// A TaskQueue is a queue of tasks waiting to be run.  To run the tasks, call
// the Run method.  A task queue is itself a Task so that it can be placed in a
// message loop or another task queue.
class BASE_API TaskQueue : public Task {
 public:
  TaskQueue();
  virtual ~TaskQueue();

  // Push the specified task onto the queue.  When the queue is run, the tasks
  // will be run in the order they are pushed.
  //
  // This method takes ownership of |task| and will delete task after it is run
  // (or when the TaskQueue is destroyed, if we never got a chance to run it).
  void Push(Task* task);

  // Remove all tasks from the queue.  The tasks are deleted.
  void Clear();

  // Returns true if this queue contains no tasks.
  bool IsEmpty() const;

  // Run all the tasks in the queue.  New tasks pushed onto the queue during
  // a run will be run next time |Run| is called.
  virtual void Run();

 private:
   // The list of tasks we are waiting to run.
   std::deque<Task*> queue_;
};

#endif  // BASE_TASK_QUEUE_H_
