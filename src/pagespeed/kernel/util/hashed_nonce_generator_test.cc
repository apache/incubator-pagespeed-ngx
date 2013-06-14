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

#include "pagespeed/kernel/util/hashed_nonce_generator.h"

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/md5_hasher.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/util/nonce_generator.h"
#include "pagespeed/kernel/util/nonce_generator_test_base.h"

namespace net_instaweb {
namespace {

const char kMainGeneratorKey[] = "main hashed nonce generator test key";
const char kOtherGeneratorKey[] = "other hashed nonce generator test key";

class HashedNonceGeneratorTest : public NonceGeneratorTestBase {
 protected:
  HashedNonceGeneratorTest() {
    main_generator_.reset(new HashedNonceGenerator(
        &hasher_, kMainGeneratorKey, new NullMutex));
    duplicate_generator_.reset(new HashedNonceGenerator(
        &hasher_, kMainGeneratorKey, new NullMutex));
    other_generator_.reset(new HashedNonceGenerator(
        &hasher_, kOtherGeneratorKey, new NullMutex));
  }

  MD5Hasher hasher_;
  scoped_ptr<NonceGenerator> duplicate_generator_;
};

TEST_F(HashedNonceGeneratorTest, DuplicateFreedom) {
  DuplicateFreedom();
}

// Show that two identically-constructed generators will produce duplicate
// results.  Not a requirement, but an important caveat for this generator.
TEST_F(HashedNonceGeneratorTest, DuplicateGeneration) {
  for (int i = 0; i < kNumIterations; ++i) {
    uint64 nonce = main_generator_->NewNonce();
    EXPECT_EQ(nonce, duplicate_generator_->NewNonce());
  }
}

TEST_F(HashedNonceGeneratorTest, DifferentNonOverlap) {
  DifferentNonOverlap();
}

// Show that all bits change state at some point reasonably early in the
// generation of nonces.  We do this by making sure we see a 0 and a 1 in each
// bit position.  We do that by setting bits in a pair of bit masks for each 0
// and 1 found.  When we're done, any remaining clear bits correspond to bit
// positions that didn't change.
TEST_F(HashedNonceGeneratorTest, AllBitsUsed) {
  AllBitsUsed();
}

}  // namespace
}  // namespace net_instaweb
