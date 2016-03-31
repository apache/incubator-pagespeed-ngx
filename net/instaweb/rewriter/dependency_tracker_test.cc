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
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/lru_cache.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/semantic_type.h"
#include "pagespeed/kernel/http/user_agent_matcher_test_base.h"

namespace net_instaweb {

namespace {

const char kRequestUrl[] = "http://www.example.com/";

class DependencyTrackerTest : public RewriteTestBase {
 protected:
  void SetUp() override {
    RewriteTestBase::SetUp();

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
  }


  PropertyCache* pcache_;
  PropertyPage* page_;
};

TEST_F(DependencyTrackerTest, BasicOperation) {
  DependencyTracker tracker(rewrite_driver());
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

}  // namespace

}  // namespace net_instaweb
