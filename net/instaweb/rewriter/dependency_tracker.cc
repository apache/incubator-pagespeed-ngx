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

#include <memory>

#include "net/instaweb/rewriter/dependencies.pb.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/semantic_type.h"
#include "pagespeed/opt/http/fallback_property_page.h"

namespace net_instaweb {

const char kDepProp[] = "dependencies";

DependencyTracker::DependencyTracker(RewriteDriver* driver)
    : driver_(driver) {
  Clear();
}

DependencyTracker::~DependencyTracker() {
  DCHECK_EQ(outstanding_candidates_, 0);
}

void DependencyTracker::Clear() {
  read_in_info_.reset();
  computed_info_.clear();
  next_id_ = 0;
  outstanding_candidates_ = 0;
  saw_end_ = false;
}

void DependencyTracker::Start() {
  Clear();

  PropertyCacheDecodeResult status;
  read_in_info_.reset(DecodeFromPropertyCache<Dependencies>(
        driver_->server_context()->page_property_cache(),
        driver_->fallback_property_page(),
        driver_->server_context()->dependencies_cohort(),
        kDepProp,
        -1 /* no ttl checking*/,
        &status));
}

void DependencyTracker::FinishedParsing() {
  saw_end_ = true;
  WriteToPropertyCacheIfDone();
}

int DependencyTracker::RegisterDependencyCandidate() {
  ++outstanding_candidates_;
  return next_id_++;
}

void DependencyTracker::ReportDependencyCandidate(
    int id, const Dependency* dep) {
  if (dep != nullptr) {
    computed_info_[id] = *dep;
  }
  --outstanding_candidates_;
  WriteToPropertyCacheIfDone();
}

void DependencyTracker::WriteToPropertyCacheIfDone() {
  if (outstanding_candidates_ > 0 || !saw_end_) {
    return;
  }

  // Make a proto, and write it out to the pcache.
  Dependencies deps;
  for (const std::pair<const int, Dependency>& key_val : computed_info_) {
    *deps.add_dependency() = key_val.second;
  }
  UpdateInPropertyCache(deps, driver_,
                        driver_->server_context()->dependencies_cohort(),
                        kDepProp, true /* write out the cohort */);
}

}  // namespace net_instaweb

