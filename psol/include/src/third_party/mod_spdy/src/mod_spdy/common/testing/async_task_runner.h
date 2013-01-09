// Copyright 2012 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MOD_SPDY_COMMON_TESTING_ASYNC_TASK_RUNNER_H_
#define MOD_SPDY_COMMON_TESTING_ASYNC_TASK_RUNNER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "mod_spdy/common/testing/notification.h"
#include "mod_spdy/common/thread_pool.h"

namespace mod_spdy {

class Executor;

namespace testing {

// An AsyncTaskRunner is a testing utility class to make it very easy to run a
// single piece of code concurrently, in order to write tests for concurrency
// features of other classes.  Standard usage is:
//
//   class FooTask : public AsyncTaskRunner::Task { ... };
//
//   AsyncTaskRunner runner(new FooTask(...));
//   ASSERT_TRUE(runner.Start());
//   ... stuff goes here ...
//   runner.notification()->ExpectSetWithin(...);  // or whatever
//
// Note that the implementation of this class is not particularly efficient,
// and is suitable only for testing purposes.
class AsyncTaskRunner {
 public:
  // A closure to be run by the AsyncTaskRunner.  If we had a simple closure
  // class available already, we'd use that instead.
  class Task {
   public:
    Task();
    virtual ~Task();
    virtual void Run() = 0;
   private:
    DISALLOW_COPY_AND_ASSIGN(Task);
  };

  // Construct an AsyncTaskRunner that will run the given task once started.
  // The AsyncTaskRunner gains ownership of the task.
  explicit AsyncTaskRunner(Task* task);

  ~AsyncTaskRunner();

  // Start the task running and return true.  If this fails, it returns false,
  // and the test should be aborted.
  bool Start();

  // Get the notification that will be set when the task completes.
  Notification* notification() { return &notification_; }

 private:
  const scoped_ptr<Task> task_;
  mod_spdy::ThreadPool thread_pool_;
  scoped_ptr<Executor> executor_;
  Notification notification_;

  DISALLOW_COPY_AND_ASSIGN(AsyncTaskRunner);
};

}  // namespace testing

}  // namespace mod_spdy

#endif  // MOD_SPDY_COMMON_TESTING_ASYNC_TASK_RUNNER_H_
