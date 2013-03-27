// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_MULTIPROCESS_TEST_H_
#define BASE_TEST_MULTIPROCESS_TEST_H_

#include <string>

#include "base/basictypes.h"
#include "base/process.h"
#include "base/process_util.h"
#include "build/build_config.h"
#include "testing/platform_test.h"

class CommandLine;

namespace base {

// A MultiProcessTest is a test class which makes it easier to
// write a test which requires code running out of process.
//
// To create a multiprocess test simply follow these steps:
//
// 1) Derive your test from MultiProcessTest. Example:
//
//    class MyTest : public MultiProcessTest {
//    };
//
//    TEST_F(MyTest, TestCaseName) {
//      ...
//    }
//
// 2) Create a mainline function for the child processes and include
//    testing/multiprocess_func_list.h.
//    See the declaration of the MULTIPROCESS_TEST_MAIN macro
//    in that file for an example.
// 3) Call SpawnChild("foo"), where "foo" is the name of
//    the function you wish to run in the child processes.
// That's it!
class MultiProcessTest : public PlatformTest {
 public:
  MultiProcessTest();

 protected:
  // Run a child process.
  // 'procname' is the name of a function which the child will
  // execute.  It must be exported from this library in order to
  // run.
  //
  // Example signature:
  //    extern "C" int __declspec(dllexport) FooBar() {
  //         // do client work here
  //    }
  //
  // Returns the handle to the child, or NULL on failure
  ProcessHandle SpawnChild(const std::string& procname, bool debug_on_start);

#if defined(OS_POSIX)
  // TODO(evan): see if we can delete this via more refactoring.
  // SpawnChild() should just take a base::LaunchOptions so that we don't
  // need multiple versions of it.
  ProcessHandle SpawnChild(const std::string& procname,
                           const FileHandleMappingVector& fds_to_map,
                           bool debug_on_start);
#endif

  // Set up the command line used to spawn the child process.
  virtual CommandLine MakeCmdLine(const std::string& procname,
                                  bool debug_on_start);

 private:
  // Shared implementation of SpawnChild.
  // TODO: |fds_to_map| is unused on Windows; see above TODO about
  // further refactoring.
  ProcessHandle SpawnChildImpl(const std::string& procname,
                               const FileHandleMappingVector& fds_to_map,
                               bool debug_on_start);

  DISALLOW_COPY_AND_ASSIGN(MultiProcessTest);
};

}  // namespace base

#endif  // BASE_TEST_MULTIPROCESS_TEST_H_
