// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_SUITE_H_
#define BASE_TEST_TEST_SUITE_H_
#pragma once

// Defines a basic test suite framework for running gtest based tests.  You can
// instantiate this class in your main function and call its Run method to run
// any gtest based tests that are linked into your executable.

#include <string>

#include "base/at_exit.h"

namespace testing {
class TestInfo;
}

namespace base {

class TestSuite {
 public:
  // Match function used by the GetTestCount method.
  typedef bool (*TestMatch)(const testing::TestInfo&);

  TestSuite(int argc, char** argv);
  virtual ~TestSuite();

  // Returns true if the test is marked as flaky.
  static bool IsMarkedFlaky(const testing::TestInfo& test);

  // Returns true if the test is marked as failing.
  static bool IsMarkedFailing(const testing::TestInfo& test);

  // Returns true if the test is marked as "MAYBE_".
  // When using different prefixes depending on platform, we use MAYBE_ and
  // preprocessor directives to replace MAYBE_ with the target prefix.
  static bool IsMarkedMaybe(const testing::TestInfo& test);

  // Returns true if the test failure should be ignored.
  static bool ShouldIgnoreFailure(const testing::TestInfo& test);

  // Returns true if the test failed and the failure shouldn't be ignored.
  static bool NonIgnoredFailures(const testing::TestInfo& test);

  // Returns the number of tests where the match function returns true.
  int GetTestCount(TestMatch test_match);

  void CatchMaybeTests();

  int Run();

  // A command-line flag that makes a test failure always result in a non-zero
  // process exit code.
  static const char kStrictFailureHandling[];

 protected:
  // By default fatal log messages (e.g. from DCHECKs) result in error dialogs
  // which gum up buildbots. Use a minimalistic assert handler which just
  // terminates the process.
  static void UnitTestAssertHandler(const std::string& str);

  // Disable crash dialogs so that it doesn't gum up the buildbot
  virtual void SuppressErrorDialogs();

  // Override these for custom initialization and shutdown handling.  Use these
  // instead of putting complex code in your constructor/destructor.

  virtual void Initialize();
  virtual void Shutdown();

  // Make sure that we setup an AtExitManager so Singleton objects will be
  // destroyed.
  base::AtExitManager at_exit_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestSuite);
};

}  // namespace base

// TODO(brettw) remove this. This is a temporary hack to allow WebKit to compile
// until we can update it to use "base::" (preventing a two-sided patch).
using base::TestSuite;

#endif  // BASE_TEST_TEST_SUITE_H_
