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
// Author: slamm@google.com (Stephen Lamm)

#include "net/instaweb/rewriter/public/critical_selector_finder.h"

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/critical_keys.pb.h"
#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/statistics.h"
#include "pagespeed/kernel/base/mock_timer.h"

namespace net_instaweb {

namespace {

const char kRequestUrl[] = "http://www.example.com";

class CriticalSelectorFinderTest : public RewriteTestBase {
 protected:
  CriticalSelectorFinderTest() { }

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    const PropertyCache::Cohort* beacon_cohort =
        SetupCohort(page_property_cache(), RewriteDriver::kBeaconCohort);
    server_context()->set_beacon_cohort(beacon_cohort);
    finder_ = CreateFinder(beacon_cohort);
    server_context()->set_critical_selector_finder(finder_);
    candidates_.insert("#bar");
    candidates_.insert(".a");
    candidates_.insert(".b");
    candidates_.insert("#c");
    candidates_.insert(".foo");
    ResetDriver();
  }

  virtual CriticalSelectorFinder* CreateFinder(
      const PropertyCache::Cohort* cohort) {
    return new BeaconCriticalSelectorFinder(
        cohort, factory()->nonce_generator(), statistics());
  }

  void ResetDriver() {
    rewrite_driver()->Clear();
    rewrite_driver()->set_request_context(
        RequestContext::NewTestRequestContext(factory()->thread_system()));
    MockPropertyPage* page = NewMockPage(kRequestUrl);
    rewrite_driver()->set_property_page(page);
    PropertyCache* pcache = server_context_->page_property_cache();
    pcache->Read(page);
  }

  void WriteBackAndResetDriver() {
    rewrite_driver()->property_page()->WriteCohort(
        server_context()->beacon_cohort());
    ResetDriver();
  }

  void CheckCriticalSelectorFinderStats(int hits, int expiries, int not_found) {
    EXPECT_EQ(hits, statistics()->GetVariable(
        CriticalSelectorFinder::kCriticalSelectorsValidCount)->Get());
    EXPECT_EQ(expiries, statistics()->GetVariable(
        CriticalSelectorFinder::kCriticalSelectorsExpiredCount)->Get());
    EXPECT_EQ(not_found, statistics()->GetVariable(
        CriticalSelectorFinder::kCriticalSelectorsNotFoundCount)->Get());
  }

  GoogleString CriticalSelectorsString() {
    WriteBackAndResetDriver();
    const StringSet& critical_selectors =
        finder_->GetCriticalSelectors(rewrite_driver());
    return JoinCollection(critical_selectors, ",");
  }

  // Write a raw critical selector set to pcache, used to test legacy
  // compatibility since new code won't create legacy protos.
  void WriteCriticalSelectorSetToPropertyCache(
      const CriticalKeys& selector_set) {
    PropertyCacheUpdateResult result = UpdateInPropertyCache(
        selector_set, server_context()->beacon_cohort(),
        CriticalSelectorFinder::kCriticalSelectorsPropertyName, true,
        rewrite_driver()->property_page());
    ASSERT_EQ(kPropertyCacheUpdateOk, result);
  }

  void WriteCriticalSelectorsToPropertyCache(const StringSet& selectors) {
    finder_->WriteCriticalSelectorsToPropertyCache(
        selectors, last_beacon_metadata_.nonce, rewrite_driver());
  }

  virtual BeaconStatus ExpectedBeaconStatus() {
    return kBeaconWithNonce;
  }

  // Simulate beacon insertion, with candidates_.
  void Beacon() {
    WriteBackAndResetDriver();
    factory()->mock_timer()->AdvanceMs(kMinBeaconIntervalMs);
    last_beacon_metadata_ =
        finder_->PrepareForBeaconInsertion(candidates_, rewrite_driver());
    ASSERT_EQ(ExpectedBeaconStatus(), last_beacon_metadata_.status);
  }

  // Set up legacy critical selectors value.  We have to do this by hand using
  // the protos and direct pcache writes, since the new finder by design doesn't
  // write legacy data.
  void SetupLegacyCriticalSelectors(bool include_history) {
    CriticalKeys legacy_selectors;
    legacy_selectors.add_critical_keys("#bar");
    legacy_selectors.add_critical_keys(".foo");
    if (include_history) {
      CriticalKeys::BeaconResponse* first_set =
          legacy_selectors.add_beacon_history();
      first_set->add_keys("#bar");
      CriticalKeys::BeaconResponse* second_set =
          legacy_selectors.add_beacon_history();
      second_set->add_keys("#bar");
      second_set->add_keys(".foo");
    }
    WriteCriticalSelectorSetToPropertyCache(legacy_selectors);
  }

  CriticalKeys* RawCriticalSelectorSet(int expected_size) {
    WriteBackAndResetDriver();
    finder_->GetCriticalSelectors(rewrite_driver());
    CriticalKeys* selectors =
        &rewrite_driver()->critical_selector_info()->proto;
    if (selectors != NULL) {
      EXPECT_EQ(0, selectors->critical_keys_size());
      EXPECT_EQ(0, selectors->beacon_history_size());
      if (selectors->key_evidence_size() != expected_size) {
        EXPECT_EQ(expected_size, selectors->key_evidence_size());
      }
    }
    return selectors;
  }

  void CheckFooBarBeaconSupport(int support) {
    CheckFooBarBeaconSupport(support, support);
  }

  void CheckFooBarBeaconSupport(int foo_support, int bar_support) {
    // Check for .foo and #bar support, with no support for other beaconed
    // candidates.
    CriticalKeys* read_selectors = RawCriticalSelectorSet(5);
    ASSERT_TRUE(read_selectors != NULL);
    EXPECT_EQ("#bar", read_selectors->key_evidence(0).key());
    EXPECT_EQ(bar_support, read_selectors->key_evidence(0).support());
    EXPECT_EQ("#c", read_selectors->key_evidence(1).key());
    EXPECT_EQ(0, read_selectors->key_evidence(1).support());
    EXPECT_EQ(".a", read_selectors->key_evidence(2).key());
    EXPECT_EQ(0, read_selectors->key_evidence(2).support());
    EXPECT_EQ(".b", read_selectors->key_evidence(3).key());
    EXPECT_EQ(0, read_selectors->key_evidence(3).support());
    EXPECT_EQ(".foo", read_selectors->key_evidence(4).key());
    EXPECT_EQ(foo_support, read_selectors->key_evidence(4).support());
  }

  CriticalSelectorFinder* finder_;
  StringSet candidates_;
  BeaconMetadata last_beacon_metadata_;
};

TEST_F(CriticalSelectorFinderTest, StoreRestore) {
  // Before beacon insertion, nothing in pcache.
  CheckCriticalSelectorFinderStats(0, 0, 0);
  CriticalSelectorInfo* read_selectors =
      rewrite_driver()->critical_selector_info();
  EXPECT_TRUE(read_selectors == NULL);
  StringSet critical_selectors =
      finder_->GetCriticalSelectors(rewrite_driver());
  read_selectors = rewrite_driver()->critical_selector_info();
  EXPECT_TRUE(read_selectors != NULL);
  EXPECT_TRUE(critical_selectors.empty());
  CheckCriticalSelectorFinderStats(0, 0, 1);

  Beacon();
  CheckCriticalSelectorFinderStats(0, 0, 2);
  StringSet selectors;
  selectors.insert(".foo");
  selectors.insert("#bar");
  WriteCriticalSelectorsToPropertyCache(selectors);
  CheckFooBarBeaconSupport(finder_->SupportInterval());
  CheckCriticalSelectorFinderStats(1, 0, 2);

  // Now test expiration.
  WriteBackAndResetDriver();
  AdvanceTimeMs(2 * options()->finder_properties_cache_expiration_time_ms());
  read_selectors = rewrite_driver()->critical_selector_info();
  EXPECT_TRUE(read_selectors == NULL);
  critical_selectors = finder_->GetCriticalSelectors(rewrite_driver());
  CheckCriticalSelectorFinderStats(1, 1, 2);
}

// Verify that writing multiple beacon results are stored and aggregated. The
// critical selector set should contain all selectors seen in the last
// SupportInterval() beacon responses.  After SupportInterval() responses,
// beacon results only seen once should no longer be considered critical.
TEST_F(CriticalSelectorFinderTest, StoreMultiple) {
  Beacon();
  StringSet selectors;
  selectors.insert(".a");
  WriteCriticalSelectorsToPropertyCache(selectors);
  EXPECT_STREQ(".a", CriticalSelectorsString());

  selectors.clear();
  selectors.insert(".b");
  for (int i = 0; i < finder_->SupportInterval() - 1; ++i) {
    Beacon();
    WriteCriticalSelectorsToPropertyCache(selectors);
    EXPECT_STREQ(".a,.b", CriticalSelectorsString());
  }

  // We send one more beacon response, which should kick .a out of the critical
  // selector set.
  Beacon();
  selectors.clear();
  selectors.insert("#c");
  WriteCriticalSelectorsToPropertyCache(selectors);
  EXPECT_STREQ("#c,.b", CriticalSelectorsString());
}

// Make sure beacon results can arrive out of order (so long as the nonce
// doesn't time out).
TEST_F(CriticalSelectorFinderTest, OutOfOrder) {
  Beacon();
  GoogleString initial_nonce(last_beacon_metadata_.nonce);
  // A second beacon occurs and the result comes back first.
  Beacon();
  StringSet selectors;
  selectors.insert(".a");
  WriteCriticalSelectorsToPropertyCache(selectors);
  EXPECT_STREQ(".a", CriticalSelectorsString());
  // Now the first beacon result comes back out of order.  It should still work.
  selectors.clear();
  selectors.insert(".b");
  finder_->WriteCriticalSelectorsToPropertyCache(
      selectors, initial_nonce, rewrite_driver());
  EXPECT_STREQ(".a,.b", CriticalSelectorsString());
  // A duplicate beacon nonce will be dropped.
  selectors.clear();
  selectors.insert("#c");
  finder_->WriteCriticalSelectorsToPropertyCache(
      selectors, initial_nonce, rewrite_driver());
  EXPECT_STREQ(".a,.b", CriticalSelectorsString());
  // As will an entirely bogus nonce (here we use non-base64 characters).
  const char kBogusNonce[] = "*&*";
  finder_->WriteCriticalSelectorsToPropertyCache(
      selectors, kBogusNonce, rewrite_driver());
  EXPECT_STREQ(".a,.b", CriticalSelectorsString());
}

TEST_F(CriticalSelectorFinderTest, NonceTimeout) {
  // Make sure that beacons time out after kBeaconTimeoutIntervalMs.
  Beacon();
  GoogleString initial_nonce(last_beacon_metadata_.nonce);
  // kMinBeaconIntervalMs passes (in mock time) before the next call completes:
  Beacon();
  factory()->mock_timer()->AdvanceMs(kBeaconTimeoutIntervalMs);
  StringSet selectors;
  selectors.insert(".a");
  // This beacon arrives right at its deadline, and is OK.
  WriteCriticalSelectorsToPropertyCache(selectors);
  EXPECT_STREQ(".a", CriticalSelectorsString());
  // The first beacon arrives after its deadline, and is dropped.
  selectors.clear();
  selectors.insert(".b");
  finder_->WriteCriticalSelectorsToPropertyCache(
      selectors, initial_nonce, rewrite_driver());
  EXPECT_STREQ(".a", CriticalSelectorsString());
}

// Make sure that inserting a non-candidate critical selector has no effect.
TEST_F(CriticalSelectorFinderTest, StoreNonCandidate) {
  Beacon();
  StringSet selectors;
  selectors.insert(".a");
  selectors.insert(".noncandidate");
  selectors.insert("#noncandidate");
  WriteCriticalSelectorsToPropertyCache(selectors);
  EXPECT_STREQ(".a", CriticalSelectorsString());
}

// Test migration of legacy critical selectors to support format during beacon
// insertion.  This tests the case where only critical_selectors were set.
TEST_F(CriticalSelectorFinderTest, LegacySelectorSetBeaconMigration) {
  // First set up legacy pcache entry.
  SetupLegacyCriticalSelectors(false /* include_history */);
  Beacon();
  CheckFooBarBeaconSupport(finder_->SupportInterval());
}

// Test migration of legacy critical selectors to support format during critical
// selector return.  This tests the case where only critical_selectors were set.
TEST_F(CriticalSelectorFinderTest, LegacySelectorSetMigration) {
  SetupLegacyCriticalSelectors(false /* include_history */);
  // Create a new critical selector set and add it.  The legacy data will have
  // migrated, and we'll add support for ".foo".
  Beacon();
  StringSet selectors;
  selectors.insert(".noncandidate");
  selectors.insert(".foo");
  WriteCriticalSelectorsToPropertyCache(selectors);
  CheckFooBarBeaconSupport(2 * finder_->SupportInterval() - 1,
                           finder_->SupportInterval() - 1);
}

// Test migration of legacy selector history to the new format (using support).
// This tests the case where both critical_selectors and selector_set_history
// were set.
TEST_F(CriticalSelectorFinderTest, LegacySelectorSetHistoryMigration) {
  SetupLegacyCriticalSelectors(true /* include_history */);
  // Create a new critical selector set and add it.  The legacy data will have
  // migrated, and we'll add support for ".foo".
  Beacon();
  StringSet selectors;
  selectors.insert(".noncandidate");
  selectors.insert(".foo");
  WriteCriticalSelectorsToPropertyCache(selectors);
  CheckFooBarBeaconSupport(2 * finder_->SupportInterval() - 1,
                           2 * finder_->SupportInterval() - 2);
}

// Make sure we aggregate duplicate beacon results.
TEST_F(CriticalSelectorFinderTest, DuplicateEntries) {
  Beacon();
  StringSet beacon_result;
  beacon_result.insert("#bar");
  beacon_result.insert(".foo");
  beacon_result.insert(".a");
  WriteCriticalSelectorsToPropertyCache(beacon_result);
  Beacon();
  beacon_result.clear();
  beacon_result.insert("#bar");
  beacon_result.insert(".foo");
  beacon_result.insert(".b");
  WriteCriticalSelectorsToPropertyCache(beacon_result);

  // Now cross-check the critical selector set.
  CriticalKeys* read_selectors = RawCriticalSelectorSet(5);
  ASSERT_TRUE(read_selectors != NULL);
  EXPECT_EQ("#bar", read_selectors->key_evidence(0).key());
  EXPECT_EQ("#c", read_selectors->key_evidence(1).key());
  EXPECT_EQ(".a", read_selectors->key_evidence(2).key());
  EXPECT_EQ(".b", read_selectors->key_evidence(3).key());
  EXPECT_EQ(".foo", read_selectors->key_evidence(4).key());
  EXPECT_EQ(2 * finder_->SupportInterval() - 1,
            read_selectors->key_evidence(0).support());
  EXPECT_EQ(0, read_selectors->key_evidence(1).support());
  EXPECT_EQ(finder_->SupportInterval() - 1,
            read_selectors->key_evidence(2).support());
  EXPECT_EQ(finder_->SupportInterval(),
            read_selectors->key_evidence(3).support());
  EXPECT_EQ(2 * finder_->SupportInterval() - 1,
            read_selectors->key_evidence(4).support());
}

// Make sure overflow of evidence can't happen, otherwise an attacker can
// convince us CSS is so critical it's not critical at all.
TEST_F(CriticalSelectorFinderTest, EvidenceOverflow) {
  // Set up pcache entry to be ready to overflow.
  CriticalKeys selectors;
  CriticalKeys::KeyEvidence* evidence = selectors.add_key_evidence();
  evidence->set_key(".a");
  evidence->set_support(kint32max);
  WriteCriticalSelectorSetToPropertyCache(selectors);
  // Now create a new critical selector set and add it repeatedly.
  StringSet new_selectors;
  new_selectors.insert(".a");
  for (int i = 0; i < finder_->SupportInterval(); ++i) {
    Beacon();
    WriteCriticalSelectorsToPropertyCache(new_selectors);
    EXPECT_STREQ(".a", CriticalSelectorsString());
  }
}

// Make sure we don't beacon if we have an empty set of candidate selectors.
TEST_F(CriticalSelectorFinderTest, NoCandidatesNoBeacon) {
  StringSet empty;
  BeaconMetadata last_beacon_metadata =
      finder_->PrepareForBeaconInsertion(empty, rewrite_driver());
  EXPECT_EQ(kDoNotBeacon, last_beacon_metadata.status);
}

TEST_F(CriticalSelectorFinderTest, DontRebeaconBeforeTimeout) {
  Beacon();
  // Now simulate a beacon insertion attempt without timing out.
  WriteBackAndResetDriver();
  factory()->mock_timer()->AdvanceMs(kMinBeaconIntervalMs / 2);
  BeaconMetadata last_beacon_metadata =
      finder_->PrepareForBeaconInsertion(candidates_, rewrite_driver());
  EXPECT_EQ(kDoNotBeacon, last_beacon_metadata.status);
  // But we'll re-beacon if some more time passes.
  Beacon();  // kMinBeaconIntervalMs passes in Beacon() call.
}

// If ShouldReplacePriorResult returns true, then a beacon result
// replaces any previous results.
class UnverifiedCriticalSelectorFinder : public CriticalSelectorFinder {
 public:
  UnverifiedCriticalSelectorFinder(const PropertyCache::Cohort* cohort,
                                   Statistics* stats)
      : CriticalSelectorFinder(cohort, NULL, stats) {}
  virtual ~UnverifiedCriticalSelectorFinder() {}

  virtual int SupportInterval() const { return 10; }

 protected:
  virtual bool ShouldReplacePriorResult() const { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(UnverifiedCriticalSelectorFinder);
};

// Test that unverified results apply.
class UnverifiedSelectorsTest : public CriticalSelectorFinderTest {
 protected:
  virtual BeaconStatus ExpectedBeaconStatus() {
    return kBeaconNoNonce;
  }
  virtual CriticalSelectorFinder* CreateFinder(
      const PropertyCache::Cohort* cohort) {
    return new UnverifiedCriticalSelectorFinder(cohort, statistics());
  }
};

TEST_F(UnverifiedSelectorsTest, NonCandidatesAreStored) {
  Beacon();
  StringSet selectors;
  selectors.insert(".a");
  selectors.insert(".noncandidate");
  selectors.insert("#noncandidate");
  finder_->WriteCriticalSelectorsToPropertyCache(
      selectors, NULL /* no nonce */, rewrite_driver());
  EXPECT_STREQ("#noncandidate,.a,.noncandidate", CriticalSelectorsString());
}

// Each beacon replaces previous results.
TEST_F(UnverifiedSelectorsTest, MultipleResultsReplace) {
  Beacon();
  StringSet selectors;
  selectors.insert(".noncandidate");
  finder_->WriteCriticalSelectorsToPropertyCache(
      selectors, NULL /* no nonce */, rewrite_driver());
  EXPECT_STREQ(".noncandidate", CriticalSelectorsString());

  selectors.clear();
  selectors.insert(".another");
  Beacon();
  finder_->WriteCriticalSelectorsToPropertyCache(
      selectors, NULL /* no nonce */, rewrite_driver());
  EXPECT_STREQ(".another", CriticalSelectorsString());
}

}  // namespace

}  // namespace net_instaweb
