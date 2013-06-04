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

// Author: nikhilmadan@google.com (Nikhil Madan)
//
// CPU: Intel Sandybridge with HyperThreading (6 cores) dL1:32KB dL2:256KB
// Benchmark                              Time(ns)    CPU(ns) Iterations
// ---------------------------------------------------------------------
// BM_DomainLawyerIsAuthorizedAllowStar        398        398    1707317
// BM_DomainLawyerIsAuthorizedAllowAll           3          3  259259259

#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/util/public/benchmark.h"
#include "net/instaweb/util/public/google_url.h"
#include "pagespeed/kernel/base/null_message_handler.h"

void RunIsDomainAuthorizedIters(const net_instaweb::DomainLawyer& lawyer,
                                int iters) {
  net_instaweb::GoogleUrl base_url("http://www.x.com/a/b/c/d/e/f");
  net_instaweb::GoogleUrl in_url("http://www.y.com/a/b/c/d/e/f");
  for (int i = 0; i < iters; ++i) {
    lawyer.IsDomainAuthorized(base_url, in_url);
  }
}

static void BM_DomainLawyerIsAuthorizedAllowStar(int iters) {
  net_instaweb::NullMessageHandler handler;
  net_instaweb::DomainLawyer lawyer;
  lawyer.AddDomain("http://*", &handler);
  RunIsDomainAuthorizedIters(lawyer, iters);
}

static void BM_DomainLawyerIsAuthorizedAllowAll(int iters) {
  net_instaweb::NullMessageHandler handler;
  net_instaweb::DomainLawyer lawyer;
  lawyer.AddDomain("*", &handler);
  RunIsDomainAuthorizedIters(lawyer, iters);
}

BENCHMARK(BM_DomainLawyerIsAuthorizedAllowStar);
BENCHMARK(BM_DomainLawyerIsAuthorizedAllowAll);
