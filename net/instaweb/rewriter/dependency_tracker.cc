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
#include <utility>

#include "base/logging.h"
#include "net/instaweb/rewriter/dependencies.pb.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/opt/http/fallback_property_page.h"

namespace net_instaweb {

const char kDepProp[] = "dependencies";

DependencyTracker::DependencyTracker(RewriteDriver* driver)
    : driver_(driver) {
}

DependencyTracker::~DependencyTracker() {
  DCHECK_EQ(outstanding_candidates_, 0);
}

void DependencyTracker::SetServerContext(ServerContext* server_context) {
  mutex_.reset(server_context->thread_system()->NewMutex());
  Clear();
}

void DependencyTracker::Clear() {
  ScopedMutex hold(mutex_.get());
  ClearLockHeld();
}

void DependencyTracker::ClearLockHeld() {
  read_in_info_.reset();
  computed_info_.clear();
  next_id_ = 0;
  outstanding_candidates_ = 0;
  saw_end_ = false;
}

void DependencyTracker::Start() {
  Clear();

  if (driver_->options()->NeedsDependenciesCohort()) {
    PropertyCacheDecodeResult status;
    read_in_info_.reset(DecodeFromPropertyCache<Dependencies>(
          driver_->server_context()->page_property_cache(),
          driver_->fallback_property_page(),
          driver_->server_context()->dependencies_cohort(),
          kDepProp,
          -1 /* no ttl checking*/,
          &status));
  }
}

void DependencyTracker::FinishedParsing() {
  ScopedMutex hold(mutex_.get());
  saw_end_ = true;
  WriteToPropertyCacheIfDone();
}

int DependencyTracker::RegisterDependencyCandidate() {
  ScopedMutex hold(mutex_.get());
  ++outstanding_candidates_;
  return next_id_++;
}

void DependencyTracker::ReportDependencyCandidate(
    int id, const Dependency* dep) {
  ScopedMutex hold(mutex_.get());
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

  if (driver_->options()->NeedsDependenciesCohort()) {
    // Make a proto, and write it out to the pcache.
    Dependencies deps;
    for (const std::pair<const int, Dependency>& key_val : computed_info_) {
      *deps.add_dependency() = key_val.second;
    }
    UpdateInPropertyCache(deps, driver_,
                          driver_->server_context()->dependencies_cohort(),
                          kDepProp, true /* write out the cohort */);
  }

  // All done, make sure we have nothing hanging around in case we
  // have non-HTML uses.
  ClearLockHeld();
}

bool DependencyOrderCompator::operator()(
    const Dependency& a, const Dependency& b) {
  int pos = 0;
  while (pos < a.order_key_size() && pos < b.order_key_size()) {
    if (a.order_key(pos) < b.order_key(pos)) {
      return true;
    }
    if (a.order_key(pos) > b.order_key(pos)) {
      return false;
    }

    ++pos;
  }

  if (pos == a.order_key_size()) {
    // a at end.
    if (pos == b.order_key_size()) {
      // b also at end -> they're the same.
      return false;
    } else {
      // not at end -> a is prefix of b, so a < b
      return true;
    }
  } else {
    // a not at end, b at end => b is a prefix of a, so b < a, so
    // clearly not a < b
    return false;
  }
}

}  // namespace net_instaweb
