/*
 * Copyright 2016 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/rewriter/public/dependency_tracker.h"

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/cache/lru_cache.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/opt/http/request_context.h"

namespace net_instaweb {

namespace {

const char kRequestUrl[] = "http://www.example.com/";

class DependencyTrackerTest : public RewriteTestBase {
 protected:
  void SetUp() override {
    RewriteTestBase::SetUp();
    options()->EnableFilter(RewriteOptions::kExperimentHttp2);

    // Setup pcache.
    pcache_ = rewrite_driver()->server_context()->page_property_cache();
    const PropertyCache::Cohort* deps_cohort =
        SetupCohort(pcache_, RewriteDriver::kDependenciesCohort);
    server_context()->set_dependencies_cohort(deps_cohort);
    ResetDriver();
  }

  void ResetDriver() {
    rewrite_driver()->Clear();
    rewrite_driver()->set_request_context(
        RequestContext::NewTestRequestContext(factory()->thread_system()));
    page_ = NewMockPage(kRequestUrl);
    rewrite_driver()->set_property_page(page_);
    pcache_->Read(page_);
    rewrite_driver()->PropertyCacheSetupDone();
  }

  void TestInRewriteDriver(bool actually_enabled) {
    if (!actually_enabled) {
      options()->DisableFilter(RewriteOptions::kExperimentHttp2);
    }
    DependencyTracker* tracker = rewrite_driver()->dependency_tracker();

    rewrite_driver()->AddFilters();
    rewrite_driver()->StartParse(kTestDomain);
    EXPECT_EQ(nullptr, tracker->read_in_info());

    rewrite_driver()->ParseText("a");
    int c1 = tracker->RegisterDependencyCandidate();
    Dependency d1;
    d1.set_url("1");

    int c2 = tracker->RegisterDependencyCandidate();
    Dependency d2;
    d2.set_url("2");

    rewrite_driver()->Flush();
    tracker->ReportDependencyCandidate(c2, &d2);
    tracker->ReportDependencyCandidate(c1, &d1);

    rewrite_driver()->ParseText("b");

    int c3 = tracker->RegisterDependencyCandidate();
    Dependency d3;
    d3.set_url("3");

    int c4 = tracker->RegisterDependencyCandidate();
    // No result for #4
    tracker->ReportDependencyCandidate(c4, nullptr);

    rewrite_driver()->FinishParse();
    // Nothing written to the cache thus far!
    EXPECT_EQ(0, lru_cache()->num_inserts());
    EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

    tracker->ReportDependencyCandidate(c3, &d3);
    // And now there is --- unless the feature is off.
    EXPECT_EQ(actually_enabled ? 1 : 0, lru_cache()->num_inserts());
    EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

    // Now we are done, and should have things in pcache.
    // (not in the in-memory copy, though, since we didn't re-read yet)
    EXPECT_EQ(nullptr, tracker->read_in_info());

    // Now re-read stuff.
    ResetDriver();
    rewrite_driver()->StartParse(kTestDomain);
    if (actually_enabled) {
      ASSERT_TRUE(tracker->read_in_info() != nullptr);
      ASSERT_EQ(3, tracker->read_in_info()->dependency_size());
      // All nicely sorted.
      EXPECT_EQ("1", tracker->read_in_info()->dependency(0).url());
      EXPECT_EQ("2", tracker->read_in_info()->dependency(1).url());
      EXPECT_EQ("3", tracker->read_in_info()->dependency(2).url());

      rewrite_driver()->FinishParse();
      // Cleaned up afterwards, though.
      ASSERT_TRUE(tracker->read_in_info() == nullptr);
    } else {
      ASSERT_TRUE(tracker->read_in_info() == nullptr);
    }
  }

  PropertyCache* pcache_;
  PropertyPage* page_;
};

TEST_F(DependencyTrackerTest, BasicOperation) {
  DependencyTracker tracker(rewrite_driver());
  tracker.SetServerContext(server_context());
  tracker.Start();
  EXPECT_EQ(nullptr, tracker.read_in_info());

  int c1 = tracker.RegisterDependencyCandidate();
  Dependency d1;
  d1.set_url("1");

  int c2 = tracker.RegisterDependencyCandidate();
  Dependency d2;
  d2.set_url("2");

  tracker.ReportDependencyCandidate(c2, &d2);
  tracker.ReportDependencyCandidate(c1, &d1);
  // Nothing ongoing atm, but no FinishedParsing yet, either.

  int c3 = tracker.RegisterDependencyCandidate();
  Dependency d3;
  d3.set_url("3");

  int c4 = tracker.RegisterDependencyCandidate();
  // No result for #4

  int c5 = tracker.RegisterDependencyCandidate();
  Dependency d5;
  d5.set_url("5");

  tracker.ReportDependencyCandidate(c4, nullptr);
  tracker.FinishedParsing();

  tracker.ReportDependencyCandidate(c3, &d3);
  // Nothing written to the cache thus far!
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  tracker.ReportDependencyCandidate(c5, &d5);

  // And now there is.
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  // Now we are done, and should have things in pcache.
  // (not in the in-memory copy, though, since we didn't re-read yet)
  EXPECT_EQ(nullptr, tracker.read_in_info());

  // Now actually create the in-memory copy, by reading back from pcache.
  ResetDriver();
  tracker.Start();
  ASSERT_TRUE(tracker.read_in_info() != nullptr);
  ASSERT_EQ(4, tracker.read_in_info()->dependency_size());
  // All nicely sorted.
  EXPECT_EQ("1", tracker.read_in_info()->dependency(0).url());
  EXPECT_EQ("2", tracker.read_in_info()->dependency(1).url());
  EXPECT_EQ("3", tracker.read_in_info()->dependency(2).url());
  EXPECT_EQ("5", tracker.read_in_info()->dependency(3).url());
}

TEST_F(DependencyTrackerTest, ViaRewriteDriver) {
  TestInRewriteDriver(true);
}

TEST_F(DependencyTrackerTest, ViaRewriteDriverOff) {
  TestInRewriteDriver(false);
}

TEST_F(DependencyTrackerTest, Comparator) {
  Dependency de;

  Dependency de1;
  de1.add_order_key(1);

  Dependency de12;
  de12.add_order_key(1);
  de12.add_order_key(2);

  Dependency de121;
  de121.add_order_key(1);
  de121.add_order_key(2);
  de121.add_order_key(1);

  Dependency de122;
  de122.add_order_key(1);
  de122.add_order_key(2);
  de122.add_order_key(2);

  Dependency de13;
  de13.add_order_key(1);
  de13.add_order_key(3);

  Dependency de2;
  de2.add_order_key(2);

  DependencyOrderCompator less;
  EXPECT_FALSE(less(de, de));
  EXPECT_TRUE(less(de, de1));
  EXPECT_TRUE(less(de, de12));
  EXPECT_TRUE(less(de, de121));
  EXPECT_TRUE(less(de, de122));
  EXPECT_TRUE(less(de, de13));
  EXPECT_TRUE(less(de, de2));

  EXPECT_FALSE(less(de1, de));
  EXPECT_FALSE(less(de1, de1));
  EXPECT_TRUE(less(de1, de12));
  EXPECT_TRUE(less(de1, de121));
  EXPECT_TRUE(less(de1, de122));
  EXPECT_TRUE(less(de1, de13));
  EXPECT_TRUE(less(de1, de2));

  EXPECT_FALSE(less(de12, de));
  EXPECT_FALSE(less(de12, de1));
  EXPECT_FALSE(less(de12, de12));
  EXPECT_TRUE(less(de12, de121));
  EXPECT_TRUE(less(de12, de122));
  EXPECT_TRUE(less(de12, de13));
  EXPECT_TRUE(less(de12, de2));

  EXPECT_FALSE(less(de121, de));
  EXPECT_FALSE(less(de121, de1));
  EXPECT_FALSE(less(de121, de12));
  EXPECT_FALSE(less(de121, de121));
  EXPECT_TRUE(less(de121, de122));
  EXPECT_TRUE(less(de121, de13));
  EXPECT_TRUE(less(de121, de2));

  EXPECT_FALSE(less(de122, de));
  EXPECT_FALSE(less(de122, de1));
  EXPECT_FALSE(less(de122, de12));
  EXPECT_FALSE(less(de122, de121));
  EXPECT_FALSE(less(de122, de122));
  EXPECT_TRUE(less(de122, de13));
  EXPECT_TRUE(less(de122, de2));

  EXPECT_FALSE(less(de13, de));
  EXPECT_FALSE(less(de13, de1));
  EXPECT_FALSE(less(de13, de12));
  EXPECT_FALSE(less(de13, de121));
  EXPECT_FALSE(less(de13, de122));
  EXPECT_FALSE(less(de13, de13));
  EXPECT_TRUE(less(de13, de2));

  EXPECT_FALSE(less(de2, de));
  EXPECT_FALSE(less(de2, de1));
  EXPECT_FALSE(less(de2, de12));
  EXPECT_FALSE(less(de2, de121));
  EXPECT_FALSE(less(de2, de122));
  EXPECT_FALSE(less(de2, de13));
  EXPECT_FALSE(less(de2, de2));
}

}  // namespace

}  // namespace net_instaweb
