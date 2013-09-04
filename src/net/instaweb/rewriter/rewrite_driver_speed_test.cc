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
// CPU: Intel Westmere with HyperThreading (3 cores) dL1:32KB dL2:256KB
// Benchmark                       Time(ns)    CPU(ns) Iterations
// --------------------------------------------------------------
// BM_RewriteDriverConstruction      29809      29572      23333

#include <cstddef>

#include "net/instaweb/rewriter/public/rewrite_driver.h"

#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/benchmark.h"

using net_instaweb::RequestContext;

static void BM_RewriteDriverConstruction(int iters) {
  net_instaweb::MockUrlFetcher fetcher;
  net_instaweb::RewriteDriverFactory::Initialize();
  // We're passing NULL here so it will break if you use the test distributed
  // fetcher.
  net_instaweb::TestRewriteDriverFactory factory("/tmp", &fetcher, NULL);
  net_instaweb::RewriteDriverFactory::InitStats(factory.statistics());
  net_instaweb::ServerContext* server_context = factory.CreateServerContext();
  for (int i = 0; i < iters; ++i) {
    net_instaweb::RewriteOptions* options = new net_instaweb::RewriteOptions(
        factory.thread_system());
    options->SetRewriteLevel(net_instaweb::RewriteOptions::kAllFilters);
    net_instaweb::RewriteDriver* driver =
        server_context->NewCustomRewriteDriver(
            options, RequestContext::NewTestRequestContext(
                         factory.thread_system()));
    driver->Cleanup();
  }
  net_instaweb::RewriteDriverFactory::Terminate();
}
BENCHMARK(BM_RewriteDriverConstruction);
