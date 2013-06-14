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

#include "pagespeed/kernel/util/nonce_generator_test_base.h"

#include <set>
#include <utility>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/util/nonce_generator.h"


namespace net_instaweb {

// Number of iterations of nonce generation to check.
const int kNumIterations = 10000;
const int kSmallNumIterations = 100;

NonceGeneratorTestBase::~NonceGeneratorTestBase() {
}

// Make sure we don't see duplicates in first segment of results.  The intent
// here is to catch coding errors that cause trivial duplication.
void NonceGeneratorTestBase::DuplicateFreedom() {
  std::set<uint64> seen;
  for (int i = 0; i < kNumIterations; ++i) {
    uint64 nonce = main_generator_->NewNonce();
    EXPECT_TRUE(seen.insert(nonce).second) << nonce << " duplicated";
  }
}

// Show that generators with distinct keys produce different results.  Given a
// uint64 result, the probability of a collision in kNumIterations is small (but
// only birthday-paradox small).  We're mostly looking for coding errors where
// one generator has the same outputs as the other, but skewed forwards or
// backwards, but we check for all kinds of overlap.
void NonceGeneratorTestBase::DifferentNonOverlap() {
  std::set<uint64> seen_main;
  std::set<uint64> seen_other;
  for (int i = 0; i < kNumIterations; ++i) {
    uint64 nonce_main = main_generator_->NewNonce();
    seen_main.insert(nonce_main);
    uint64 nonce_other = other_generator_->NewNonce();
    seen_other.insert(nonce_other);
    EXPECT_EQ(seen_main.end(), seen_main.find(nonce_other)) <<
        nonce_other << " collides with main generator";
    EXPECT_EQ(seen_other.end(), seen_other.find(nonce_main)) <<
        nonce_main << " collides with other generator";
  }
}

// Show that all bits change state at some point reasonably early in the
// generation of nonces.  We do this by making sure we see a 0 and a 1 in each
// bit position.  We do that by setting bits in a pair of bit masks for each 0
// and 1 found.  When we're done, any remaining clear bits correspond to bit
// positions that didn't change.
void NonceGeneratorTestBase::AllBitsUsed() {
  uint64 ones_found = 0;
  uint64 zeros_found = 0;
  for (int i = 0;
       (i < kSmallNumIterations) &&
           ((~ones_found != 0) || (~zeros_found != 0));
       ++i) {
    uint64 nonce = main_generator_->NewNonce();
    ones_found |= nonce;
    zeros_found |= ~nonce;
  }
  EXPECT_EQ(0, ~ones_found);
  EXPECT_EQ(0, ~zeros_found);
}

}  // namespace net_instaweb
