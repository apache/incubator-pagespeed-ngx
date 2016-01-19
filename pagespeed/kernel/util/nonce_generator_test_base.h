// Copyright 2013 Google Inc. All Rights Reserved.
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

// Author: jmaessen@google.com (Jan-Willem Maessen)

#ifndef PAGESPEED_KERNEL_UTIL_NONCE_GENERATOR_TEST_BASE_H_
#define PAGESPEED_KERNEL_UTIL_NONCE_GENERATOR_TEST_BASE_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/util/nonce_generator.h"

namespace net_instaweb {

// Number of iterations of nonce generation to check in tests.
extern const int kNumIterations;
extern const int kSmallNumIterations;

class NonceGeneratorTestBase : public testing::Test {
 protected:
  NonceGeneratorTestBase() { }
  ~NonceGeneratorTestBase();

  void DuplicateFreedom();
  void DifferentNonOverlap();
  void AllBitsUsed();

  scoped_ptr<NonceGenerator> main_generator_;
  scoped_ptr<NonceGenerator> other_generator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonceGeneratorTestBase);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_NONCE_GENERATOR_TEST_BASE_H_
