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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DEPENDENCY_TRACKER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DEPENDENCY_TRACKER_H_

#include <memory>

#include "net/instaweb/rewriter/dependencies.pb.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"

namespace net_instaweb {

class AbstractMutex;
class RewriteDriver;
class ServerContext;

// Helper for keeping track of what resources a page depends on --- it helps
// decode information saved in property cache, and to assemble information
// collected from the actual page to update it.
//
// The Register/Report methods are thread-safe.
// TODO(morlovich): Might need merging strategy for stability.
class DependencyTracker {
 public:
  // Note: you must also call SetServerContext on this before operation.
  explicit DependencyTracker(RewriteDriver* driver);
  ~DependencyTracker();

  // This needs to be called to help initialize locking.
  void SetServerContext(ServerContext* server_context);

  // Must be called when parsing pages, after pcache has read in.
  void Start();

  // Must be called after the last flush window has been processed, so we
  // know no further processing.
  void FinishedParsing();

  // Notices the tracker that some filter may be trying to compute a dependency
  // asynchronously. This returns an ID which should then be passed to
  // ReportDependencyCandidate();
  int RegisterDependencyCandidate();

  // Reports result of a dependency computation. You must call
  // ReportDependencyCandidate() for every RegisterDependencyCandidate()
  // call, but it can report that the lead didn't checkout by setting dep to
  // nullptr.
  //
  // 'id' must be the result of the corresponding RegisterDependencyCandidate()
  //      call.
  // 'dep' is the dependency summary for the resource in question, and may be
  //       nullptr if there doesn't seem to be something we want to push there
  //       (e.g. it got removed due to combining). *dep will be copied in,
  //       so only needs to be alive for the duration of the call.
  void ReportDependencyCandidate(int id, const Dependency* dep);

  // This is temporary, nicer API coming later.
  const Dependencies* read_in_info() const { return read_in_info_.get(); }

 private:
  void Clear() LOCKS_EXCLUDED(mutex_);
  void ClearLockHeld() EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void WriteToPropertyCacheIfDone() EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  RewriteDriver* driver_;

  std::unique_ptr<AbstractMutex> mutex_;

  // Info we read in from property cache --- used to make decisions about
  // the current page.
  std::unique_ptr<Dependencies> read_in_info_;

  // Things we compute on the current page.
  // This uses std::map so we can get a stable sort in document order.
  std::map<int, Dependency> computed_info_ GUARDED_BY(mutex_);

  int next_id_ GUARDED_BY(mutex_);
  int outstanding_candidates_ GUARDED_BY(mutex_);

  // Called on ... so we know when we can finally commit results to property
  // cache once number of outstanding candidates goes to 0.
  bool saw_end_ GUARDED_BY(mutex_);

  DISALLOW_COPY_AND_ASSIGN(DependencyTracker);
};


// Compares two Dependency objects based on the order_key field.
class DependencyOrderCompator {
 public:
  bool operator()(const Dependency& a, const Dependency& b);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DEPENDENCY_TRACKER_H_
