/*
 * Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)
//
// CPU: Intel Ivybridge with HyperThreading (6 cores) dL1:32KB dL2:256KB dL3:12MB  // NOLINT
// Benchmark                       Time(ns)    CPU(ns) Iterations
// --------------------------------------------------------------
// BM_RewriteDriverConstruction      36496      36538      19067
// BM_EmptyFilter                  4355548    4397615        154
//
// Thus, about 4 ms per 35k file, running all filters.
//
// Disclaimer: comparing runs over time and across different machines
// can be misleading.  When contemplating an algorithm change, always do
// interleaved runs with the old & new algorithm.

#include <memory>

#include "net/instaweb/rewriter/public/rewrite_driver.h"

#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/critical_selector_finder.h"
#include "net/instaweb/rewriter/public/process_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "pagespeed/kernel/base/benchmark.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"
#include "pagespeed/opt/http/property_cache.h"

namespace net_instaweb {
namespace {

ProcessContext process_context;

class SpeedTestContext {
 public:
  SpeedTestContext() {
    StopBenchmarkTiming();
    RewriteDriverFactory::Initialize();
    factory_.reset(new TestRewriteDriverFactory(
        process_context, "/tmp", &fetcher_));
    TestRewriteDriverFactory::InitStats(factory_->statistics());
    server_context_ = factory_->CreateServerContext();
    StartBenchmarkTiming();
  }

  ~SpeedTestContext() {
    factory_.reset();  // Must precede ProcessContext destruction.
    RewriteDriverFactory::Terminate();
  }

  RewriteDriver* NewDriver(RewriteOptions* options) {
    return server_context_->NewCustomRewriteDriver(
        options, RequestContext::NewTestRequestContext(
            factory_->thread_system()));
  }

  TestRewriteDriverFactory* factory() { return factory_.get(); }
  ServerContext* server_context() { return server_context_; }

  // Setup statistics for the given cohort and add it to the give PropertyCache.
  const PropertyCache::Cohort*  SetupCohort(
      PropertyCache* cache, const GoogleString& cohort) {
    return factory_->SetupCohort(cache, cohort);
  }

  // Returns a new mock property page for the page property cache.
  MockPropertyPage* NewMockPage(StringPiece url) {
    return new MockPropertyPage(
        server_context_->thread_system(),
        server_context_->page_property_cache(),
        url, "hash",
        UserAgentMatcher::DeviceTypeSuffix(UserAgentMatcher::kDesktop));
  }

 private:
  MockUrlFetcher fetcher_;
  std::unique_ptr<TestRewriteDriverFactory> factory_;
  ServerContext* server_context_;
};

static void BM_RewriteDriverConstruction(int iters) {
  SpeedTestContext speed_test_context;
  for (int i = 0; i < iters; ++i) {
    RewriteOptions* options = new RewriteOptions(
        speed_test_context.factory()->thread_system());
    options->SetRewriteLevel(RewriteOptions::kAllFilters);
    RewriteDriver* driver = speed_test_context.NewDriver(options);
    driver->Cleanup();
  }
}
BENCHMARK(BM_RewriteDriverConstruction);

// This measures the speed of the HTML parsing & filter dispatch mechanism.
static void BM_EmptyFilter(int iters) {
  SpeedTestContext speed_test_context;

  StopBenchmarkTiming();

  // Set up the cohorts which are needed for some filters to operate properly.
  ServerContext* server_context = speed_test_context.server_context();
  PropertyCache* page_property_cache = server_context->page_property_cache();
  const PropertyCache::Cohort* beacon_cohort =
      speed_test_context.SetupCohort(page_property_cache,
                                     RewriteDriver::kBeaconCohort);
  server_context->set_beacon_cohort(beacon_cohort);
  const PropertyCache::Cohort* dom_cohort =
      speed_test_context.SetupCohort(page_property_cache,
                                     RewriteDriver::kDomCohort);
  server_context->set_dom_cohort(dom_cohort);

  // Set up the driver to enable all filters.
  std::unique_ptr<RewriteOptions> options(new RewriteOptions(
      speed_test_context.factory()->thread_system()));
  options->SetRewriteLevel(RewriteOptions::kAllFilters);

  GoogleString html;
  for (int i = 0; i < 1000; ++i) {
    html += "<div id='x' class='y'> x y z </div>";  // 35 bytes
  }

  StartBenchmarkTiming();

  for (int i = 0; i < iters; ++i) {
    RewriteDriver* driver = speed_test_context.NewDriver(options->Clone());

    // Critical css needs its finder and pcache to work, and of course we
    // don't want to accumulate everything in memory after every file, so we
    // set it up fresh.
    driver->set_property_page(speed_test_context.NewMockPage(
        "http://example.com"));
    page_property_cache->Read(driver->property_page());

    // Set up and register a beacon finder.
    CriticalSelectorFinder* finder = new BeaconCriticalSelectorFinder(
        server_context->beacon_cohort(),
        speed_test_context.factory()->nonce_generator(),
        server_context->statistics());
    server_context->set_critical_selector_finder(finder);

    driver->StartParse("http://example.com/index.html");
    driver->ParseText("<html><head></head><body>");
    driver->Flush();
    driver->ParseText(html);  // 35k bytes
    driver->Flush();
    driver->ParseText("</body></html>");
    driver->FinishParse();
  }
}
BENCHMARK(BM_EmptyFilter);

}  // namespace
}  // namespace net_instaweb
