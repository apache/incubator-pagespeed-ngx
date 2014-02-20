/*
 * Copyright 2014 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "net/instaweb/rewriter/public/critical_finder_support_util.h"

#include "net/instaweb/rewriter/critical_keys.pb.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/gtest.h"
#include "pagespeed/kernel/base/mock_timer.h"

namespace net_instaweb {
namespace {

const int kSupportInterval = 10;

class CriticalFinderSupportUtilTest : public RewriteTestBase {
 protected:
  CriticalFinderSupportUtilTest() {}

  virtual ~CriticalFinderSupportUtilTest() {}

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    critical_keys_proto_.Clear();
  }

  void TestPrepareForBeaconInsertionHelper(const StringSet& keys,
                                           BeaconStatus expected_status) {
    BeaconMetadata result;
    UpdateCandidateKeys(keys, &critical_keys_proto_, true);
    PrepareForBeaconInsertionHelper(
        &critical_keys_proto_, factory()->nonce_generator(), rewrite_driver(),
        true /* using_candidate_key_detection */, &result);
    EXPECT_EQ(expected_status, result.status);
    // Clear the count of expired nonces. We aren't actually sending back the
    // nonces in our test, so they can expire and put us into low frequency
    // beaconing mode when we aren't expecting it to.
    critical_keys_proto_.set_nonces_recently_expired(0);
  }

  CriticalKeys critical_keys_proto_;
};

TEST_F(CriticalFinderSupportUtilTest,
       PrepareForBeaconInsertionHelperWithCandidateKeys) {
  StringSet keys;

  keys.insert("a");
  TestPrepareForBeaconInsertionHelper(keys, kBeaconWithNonce);

  // We shouldn't get another beacon until we either change the keys, or time
  // advances to high frequency beaconing amount.
  TestPrepareForBeaconInsertionHelper(keys, kDoNotBeacon);
  keys.insert("b");
  TestPrepareForBeaconInsertionHelper(keys, kBeaconWithNonce);
  TestPrepareForBeaconInsertionHelper(keys, kDoNotBeacon);
  factory()->mock_timer()->AdvanceMs(options()->beacon_reinstrument_time_sec() *
                                     Timer::kSecondMs);
  TestPrepareForBeaconInsertionHelper(keys, kBeaconWithNonce);

  // Verify that if the candidate keys don't change for kHighFreqBeaconCount
  // then we transition into low frequency beaconing.
  keys.insert("c");
  for (int i = 0; i < kHighFreqBeaconCount; ++i) {
    TestPrepareForBeaconInsertionHelper(keys, kBeaconWithNonce);
    factory()->mock_timer()->AdvanceMs(
        options()->beacon_reinstrument_time_sec() * Timer::kSecondMs);
    // Normally the beacon_received field would be updated upon beacon response
    // by UpdateCriticalKeys.
    critical_keys_proto_.set_valid_beacons_received(
        critical_keys_proto_.valid_beacons_received() + 1);
  }
  // Now critical_keys_proto_.valid_beacons_received() == kHighFreqBeaconCount,
  // so after the next call to PrepareForBeaconInsertionHelper the next beacon
  // should occur at the low frequency time.
  TestPrepareForBeaconInsertionHelper(keys, kBeaconWithNonce);
  factory()->mock_timer()->AdvanceMs(options()->beacon_reinstrument_time_sec() *
                                     Timer::kSecondMs);
  TestPrepareForBeaconInsertionHelper(keys, kDoNotBeacon);
  factory()->mock_timer()->AdvanceMs(options()->beacon_reinstrument_time_sec() *
                                     Timer::kSecondMs * kLowFreqBeaconMult);
  TestPrepareForBeaconInsertionHelper(keys, kBeaconWithNonce);

  // And changing the keys again should put us back into high frequency
  // beaconing.
  keys.insert("d");
  TestPrepareForBeaconInsertionHelper(keys, kBeaconWithNonce);
  EXPECT_EQ(0, critical_keys_proto_.valid_beacons_received());
  factory()->mock_timer()->AdvanceMs(options()->beacon_reinstrument_time_sec() *
                                     Timer::kSecondMs);
  TestPrepareForBeaconInsertionHelper(keys, kBeaconWithNonce);
}

}  // namespace
}  // namespace net_instaweb
