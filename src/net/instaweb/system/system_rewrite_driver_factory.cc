// Copyright 2013 Google Inc.
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
//
// Author: jefftk@google.com (Jeff Kaufman)

#include "net/instaweb/system/public/system_rewrite_driver_factory.h"

#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/system/public/system_caches.h"

namespace net_instaweb {

class ThreadSystem;

SystemRewriteDriverFactory::SystemRewriteDriverFactory(
    ThreadSystem* thread_system)
    : RewriteDriverFactory(thread_system) {
}

void SystemRewriteDriverFactory::InitStats(Statistics* statistics) {
  // Init standard PSOL stats.
  RewriteDriverFactory::InitStats(statistics);

  // Init System-specific stats.
  SerfUrlAsyncFetcher::InitStats(statistics);
  SystemCaches::InitStats(statistics);
  PropertyCache::InitCohortStats(RewriteDriver::kBeaconCohort, statistics);
  PropertyCache::InitCohortStats(RewriteDriver::kDomCohort, statistics);
}

}  // namespace net_instaweb
