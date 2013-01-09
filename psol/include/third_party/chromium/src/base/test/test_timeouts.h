// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_TIMEOUTS_H_
#define BASE_TEST_TEST_TIMEOUTS_H_

#include "base/basictypes.h"
#include "base/logging.h"

// Returns common timeouts to use in tests. Makes it possible to adjust
// the timeouts for different environments (like Valgrind).
class TestTimeouts {
 public:
  // Initializes the timeouts. Non thread-safe. Should be called exactly once
  // by the test suite.
  static void Initialize();

  // Timeout for actions that are expected to finish "almost instantly".
  static int tiny_timeout_ms() {
    DCHECK(initialized_);
    return tiny_timeout_ms_;
  }

  // Timeout to wait for something to happen. If you are not sure
  // which timeout to use, this is the one you want.
  static int action_timeout_ms() {
    DCHECK(initialized_);
    return action_timeout_ms_;
  }

  // Timeout longer than the above, but still suitable to use
  // multiple times in a single test. Use if the timeout above
  // is not sufficient.
  static int action_max_timeout_ms() {
    DCHECK(initialized_);
    return action_max_timeout_ms_;
  }

  // Timeout for a large test that may take a few minutes to run.
  static int large_test_timeout_ms() {
    DCHECK(initialized_);
    return large_test_timeout_ms_;
  }

  // Timeout for a huge test (like running a layout test inside the browser).
  // Do not use unless absolutely necessary - try to make the test smaller.
  // Do not use multiple times in a single test.
  static int huge_test_timeout_ms() {
    DCHECK(initialized_);
    return huge_test_timeout_ms_;
  }

  // Timeout to wait for a live operation to complete. Used by tests that access
  // external services.
  static int live_operation_timeout_ms() {
    DCHECK(initialized_);
    return live_operation_timeout_ms_;
  }

 private:
  static bool initialized_;

  static int tiny_timeout_ms_;
  static int action_timeout_ms_;
  static int action_max_timeout_ms_;
  static int large_test_timeout_ms_;
  static int huge_test_timeout_ms_;
  static int live_operation_timeout_ms_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TestTimeouts);
};

#endif  // BASE_TEST_TEST_TIMEOUTS_H_
