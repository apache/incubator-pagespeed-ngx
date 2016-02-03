// Copyright 2016 Google Inc.
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
// Author: cheesy@google.com (Steve Hill)

#ifndef PAGESPEED_CONTROLLER_POPULARITY_CONTEST_SCHEDULE_REWRITE_CONTROLLER_H_
#define PAGESPEED_CONTROLLER_POPULARITY_CONTEST_SCHEDULE_REWRITE_CONTROLLER_H_

#include <cstddef>
#include <unordered_map>

#include "pagespeed/controller/priority_queue.h"
#include "pagespeed/controller/schedule_rewrite_controller.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"

// Implementation of ScheduleRewriteController that uses priority queue to
// process rewrites in the order of most requested. Gurantees that at most one
// client will be waiting for a given key. Also limits the number of queued
// rewrites and the number of rewrites running in parallel.
//
// Every request is tracked in a Rewrite object, the lifetime of which is
// described by the following state digram:
//
//      begin
//        |
//  +-----v-----+
//  |           | Queue full
//  |  STOPPED  +-----------> delete <---------+
//  |           |                              |
//  +-----+-----+                              |
//        |                                    |
// ScheduleRewrite()                           | Other Rewrite needs
//        |    +----+                          | slot in queue and
//        |    |    | ScheduleRewrite()        | this is oldest rewrite
//  +-----v----+-+  | (increments priority     | in AWAITING_RETRY.
//  |            <--+  discards old request)   |
//  |   QUEUED   |                             |
//  |            <--+    ScheduleRewrite()  +--+-------+
//  +-----+------+  |  (increments priority)|          |
//        |         +-----------------------+ AWAITING |
//     Pop Queue                            |  RETRY   |
// (when most requested)                    |          |
//        |                                 +----^-----+
//        |   +-----+                            |
//        |   |     | ScheduleRewrite()          |
//  +-----v---+-+   | (increments priority       |
//  |           |   |  rejects new request)      |
//  |  RUNNING  <---+                            |
//  |           |                                |
//  +--+----+---+                                |
//     |    |                                    |
//     |    +------------------------------------+
//     |           NotifyFailure()
//     |
//     +-----------------------------> delete
//                 NotifySuccess()

namespace net_instaweb {

class PopularityContestScheduleRewriteController
    : public ScheduleRewriteController {
 public:
  static const char kNumRewritesRequested[];
  static const char kNumRewritesSucceeded[];
  static const char kNumRewritesFailed[];
  static const char kNumRewritesRejectedQueueSize[];
  static const char kNumRewritesRejectedInProgress[];
  static const char kRewriteQueueSize[];
  static const char kNumRewritesRunning[];
  static const char kNumRewritesAwaitingRetry[];

  // max_running_rewrites and max_queued_rewrites are CHECKed to be > 0.
  // Since max_running_rewrites is implicity bounded by the queue size,
  // you probably want queued >= running, but this isn't enforced by the code.
  PopularityContestScheduleRewriteController(ThreadSystem* thread_system,
                                             Statistics* statistics,
                                             Timer* timer,
                                             int max_running_rewrites,
                                             int max_queued_rewrites);
  virtual ~PopularityContestScheduleRewriteController();

  // ScheduleRewriteController interface.
  void ScheduleRewrite(const GoogleString& key, Function* callback) override;
  void NotifyRewriteComplete(const GoogleString& key) override;
  void NotifyRewriteFailed(const GoogleString& key) override;

  static void InitStats(Statistics* stats);

 private:
  enum RewriteState {
    STOPPED,
    QUEUED,
    RUNNING,
    AWAITING_RETRY,
  };

  struct Rewrite {
    Rewrite(const GoogleString& k)
        : key(k), saved_priority(0), callback(nullptr), state(STOPPED) {}
    GoogleString key;
    int saved_priority;
    Function* callback;
    RewriteState state;
  };

  struct StringPtrHash {
    size_t operator()(const GoogleString* x) const {
      return std::hash<GoogleString>()(*x);
    }
  };

  struct StringPtrEq {
    bool operator()(const GoogleString* a, const GoogleString* b) const {
      return (a == b || *a == *b);
    }
  };

  typedef std::unordered_map<const GoogleString*, Rewrite*,
                             StringPtrHash, StringPtrEq>
      RewriteMap;

  // Consider starting the next rewrite in queue_, depending on available
  // resources. Returns either nullptr or a Function which must be run
  // *WITHOUT* mutex_ locked.
  Function* AttemptStartRewrite()
      EXCLUSIVE_LOCKS_REQUIRED(mutex_) WARN_UNUSED_RESULT;

  // Start the supplied rewrite, ie: Update bookkeeping.
  // Returns the callback from the rewrite, which must be run *WITHOUT*
  // mutex_ locked.
  Function* StartRewrite(Rewrite* rewrite)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_) WARN_UNUSED_RESULT;

  // Stop the supplied rewrite. Undoes the bookkeeping from Start.
  void StopRewrite(Rewrite* rewrite) EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Save the Rewrite so it may be retried later. The Rewrite may later be
  // discarded if the queue fills up.
  void SaveRewriteForRetry(Rewrite* rewrite) EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // If there are no remaining slots in the queue, will drop the oldest Rewrite
  // on the retry queue.
  void ConsiderDroppingRetry() EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Retrieve or create a Rewrite by key from all_rewrites_. The return value
  // is protected by mutex_ which should remain held until you are done with
  // the Rewrite.
  Rewrite* GetRewrite(const GoogleString& key) EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // delete a Rewrite and remove it from all_rewrites_.
  void DeleteRewrite(const Rewrite* rewrite) EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Re-assign max_queued_rewrites. For use only in tests.
  void SetMaxQueueSizeForTesting(int size) LOCKS_EXCLUDED(mutex_);

  scoped_ptr<AbstractMutex> mutex_;
  // All known rewrites, indexed by Rewrite->key. Key pointers are all owned
  // by their respective Rewrite.
  RewriteMap all_rewrites_ GUARDED_BY(mutex_);
  // No additional templates required on queue_; it uses pointer hash/eq.
  PriorityQueue<Rewrite*> queue_ GUARDED_BY(mutex_);

  // The retry queue is ordered by negative time last seen. This allows us to
  // quickly discard the oldest items, if we need to.
  PriorityQueue<Rewrite*> retry_queue_ GUARDED_BY(mutex_);

  Timer* timer_;

  int running_rewrites_ GUARDED_BY(mutex_);
  const int max_running_rewrites_;
  // max_queued_rewrites_ can't be const because of SetMaxQueueSizeForTesting.
  int max_queued_rewrites_ GUARDED_BY(mutex_);

  TimedVariable* num_rewrite_requests_;
  TimedVariable* num_rewrites_succeeded_;
  TimedVariable* num_rewrites_failed_;
  TimedVariable* num_rewrites_rejected_queue_size_;
  TimedVariable* num_rewrites_rejected_in_progress_;
  UpDownCounter* queue_size_;
  UpDownCounter* num_rewrites_running_;
  UpDownCounter* num_rewrites_awaiting_retry_;

  friend class PopularityContestScheduleRewriteControllerTest;

  DISALLOW_COPY_AND_ASSIGN(PopularityContestScheduleRewriteController);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_POPULARITY_CONTEST_SCHEDULE_REWRITE_CONTROLLER_H_
