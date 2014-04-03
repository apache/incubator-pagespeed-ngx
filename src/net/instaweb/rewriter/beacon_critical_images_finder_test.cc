/*
 * Copyright 2013 Google Inc.
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
// Author: jmaessen@google.com (Jan-Willem Maessen)

#include "net/instaweb/rewriter/public/beacon_critical_images_finder.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/critical_images.pb.h"
#include "net/instaweb/rewriter/critical_keys.pb.h"
#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/critical_images_finder_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/timer.h"

using ::testing::Eq;

namespace net_instaweb {

namespace {

const char kRequestUrl[] = "http://www.example.com";

class BeaconCriticalImagesFinderTest : public CriticalImagesFinderTestBase {
 public:
  virtual CriticalImagesFinder* finder() { return finder_; }

 protected:
  BeaconCriticalImagesFinderTest() { }

  virtual void SetUp() {
    CriticalImagesFinderTestBase::SetUp();
    const PropertyCache::Cohort* beacon_cohort =
        SetupCohort(page_property_cache(), RewriteDriver::kBeaconCohort);
    server_context()->set_beacon_cohort(beacon_cohort);
    finder_ = new BeaconCriticalImagesFinder(
        beacon_cohort, factory()->nonce_generator(), statistics());
    server_context()->set_critical_images_finder(finder_);
    ResetDriver();
    SetDummyRequestHeaders();
    // Set up default critical image sets to use for testing.
    html_images_.insert("x.jpg");
    html_images_.insert("y.png");
    html_images_.insert("z.gif");
    css_images_.insert("a.jpg");
    css_images_.insert("b.png");
    css_images_.insert("c.gif");
  }

  void WriteToPropertyCache() {
    rewrite_driver()->property_page()->WriteCohort(
        server_context()->beacon_cohort());
  }

  void WriteBackAndResetDriver() {
    WriteToPropertyCache();
    ResetDriver();
    SetDummyRequestHeaders();
  }

  GoogleString CriticalImagesString() {
    WriteBackAndResetDriver();
    const StringSet& html_images =
        finder_->GetHtmlCriticalImages(rewrite_driver());
    const StringSet& css_images =
        finder_->GetCssCriticalImages(rewrite_driver());
    GoogleString result = JoinCollection(html_images, ",");
    StrAppend(&result, ";");
    AppendJoinCollection(&result, css_images, ",");
    return result;
  }

  // Simulate beacon insertion.
  void Beacon() {
    WriteBackAndResetDriver();
    factory()->mock_timer()->AdvanceMs(
        options()->beacon_reinstrument_time_sec() * Timer::kSecondMs);
    VerifyBeaconStatus(kBeaconWithNonce);
  }

  // Same as Beacon(), but advances time by the low frequency beacon interval.
  // Useful in cases where a lot of beacons with the same critical image set are
  // being sent.
  void BeaconLowFrequency() {
    WriteBackAndResetDriver();
    factory()->mock_timer()->AdvanceMs(
        options()->beacon_reinstrument_time_sec() * Timer::kSecondMs *
        kLowFreqBeaconMult);
    VerifyBeaconStatus(kBeaconWithNonce);
  }

  // Verify that no beacon injection occurs.
  void VerifyNoBeaconing() {
    VerifyBeaconStatus(kDoNotBeacon);
  }

  // Verify that beacon injection occurs.
  void VerifyBeaconing() {
    VerifyBeaconStatus(kBeaconWithNonce);
  }

  // Helper method used for verifying beacon injection status.
  void VerifyBeaconStatus(BeaconStatus status) {
    bool status_is_beacon_with_nonce = (status == kBeaconWithNonce);
    EXPECT_THAT(finder_->ShouldBeacon(rewrite_driver()),
                Eq(status_is_beacon_with_nonce));
    last_beacon_metadata_ =
        finder_->PrepareForBeaconInsertion(rewrite_driver());
    EXPECT_EQ(status, last_beacon_metadata_.status);
    if (status == kBeaconWithNonce) {
      EXPECT_STREQ(ExpectedNonce(), last_beacon_metadata_.nonce);
    } else {
      EXPECT_TRUE(last_beacon_metadata_.nonce.empty());
    }
  }

  CriticalImages* GetCriticalImages() {
    WriteBackAndResetDriver();
    EXPECT_TRUE(finder()->IsCriticalImageInfoPresent(rewrite_driver()));
    return &rewrite_driver()->critical_images_info()->proto;
  }

  void CheckDefaultBeaconSupport(int support) {
    CheckAXBeaconSupport(support, support, support);
  }

  void CheckAXBeaconSupport(int a_support, int x_support, int other_support) {
    // Inspect support values in the critical images protobuf.
    const CriticalImages* critical_images = GetCriticalImages();
    const CriticalKeys& html_keys =
        critical_images->html_critical_image_support();
    const CriticalKeys& css_keys =
        critical_images->css_critical_image_support();
    ASSERT_EQ(3, html_keys.key_evidence_size());
    EXPECT_EQ("x.jpg", html_keys.key_evidence(0).key());
    EXPECT_EQ(x_support, html_keys.key_evidence(0).support());
    EXPECT_EQ("y.png", html_keys.key_evidence(1).key());
    EXPECT_EQ(other_support, html_keys.key_evidence(1).support());
    EXPECT_EQ("z.gif", html_keys.key_evidence(2).key());
    EXPECT_EQ(other_support, html_keys.key_evidence(2).support());
    ASSERT_EQ(3, css_keys.key_evidence_size());
    EXPECT_EQ("a.jpg", css_keys.key_evidence(0).key());
    EXPECT_EQ(a_support, css_keys.key_evidence(0).support());
    EXPECT_EQ("b.png", css_keys.key_evidence(1).key());
    EXPECT_EQ(other_support, css_keys.key_evidence(1).support());
    EXPECT_EQ("c.gif", css_keys.key_evidence(2).key());
    EXPECT_EQ(other_support, css_keys.key_evidence(2).support());
  }

  bool UpdateCriticalImagesCacheEntry(
      const StringSet* html_critical_images_set,
      const StringSet* css_critical_images_set) {
    // If this fails, you should have called Beacon().
    CHECK_EQ(kBeaconWithNonce, last_beacon_metadata_.status);
    return UpdateCriticalImagesCacheEntry(
        html_critical_images_set, css_critical_images_set,
        last_beacon_metadata_.nonce);
  }

  bool UpdateCriticalImagesCacheEntry(
      const StringSet* html_critical_images_set,
      const StringSet* css_critical_images_set,
      const GoogleString& nonce) {
    EXPECT_FALSE(nonce.empty());
    return finder_->UpdateCriticalImagesCacheEntry(
        html_critical_images_set, css_critical_images_set, NULL,
        nonce, server_context()->beacon_cohort(),
        rewrite_driver()->property_page(), server_context()->timer());
  }

  BeaconCriticalImagesFinder* finder_;
  BeaconMetadata last_beacon_metadata_;
  StringSet html_images_;
  StringSet css_images_;
};

TEST_F(BeaconCriticalImagesFinderTest, StoreRestore) {
  // Before beacon insertion, nothing in pcache.
  CheckCriticalImageFinderStats(0, 0, 0);
  CriticalImagesInfo* read_images =
      rewrite_driver()->critical_images_info();
  EXPECT_TRUE(read_images == NULL);
  // Force computation of critical_images_info() via CriticalImagesString()
  EXPECT_STREQ(";", CriticalImagesString());
  read_images = rewrite_driver()->critical_images_info();
  EXPECT_TRUE(read_images != NULL);
  // Now beacon and register some critical image results.
  Beacon();
  CheckCriticalImageFinderStats(0, 0, 2);
  EXPECT_TRUE(UpdateCriticalImagesCacheEntry(&html_images_, &css_images_));
  // Check actual support values, but also verify that images are considered
  // critical.
  CheckDefaultBeaconSupport(finder_->SupportInterval());
  EXPECT_STREQ("x.jpg,y.png,z.gif;a.jpg,b.png,c.gif", CriticalImagesString());
  CheckCriticalImageFinderStats(2, 0, 2);
  // Now test expiration.
  WriteBackAndResetDriver();
  AdvanceTimeMs(2 * options()->finder_properties_cache_expiration_time_ms());
  read_images = rewrite_driver()->critical_images_info();
  EXPECT_TRUE(read_images == NULL);
  // Force computation of critical_images_info() via CriticalImagesString()
  EXPECT_STREQ(";", CriticalImagesString());
  CheckCriticalImageFinderStats(2, 1, 2);
}

// Verify that writing multiple beacon results are stored and aggregated. The
// critical selector set should contain all images seen in the last
// SupportInterval() beacon responses.  After SupportInterval() responses,
// beacon results only seen once should no longer be considered critical.
TEST_F(BeaconCriticalImagesFinderTest, StoreMultiple) {
  Beacon();
  EXPECT_TRUE(UpdateCriticalImagesCacheEntry(&html_images_, &css_images_));
  EXPECT_STREQ("x.jpg,y.png,z.gif;a.jpg,b.png,c.gif", CriticalImagesString());
  CheckDefaultBeaconSupport(finder_->SupportInterval());

  html_images_.clear();
  html_images_.insert("x.jpg");
  css_images_.clear();
  css_images_.insert("a.jpg");
  for (int i = 0; i < finder_->SupportInterval() - 1; ++i) {
    BeaconLowFrequency();
    EXPECT_TRUE(UpdateCriticalImagesCacheEntry(&html_images_, &css_images_));
    EXPECT_STREQ("x.jpg;a.jpg", CriticalImagesString());
  }

  // We send two more beacon responses, which should kick a.jpg out of the
  // critical css images set as it falls below the 80% support threshold.  y.png
  // will not accumulate enough support to be considered critical.
  css_images_.clear();
  html_images_.insert("y.png");
  for (int i = 0; i < 2; ++i) {
    BeaconLowFrequency();
    EXPECT_TRUE(UpdateCriticalImagesCacheEntry(&html_images_, &css_images_));
  }
  EXPECT_STREQ("x.jpg;", CriticalImagesString());
}

// Make sure beacon results can arrive out of order (so long as the nonce
// doesn't time out).
TEST_F(BeaconCriticalImagesFinderTest, OutOfOrder) {
  // Make sure that the rebeaconing time is less than the time a nonce is valid,
  // so that we can test having multiple outstanding nonces.
  options()->set_beacon_reinstrument_time_sec(kBeaconTimeoutIntervalMs /
                                              Timer::kSecondMs / 2);
  Beacon();
  GoogleString initial_nonce(last_beacon_metadata_.nonce);
  // A second beacon occurs and the result comes back first.
  Beacon();
  EXPECT_TRUE(UpdateCriticalImagesCacheEntry(&html_images_, &css_images_));
  EXPECT_STREQ("x.jpg,y.png,z.gif;a.jpg,b.png,c.gif", CriticalImagesString());
  CheckDefaultBeaconSupport(finder_->SupportInterval());
  // Now the first beacon result comes back out of order.  It should still work.
  html_images_.clear();
  html_images_.insert("x.jpg");
  css_images_.clear();
  css_images_.insert("a.jpg");
  EXPECT_TRUE(UpdateCriticalImagesCacheEntry(
      &html_images_, &css_images_, initial_nonce));
  EXPECT_STREQ("x.jpg;a.jpg", CriticalImagesString());
  int supportedTwice = 2 * finder_->SupportInterval() - 1;
  CheckAXBeaconSupport(supportedTwice, supportedTwice,
                       finder_->SupportInterval() - 1);
  // A duplicate beacon nonce will be dropped, and support won't change.
  EXPECT_FALSE(UpdateCriticalImagesCacheEntry(&html_images_, &css_images_));
  EXPECT_STREQ("x.jpg;a.jpg", CriticalImagesString());
  CheckAXBeaconSupport(supportedTwice, supportedTwice,
                       finder_->SupportInterval() - 1);
  // As will an entirely bogus nonce (here we use non-base64 characters).
  const char kBogusNonce[] = "*&*";
  EXPECT_FALSE(UpdateCriticalImagesCacheEntry(
      &html_images_, &css_images_, kBogusNonce));
  EXPECT_STREQ("x.jpg;a.jpg", CriticalImagesString());
  CheckAXBeaconSupport(supportedTwice, supportedTwice,
                       finder_->SupportInterval() - 1);
}

TEST_F(BeaconCriticalImagesFinderTest, NonceTimeout) {
  // Make sure that beacons time out after kBeaconTimeoutIntervalMs.
  Beacon();
  GoogleString initial_nonce(last_beacon_metadata_.nonce);
  // beacon_reinstrument_time_sec() passes (in mock time) before the next call
  // completes:
  Beacon();
  factory()->mock_timer()->AdvanceMs(kBeaconTimeoutIntervalMs);
  // This beacon arrives right at its deadline, and is OK.
  EXPECT_TRUE(UpdateCriticalImagesCacheEntry(&html_images_, &css_images_));
  EXPECT_STREQ("x.jpg,y.png,z.gif;a.jpg,b.png,c.gif", CriticalImagesString());
  CheckDefaultBeaconSupport(finder_->SupportInterval());
  // The first beacon arrives after its deadline, and is dropped.
  html_images_.clear();
  html_images_.insert("x.jpg");
  css_images_.clear();
  css_images_.insert("a.jpg");
  EXPECT_FALSE(UpdateCriticalImagesCacheEntry(
      &html_images_, &css_images_, initial_nonce));
  EXPECT_STREQ("x.jpg,y.png,z.gif;a.jpg,b.png,c.gif", CriticalImagesString());
  CheckDefaultBeaconSupport(finder_->SupportInterval());
}

TEST_F(BeaconCriticalImagesFinderTest, DontRebeaconBeforeTimeout) {
  Beacon();
  // Now simulate a beacon insertion attempt without timing out.
  WriteBackAndResetDriver();
  factory()->mock_timer()->AdvanceMs(options()->beacon_reinstrument_time_sec() *
                                     Timer::kSecondMs / 2);
  BeaconMetadata metadata =
      finder_->PrepareForBeaconInsertion(rewrite_driver());
  EXPECT_EQ(kDoNotBeacon, metadata.status);
  // But we'll re-beacon if some more time passes.
  Beacon();  // beacon_reinstrument_time_sec() passes in Beacon() call.
}

TEST_F(BeaconCriticalImagesFinderTest, RebeaconBeforeTimeoutWithHeader) {
  Beacon();

  // Write a dummy value to the property cache.
  WriteToPropertyCache();

  // If downstream caching is disabled, any beaconing key configuration and/or
  // presence of PS-ShouldBeacon header should be ignored. In such situations,
  // unless the reinstrumentation time interval is exceeded, beacon injection
  // should not happen.
  ResetDriver();
  SetDownstreamCacheDirectives("", "", kConfiguredBeaconingKey);
  SetShouldBeaconHeader(kConfiguredBeaconingKey);
  VerifyNoBeaconing();
  // Advance the timer past the beacon interval.
  factory()->mock_timer()->AdvanceMs(
      options()->beacon_reinstrument_time_sec() * Timer::kSecondMs + 1);
  // When the reinstrumentation time interval is exceeded, beacon injection
  // should happen as usual.
  ResetDriver();
  SetDownstreamCacheDirectives("", "", kConfiguredBeaconingKey);
  SetShouldBeaconHeader(kConfiguredBeaconingKey);
  VerifyBeaconing();

  // Beacon injection should not happen when rebeaconing key is not configured.
  ResetDriver();
  SetDownstreamCacheDirectives("", "localhost:80", "");
  SetShouldBeaconHeader(kConfiguredBeaconingKey);
  VerifyNoBeaconing();

  // Beacon injection should not happen when the PS-ShouldBeacon header is
  // absent and both downstream caching and the associated rebeaconing key
  // are configured.
  ResetDriver();
  SetDownstreamCacheDirectives("", "localhost:80", kConfiguredBeaconingKey);
  SetDummyRequestHeaders();
  VerifyNoBeaconing();

  // Beacon injection should not happen when the PS-ShouldBeacon header is
  // incorrect.
  ResetDriver();
  SetDownstreamCacheDirectives("", "localhost:80", kConfiguredBeaconingKey);
  SetShouldBeaconHeader(kWrongBeaconingKey);
  VerifyNoBeaconing();

  // Beacon injection happens when the PS-ShouldBeacon header is present even
  // when the pcache value has not expired and the reinstrumentation time
  // interval has not been exceeded.
  ResetDriver();
  SetDownstreamCacheDirectives("", "localhost:80", kConfiguredBeaconingKey);
  SetShouldBeaconHeader(kConfiguredBeaconingKey);
  VerifyBeaconing();

  // Advance the timer past the beacon interval.
  factory()->mock_timer()->AdvanceMs(
      options()->beacon_reinstrument_time_sec() * Timer::kSecondMs + 1);
  // Beacon injection should happen after reinstrumentation time interval has
  // passed when downstream caching is enabled but rebeaconing key is not
  // configured.
  ResetDriver();
  SetDownstreamCacheDirectives("", "localhost:80", "");
  SetShouldBeaconHeader(kConfiguredBeaconingKey);
  VerifyBeaconing();

  // Advance the timer past the beacon interval.
  factory()->mock_timer()->AdvanceMs(
      options()->beacon_reinstrument_time_sec() * Timer::kSecondMs + 1);
  // Beacon injection should not happen when the PS-ShouldBeacon header is
  // incorrect even if the reinstrumentation time interval has been exceeded.
  ResetDriver();
  SetDownstreamCacheDirectives("", "localhost:80", kConfiguredBeaconingKey);
  SetShouldBeaconHeader(kWrongBeaconingKey);
  VerifyNoBeaconing();
}

// Verify that sending enough beacons with the same critical image set puts us
// into low frequency beaconing mode.
TEST_F(BeaconCriticalImagesFinderTest, LowFrequencyBeaconing) {
  StringSet html_critical_images_set;
  html_critical_images_set.insert("x.jpg");
  finder_->UpdateCandidateImagesForBeaconing(
      html_critical_images_set, rewrite_driver(), false /* beaconing */);
  // Send enough beacons to put us into low frequency beaconing mode.
  for (int i = 0; i <= kHighFreqBeaconCount; ++i) {
    Beacon();
    BeaconCriticalImagesFinder::UpdateCriticalImagesCacheEntry(
        &html_critical_images_set, NULL, NULL, last_beacon_metadata_.nonce,
        server_context()->beacon_cohort(), rewrite_driver()->property_page(),
        factory()->mock_timer());
    CriticalKeys* html_critical_images =
        GetCriticalImages()->mutable_html_critical_image_support();
    EXPECT_THAT(html_critical_images->valid_beacons_received(), Eq(i + 1));
  }

  // Now we are in low frequency beaconing mode, so advancing by the high
  // frequency beaconing amount should not trigger beaconing.
  factory()->mock_timer()->AdvanceMs(options()->beacon_reinstrument_time_sec() *
                                     Timer::kSecondMs);
  EXPECT_FALSE(finder_->ShouldBeacon(rewrite_driver()));
  // But advancing by the low frequency amount should.
  factory()->mock_timer()->AdvanceMs(options()->beacon_reinstrument_time_sec() *
                                     Timer::kSecondMs * kLowFreqBeaconMult);
  Beacon();
  factory()->mock_timer()->AdvanceMs(options()->beacon_reinstrument_time_sec() *
                                     Timer::kSecondMs);
  VerifyNoBeaconing();

  // Now verify that updating the candidate images works correctly. If we are
  // beaconing, then the next beacon timestamp does not get updated.
  html_critical_images_set.insert("y.jpg");
  finder_->UpdateCandidateImagesForBeaconing(
      html_critical_images_set, rewrite_driver(), true /* beaconing */);
  VerifyNoBeaconing();

  // Verify that setting the beaconing flag to false when inserting a new
  // candidate key does trigger beaconing on the next request.
  html_critical_images_set.insert("z.jpg");
  finder_->UpdateCandidateImagesForBeaconing(
      html_critical_images_set, rewrite_driver(), false /* beaconing */);
  Beacon();
}

}  // namespace

}  // namespace net_instaweb
