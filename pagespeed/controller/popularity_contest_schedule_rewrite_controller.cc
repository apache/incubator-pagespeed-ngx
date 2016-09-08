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

#include "pagespeed/controller/popularity_contest_schedule_rewrite_controller.h"

#include <unordered_map>
#include <utility>

#include "base/logging.h"
#include "pagespeed/controller/priority_queue.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

const char PopularityContestScheduleRewriteController::kNumRewritesRequested[] =
    "popularity-contest-num-rewrites-requested";
const char PopularityContestScheduleRewriteController::kNumRewritesSucceeded[] =
    "popularity-contest-num-rewrites-succeeded";
const char PopularityContestScheduleRewriteController::kNumRewritesFailed[] =
    "popularity-contest-num-rewrites-failed";
const char PopularityContestScheduleRewriteController::
    kNumRewritesRejectedQueueSize[] =
        "popularity-contest-num-rewrites-rejected-queue-full";
const char PopularityContestScheduleRewriteController::
    kNumRewritesRejectedInProgress[] =
        "popularity-contest-num-rewrites-rejected-already-running";
const char PopularityContestScheduleRewriteController::kRewriteQueueSize[] =
    "popularity-contest-queue-size";
const char PopularityContestScheduleRewriteController::kNumRewritesRunning[] =
    "popularity-contest-num-rewrites-running";
const char
    PopularityContestScheduleRewriteController::kNumRewritesAwaitingRetry[] =
        "popularity-contest-num-rewrites-awaiting-retry";

PopularityContestScheduleRewriteController::
    PopularityContestScheduleRewriteController(ThreadSystem* thread_system,
                                               Statistics* stats, Timer* timer,
                                               int max_running_rewrites,
                                               int max_queued_rewrites)
    : mutex_(thread_system->NewMutex()),
      timer_(timer),
      running_rewrites_(0),
      max_running_rewrites_(max_running_rewrites),
      max_queued_rewrites_(max_queued_rewrites),
      num_rewrite_requests_(stats->GetTimedVariable(kNumRewritesRequested)),
      num_rewrites_succeeded_(stats->GetTimedVariable(kNumRewritesSucceeded)),
      num_rewrites_failed_(stats->GetTimedVariable(kNumRewritesFailed)),
      num_rewrites_rejected_queue_size_(
          stats->GetTimedVariable(kNumRewritesRejectedQueueSize)),
      num_rewrites_rejected_in_progress_(
          stats->GetTimedVariable(kNumRewritesRejectedInProgress)),
      queue_size_(stats->GetUpDownCounter(kRewriteQueueSize)),
      num_rewrites_running_(stats->GetUpDownCounter(kNumRewritesRunning)),
      num_rewrites_awaiting_retry_(
          stats->GetUpDownCounter(kNumRewritesAwaitingRetry)) {
  // Technically the code should work with these *at* zero, but then what's the
  // point?
  CHECK_GT(max_running_rewrites_, 0);
  CHECK_GT(max_queued_rewrites_, 0);
}

void PopularityContestScheduleRewriteController::InitStats(Statistics* stats) {
  stats->AddTimedVariable(kNumRewritesRequested, Statistics::kDefaultGroup);
  stats->AddTimedVariable(kNumRewritesSucceeded, Statistics::kDefaultGroup);
  stats->AddTimedVariable(kNumRewritesFailed, Statistics::kDefaultGroup);
  stats->AddTimedVariable(kNumRewritesRejectedQueueSize,
                          Statistics::kDefaultGroup);
  stats->AddTimedVariable(kNumRewritesRejectedInProgress,
                          Statistics::kDefaultGroup);
  stats->AddUpDownCounter(kRewriteQueueSize);
  stats->AddUpDownCounter(kNumRewritesRunning);
  stats->AddUpDownCounter(kNumRewritesAwaitingRetry);
}

PopularityContestScheduleRewriteController::
    ~PopularityContestScheduleRewriteController() {
  // TODO(cheesy): I think this might not be cleaned up properly in a
  // multi-process server, since I doubt we have ordering guarantees about
  // workers dying before the supervisor process. I'd like to keep an eye on
  // that, so leaving this here.
  DCHECK(queue_.Empty());
  // Even if queue_ is empty, we may still have leftover AWAITING_RETRY rewrites
  // which must be freed.
  for (const auto& key_and_rewrite : all_rewrites_) {
    Rewrite* rewrite = key_and_rewrite.second;
    // This should always be true for AWAITING_RETRY rewrites.
    DCHECK(rewrite->callback == nullptr);
    if (rewrite->callback != nullptr) {
      // This might be scary if the client has already quit. The only other
      // option would be to delete it.
      rewrite->callback->CallCancel();
    }
    delete rewrite;
  }
}

void PopularityContestScheduleRewriteController::ScheduleRewrite(
    const GoogleString& key, Function* callback) {
  ScopedMutex lock(mutex_.get());
  num_rewrite_requests_->IncBy(1);

  CHECK(callback != nullptr);

  Rewrite* rewrite = GetRewrite(key);
  if (rewrite == nullptr) {
    // Too many queued rewrites.
    num_rewrites_rejected_queue_size_->IncBy(1);
    lock.Release();
    callback->CallCancel();
    return;
  }

  if (rewrite->state == RUNNING) {
    // The key is already being processed by another worker, so cancel this
    // request.
    ++rewrite->saved_priority;
    num_rewrites_rejected_in_progress_->IncBy(1);
    lock.Release();
    callback->CallCancel();
    return;
  }

  Function* old_callback_to_cancel = nullptr;
  if (rewrite->callback != nullptr) {
    // There's already another rewrite queued for this rewrite, so cancel
    // the old request. We always prefer to hold onto the most recent request
    // since workers are not expected to live forever.
    old_callback_to_cancel = rewrite->callback;
    rewrite->callback = nullptr;
  }

  int priority = 1;
  if (rewrite->state == AWAITING_RETRY) {
    // saved_priority is what was left over from the previous failed attempt. It
    // may be zero.
    priority += rewrite->saved_priority;
    rewrite->saved_priority = 0;
    retry_queue_.Remove(rewrite);
    num_rewrites_awaiting_retry_->Add(-1);
  }
  rewrite->state = QUEUED;
  rewrite->callback = callback;
  queue_.IncreasePriority(rewrite, priority);
  Function* callback_to_start = AttemptStartRewrite();

  // Release the lock and run any oustanding callbacks.
  lock.Release();
  if (old_callback_to_cancel != nullptr) {
    old_callback_to_cancel->CallCancel();
  }
  if (callback_to_start != nullptr) {
    callback_to_start->CallRun();
  }
}

void PopularityContestScheduleRewriteController::NotifyRewriteComplete(
    const GoogleString& key) {
  ScopedMutex lock(mutex_.get());
  num_rewrites_succeeded_->IncBy(1);

  Rewrite* rewrite = GetRewrite(key);
  CHECK(rewrite != nullptr) << "NotifyRewriteComplete called for unknown key: "
                            << key;
  CHECK_EQ(rewrite->state, RUNNING) << "NotifyRewriteComplete called for key '"
                                    << key << "' that isn't currently running";
  StopRewrite(rewrite);
  DeleteRewrite(rewrite);
  Function* run_callback = AttemptStartRewrite();

  lock.Release();
  if (run_callback != nullptr) {
    run_callback->CallRun();
  }
}

void PopularityContestScheduleRewriteController::NotifyRewriteFailed(
    const GoogleString& key) {
  ScopedMutex lock(mutex_.get());
  num_rewrites_failed_->IncBy(1);

  Rewrite* rewrite = GetRewrite(key);
  CHECK(rewrite != nullptr) << "NotifyRewriteFailed called for unknown key: "
                             << key;
  CHECK_EQ(rewrite->state, RUNNING) << "NotifyRewriteFailed called for key '"
                                    << key << "' that isn't currently running";
  // Mark the rewrite as stopped but don't delete it. This ensures
  // saved_priority will be set on subsequent retries.
  StopRewrite(rewrite);
  SaveRewriteForRetry(rewrite);
  Function* run_callback = AttemptStartRewrite();

  lock.Release();
  if (run_callback != nullptr) {
    run_callback->CallRun();
  }
}

Function* PopularityContestScheduleRewriteController::AttemptStartRewrite() {
  if (running_rewrites_ >= max_running_rewrites_ || queue_.Empty()) {
    return nullptr;
  }
  const std::pair<Rewrite* const*, int64>& queue_top = queue_.Top();
  Rewrite* rewrite = *queue_top.first;
  rewrite->saved_priority = queue_top.second;
  DCHECK_EQ(rewrite->state, QUEUED);
  queue_.Pop();
  return StartRewrite(rewrite);
}

Function* PopularityContestScheduleRewriteController::StartRewrite(
    Rewrite* rewrite) {
  Function* callback = nullptr;
  DCHECK_LT(running_rewrites_, max_running_rewrites_);
  DCHECK_NE(rewrite->state, RUNNING);
  DCHECK(rewrite->callback != nullptr);
  if (rewrite->callback != nullptr) {
    rewrite->state = RUNNING;
    ++running_rewrites_;
    num_rewrites_running_->Add(1);
    callback = rewrite->callback;
    rewrite->callback = nullptr;
  } else {
    rewrite->state = STOPPED;
  }
  return callback;
}

void PopularityContestScheduleRewriteController::StopRewrite(
    Rewrite* rewrite) {
  DCHECK_EQ(rewrite->state, RUNNING);
  rewrite->state = STOPPED;
  --running_rewrites_;
  num_rewrites_running_->Add(-1);
}

void PopularityContestScheduleRewriteController::SaveRewriteForRetry(
    Rewrite* rewrite) {
  DCHECK_EQ(rewrite->state, STOPPED);
  rewrite->state = AWAITING_RETRY;
  // Insert the item into retry_queue_ with a priority of "negative now".
  // This will cause the queue to be ordered by "oldest first".
  int64 priority = -timer_->NowMs();
  retry_queue_.IncreasePriority(rewrite, priority);
  num_rewrites_awaiting_retry_->Add(1);
}

void PopularityContestScheduleRewriteController::DeleteRewrite(
    const Rewrite* rewrite) {
  RewriteMap::iterator i = all_rewrites_.find(&rewrite->key);
  DCHECK(i != all_rewrites_.end());
  if (i != all_rewrites_.end()) {
    CHECK_EQ(i->second, rewrite);
    all_rewrites_.erase(i);
    queue_size_->Add(-1);
  }
  CHECK_NE(rewrite->state, RUNNING);
  DCHECK(rewrite->callback == nullptr);
  // If rewrite->callback isn't null, we just leak it since we'd need to drop
  // the lock to invoke Cancel.
  delete rewrite;
}

PopularityContestScheduleRewriteController::Rewrite*
PopularityContestScheduleRewriteController::GetRewrite(
    const GoogleString& key) {
  RewriteMap::iterator i = all_rewrites_.find(&key);
  if (i == all_rewrites_.end()) {
    // This rewrite isn't already queued. Do we have an available queue slot?
    ConsiderDroppingRetry();
    if (all_rewrites_.size() >= static_cast<size_t>(max_queued_rewrites_)) {
      return nullptr;
    }
    Rewrite* rewrite = new Rewrite(key);
    std::pair<RewriteMap::iterator, bool> insert_result =
        all_rewrites_.emplace(&rewrite->key, rewrite);
    bool was_inserted = insert_result.second;
    CHECK(was_inserted);
    queue_size_->Add(1);
    i = insert_result.first;
  }
  return i->second;
}

void PopularityContestScheduleRewriteController::ConsiderDroppingRetry() {
  // Pop rewrites out of the retry_queue_ until either the retry queue is empty
  // or there is an available rewrite slot.
  while (all_rewrites_.size() >= static_cast<size_t>(max_queued_rewrites_) &&
         !retry_queue_.Empty()) {
    Rewrite* rewrite = *retry_queue_.Top().first;
    retry_queue_.Pop();
    num_rewrites_awaiting_retry_->Add(-1);
    rewrite->state = STOPPED;
    DeleteRewrite(rewrite);
  }
}

void PopularityContestScheduleRewriteController::SetMaxQueueSizeForTesting(
    int size) {
  ScopedMutex lock(mutex_.get());
  max_queued_rewrites_ = size;
}

}  // namespace net_instaweb
