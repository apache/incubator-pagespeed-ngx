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

// Author: jmarantz@google.com (Joshua Marantz)

#include "pagespeed/kernel/cache/purge_context.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/atomic_bool.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/named_lock_manager.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/time_util.h"
#include "pagespeed/kernel/cache/lru_cache_base.h"
#include "pagespeed/kernel/thread/scheduler.h"
#include "pagespeed/kernel/util/copy_on_write.h"

namespace net_instaweb {

namespace {

const int64 kStealLockAfterMs = 2 * Timer::kSecondMs;
const int64 kTimeoutMs = 3 * Timer::kSecondMs;
const int kMaxContentionRetries = 2;

}  // namespace

const char PurgeContext::kCancellations[]        = "purge_cancellations";
const char PurgeContext::kContentions[]          = "purge_contentions";
const char PurgeContext::kFileParseFailures[]    = "purge_file_parse_failures";
const char PurgeContext::kFileStats[]            = "purge_file_stats";
const char PurgeContext::kFileWriteFailures[]    = "purge_file_write_failures";
const char PurgeContext::kFileWrites[]           = "purge_file_writes";
const char PurgeContext::kPurgeIndex[]           = "purge_index";

// TODO(jmarantz): make it possible to avoid showing this implementation detail
// in the statistics page.
const char PurgeContext::kPurgePollTimestampMs[] = "_purge_poll_timestamp_ms";

namespace {

// Wrapper class around an UpDownCounter which makes it guaranteed to be
// locally consistent, even if the backing UpDownCounter is non-functional
// due to a shared-memory failure or statistics being turned off.
class BackupUpDownCounter : public UpDownCounter {
 public:
  // Takes ownership of mutex.
  BackupUpDownCounter(UpDownCounter* counter, AbstractMutex* mutex)
      : counter_(counter),
        mutex_(mutex),
        local_counter_(0) {
  }

  virtual int64 Get() const {
    // Don't take the lock for local_counter unless we have actually set some
    // value in the past, yet we can't retrieve it from the real UpDownCounter.
    bool was_set = was_set_.value();
    int64 val = counter_->Get();
    if (was_set && (val == 0)) {
      ScopedMutex lock(mutex_.get());
      val = local_counter_;
    }
    return val;
  }

  virtual StringPiece GetName() const { return counter_->GetName(); }

  virtual void Set(int64 value) {
    counter_->Set(value);
    {
      ScopedMutex lock(mutex_.get());
      local_counter_ = value;
      was_set_.set_value(true);
    }
  }

  virtual int64 AddHelper(int delta) {
    LOG(DFATAL) << "AddHelper is not used in PurgeContext";
    return 0;
  }

 private:
  UpDownCounter* counter_;
  scoped_ptr<AbstractMutex> mutex_;
  int64 local_counter_ GUARDED_BY(mutex_);
  AtomicBool was_set_;
};

}  // namespace

PurgeContext::PurgeContext(StringPiece filename,
                           FileSystem* file_system,
                           Timer* timer,
                           int max_bytes_in_cache,
                           ThreadSystem* thread_system,
                           NamedLockManager* lock_manager,
                           Scheduler* scheduler,
                           Statistics* statistics,
                           MessageHandler* handler)
    : filename_(filename.data(), filename.size()),
      interprocess_lock_(lock_manager->CreateNamedLock(LockName())),
      file_system_(file_system),
      timer_(timer),
      statistics_(statistics),
      mutex_(thread_system->NewMutex()),
      pending_purges_(max_bytes_in_cache),
      local_purge_index_(0),
      num_consecutive_failures_(0),
      waiting_for_interprocess_lock_(false),
      reading_(false),
      enable_purge_(true),
      max_bytes_in_cache_(max_bytes_in_cache),
      request_batching_delay_ms_(0),
      cancellations_(statistics->GetVariable(kCancellations)),
      contentions_(statistics->GetVariable(kContentions)),
      file_parse_failures_(statistics->GetVariable(kFileParseFailures)),
      file_stats_(statistics->GetVariable(kFileStats)),
      file_write_failures_(statistics->GetVariable(kFileWriteFailures)),
      file_writes_(statistics->GetVariable(kFileWrites)),
      purge_index_(statistics->GetVariable(kPurgeIndex)),
      purge_poll_timestamp_ms_(new BackupUpDownCounter(
          statistics->GetUpDownCounter(kPurgePollTimestampMs),
          thread_system->NewMutex())),
      scheduler_(scheduler),
      message_handler_(handler) {
  purge_set_.MakeWriteable()->set_max_size(max_bytes_in_cache_);
}

PurgeContext::~PurgeContext() {
}

void PurgeContext::InitStats(Statistics* statistics) {
  statistics->AddVariable(kCancellations);
  statistics->AddVariable(kContentions);
  statistics->AddVariable(kFileParseFailures);
  statistics->AddVariable(kFileStats);
  statistics->AddVariable(kFileWrites);
  statistics->AddVariable(kFileWriteFailures);
  statistics->AddVariable(kPurgeIndex);
  statistics->AddUpDownCounter(kPurgePollTimestampMs);
}

bool PurgeContext::ParseAndValidateTimestamp(
    StringPiece time_string, int64 now_ms, int64* timestamp_ms) {
  if (!StringToInt64(time_string, timestamp_ms)) {
    message_handler_->Info(filename_.c_str(), 1,
                           "Invalidation timestamp (%s) not parsed as int64",
                           time_string.as_string().c_str());
    return false;
  } else if ((*timestamp_ms != PurgeSet::kInitialTimestampMs) &&
             ((*timestamp_ms < 0) ||
              (*timestamp_ms > now_ms + PurgeSet::kClockSkewAllowanceMs))) {
    GoogleString converted_time_string;
    ConvertTimeToString(*timestamp_ms, &converted_time_string);
    message_handler_->Info(filename_.c_str(), 1,
                           "Invalidation timestamp (%s) in the future: %s",
                           time_string.as_string().c_str(),
                           converted_time_string.c_str());
    return false;
  }
  return true;
}

// Parses the cache purge file.
void PurgeContext::ReadPurgeFile(PurgeSet* purges_from_file) {
  GoogleString buffer;

  file_stats_->Add(1);
  NullMessageHandler null_handler;

  // Prior to mod_pagespeed 1.8, the cache.flush file's contents were not
  // significant, and generally empty, and only the timestamp of the file
  // itself was important, meaning "wipe everything out of the cache predating
  // that timestamp."
  if (!enable_purge_) {
    int64 timestamp_sec;
    if (file_system_->Mtime(filename_, &timestamp_sec, &null_handler)) {
      int64 timestamp_ms = timestamp_sec * Timer::kSecondMs;
      purges_from_file->UpdateGlobalInvalidationTimestampMs(timestamp_ms);
    }
    return;
  }

  if (!file_system_->ReadFile(filename_.c_str(), &buffer, &null_handler)) {
    // If the file simply doesn't exist, that's a 'successful' read.  It's
    // fine for there to be no cache file and no invalidation data, and thus
    // we swallow file-not-found messages with NullMessageHandler.
    return;
  }
  StringPieceVector lines;
  SplitStringPieceToVector(buffer, "\n", &lines, true);
  int64 timestamp_ms = 0;
  int64 now_ms = timer_->NowMs();

  // The first line should contain the global invalidation timestamp,
  // though we'll just silently leave the invalidation timestamp unchanged
  // if the file was empty.
  if (lines.empty() || !ParseAndValidateTimestamp(lines[0], now_ms,
                                                  &timestamp_ms)) {
    file_parse_failures_->Add(1);
    return;
  }
  purges_from_file->UpdateGlobalInvalidationTimestampMs(timestamp_ms);

  for (int i = 1, n = lines.size(); i < n; ++i) {
    // Each line is in the form "TIMESTAMP_MS URL".  The url
    // has no character restrictions in this file format, and terminates at
    // newline.  The timestamp is a 64-bit decimal number measured in
    // milliseconds since 1970.
    StringPiece line = lines[i];
    if (!line.empty()) {  // skip empty lines but include them in line-count.
      stringpiece_ssize_type pos = line.find(' ');
      if ((pos == StringPiece::npos) ||
          !ParseAndValidateTimestamp(line.substr(0, pos), now_ms,
                                     &timestamp_ms)) {
        file_parse_failures_->Add(1);
      } else {
        StringPiece url = line.substr(pos + 1);
        purges_from_file->Put(url.as_string(), timestamp_ms);
      }
    }
  }
}

// While still holding the interprocess lock, verify that the bytes in
// the file are the ones we wrote.  If another process stole the lock
// and overwrote our bytes, we'll simply schedule another try, which will
// merge in whatever results they had written.
bool PurgeContext::Verify(const GoogleString& expected_purge_file_contents) {
  GoogleString verify;
  return (file_system_->ReadFile(filename_.c_str(), &verify,
                                 message_handler_) &&
          (verify == expected_purge_file_contents));
}

void PurgeContext::UpdateCachePurgeFile() {
  // Use a global lock to perform an atomic read/modify/write.
  //
  // Note that the lock is advisory, and if it gets stolen, then we'll
  // have a contended update to the file and potentially lose some
  // purge records, at least temporarily.
  DCHECK(interprocess_lock_->Held());
  DCHECK(waiting_for_interprocess_lock_);
  PurgeSet purges_from_file(max_bytes_in_cache_);
  PurgeSet return_purges(max_bytes_in_cache_);
  PurgeCallbackVector callbacks;
  bool lock_and_update = false;
  bool success = true;
  int failures = 0;

  // Initiate a read/modify/write/verify sequence while holding
  // interprocess_lock_.  Note that during 'modify' we need to
  // also grab mutex_, so we'll need to collect the serizlized
  // buffer and callback-list at the same time for atomicity.
  GoogleString buffer, verify;
  ReadPurgeFile(&purges_from_file);                                   // read
  ModifyPurgeSet(&purges_from_file, &buffer, &callbacks,
                 &return_purges, &failures);                          // modify
  if (!WritePurgeFile(buffer) ||                                      // write
      !Verify(buffer)) {                                              // verify
    contentions_->Add(1);
    success = false;
    HandleWriteFailure(failures, &callbacks, &return_purges, &lock_and_update);
  }

  interprocess_lock_->Unlock();

  if (!callbacks.empty()) {
    if (success) {
      // Induce a file-read the next time PollFileSystem() is called.
      // Note that there is a small chance we might read the same
      // version of the file twice if we get a PollFileSystem()
      // request with 5 seconds since the last check, after writing
      // the file and before this line.  However that redundant read
      // is not harmful.
      purge_index_->Add(1);
    }
    for (int i = 0, n = callbacks.size(); i < n; ++i) {
      callbacks[i]->Run(success, "");
    }
  } else if (lock_and_update) {
    GrabLockAndUpdate();
  }
}

void PurgeContext::HandleWriteFailure(int failures,
                                      PurgeCallbackVector* callbacks,
                                      PurgeSet* return_purges,
                                      bool* lock_and_update) {
  ScopedMutex lock(mutex_.get());

  num_consecutive_failures_ += failures + 1;
  if (num_consecutive_failures_ <= kMaxContentionRetries) {
    // Since we relinquished the lock prior to verifying, another
    // thread may have added more purge requests prior to re-acquiring
    // the lock.  In either case we need to restore the callbacks so
    // we can try again and notify interested parties.
    if (waiting_for_interprocess_lock_) {
      DCHECK(!pending_callbacks_.empty());
      pending_callbacks_.insert(pending_callbacks_.end(), callbacks->begin(),
                                callbacks->end());
      pending_purges_.Merge(*return_purges);
      callbacks->clear();
    } else {
      DCHECK(pending_callbacks_.empty());
      DCHECK(!callbacks->empty());
      waiting_for_interprocess_lock_ = true;
      callbacks->swap(pending_callbacks_);
      pending_purges_.Swap(return_purges);
      *lock_and_update = true;
    }
  } else {
    // Either the file-write failed or the read-verify mismatched.
    //
    // This might be due to a permissions problem, in which case
    // we might never succeed, or it might be a transient issue
    // due to a broken lock contention.  In either case, we will
    // give up and report failure.
    //
    // Note: the file-system itself already reported Info messages on
    // whatever the error was so we don't need another error here, but we'll
    // return a 404 or whatever to the PURGE request.
    //
    // TODO(jmarantz): capture the file-system logs and return those
    // to PURGE clients as the response body.
    file_write_failures_->Add(callbacks->size());
    num_consecutive_failures_ = 0;
  }
}

void PurgeContext::ModifyPurgeSet(PurgeSet* purges_from_file,
                                  GoogleString* buffer,
                                  PurgeCallbackVector* return_callbacks,
                                  PurgeSet* return_purges,
                                  int* failures) {
  // Note that while were are reading the file, another Purge might
  // arrive in pending_purges_, protected by mutex_.  We avoid holding
  // the that mutex when reading/writing the file as that might create
  // contention, with IsValid requests in other threads.  But we must
  // hold the mutex during merges.
  ScopedMutex lock(mutex_.get());

  // TODO(jmarantz): as we do not currently merge in purge_set_ as
  // consider the Source Of Truth to be the contents of the file plus
  // the pending purges.  However I would like to add some resilience
  // to file corruption, which might be partially addressed by putting
  // in 'purges_from_file->Merge(purge_set_);' here.  However we
  // should have testcases for how the recovery would work and be
  // transmitted back to the file system and to all servers in
  // a bounded amount of time.

  purges_from_file->Merge(pending_purges_);
  return_purges->Swap(&pending_purges_);
  pending_purges_.Clear();
  waiting_for_interprocess_lock_ = false;
  // Now if another Purge arrives it will have to wait for the next lock
  // again.

  // Collect the write-buffer from the aggregated PurgeSet while we
  // have mutex_ held.
  StrAppend(buffer, Integer64ToString(
      purges_from_file->global_invalidation_timestamp_ms()),
            "\n");
  for (PurgeSet::Iterator p = purges_from_file->Begin(),
           e = purges_from_file->End();
       p != e; ++p) {
    StrAppend(buffer, Integer64ToString(p.Value()), " ", p.Key(), "\n");
  }

  // We must also collect up and return callbacks for the requests we have
  // now merged, since once the lock is released more Purge requests may
  // come in and we don't want to call their callbacks prematurely.
  //
  // Note we can't call these callbacks until we've written the serialized
  // form to disk and verified that the data indeed hit the disk.
  return_callbacks->swap(pending_callbacks_);

  // Same with the number of failures.  They need to be removed from
  // our mutex-protected world now and re-accumulated under mutex if
  // another failure ensues.
  *failures = num_consecutive_failures_;
  num_consecutive_failures_ = 0;
}

bool PurgeContext::WritePurgeFile(const GoogleString& buffer) {
  // Atomicly write file so we don't have to acquire the lock to read it.
  file_writes_->Add(1);
  return file_system_->WriteFileAtomic(filename_, buffer, message_handler_);
}

// There is a non-zero chance that this thread will be unable to
// acquire the named-lock in the given time-limit.  We'll just bump up
// the cancellation count.
void PurgeContext::CancelCachePurgeFile() {
  PurgeCallbackVector callbacks;
  {
    ScopedMutex lock(mutex_.get());
    callbacks.swap(pending_callbacks_);

    // We are giving up on these pending invalidations rather than having
    // them take effect at some random time in the future.
    pending_purges_.Clear();

    waiting_for_interprocess_lock_ = false;
  }

  // All the purges in the queue failed :(.  Maybe the clients
  // will retry.
  cancellations_->Add(callbacks.size());
  for (int i = 0, n = callbacks.size(); i < n; ++i) {
    callbacks[i]->Run(false, "timeout");
  }
}

void PurgeContext::WaitForTimerAndGrabLock() {
  if (request_batching_delay_ms_ == 0) {
    GrabLockAndUpdate();
  } else {
    int64 alarm_time_us =
        timer_->NowUs() + request_batching_delay_ms_ * Timer::kMsUs;
    scheduler_->AddAlarmAtUs(alarm_time_us,
                             MakeFunction(this,
                                          &PurgeContext::GrabLockAndUpdate,
                                          &PurgeContext::CancelCachePurgeFile));
  }
}

void PurgeContext::GrabLockAndUpdate() {
  interprocess_lock_->LockTimedWaitStealOld(
      kTimeoutMs, kStealLockAfterMs,
      MakeFunction(this,
                   &PurgeContext::UpdateCachePurgeFile,
                   &PurgeContext::CancelCachePurgeFile));
}

void PurgeContext::SetCachePurgeGlobalTimestampMs(
    int64 timestamp_ms, PurgeCallback* callback) {
  bool grab_lock = false;
  {
    ScopedMutex lock(mutex_.get());
    pending_purges_.UpdateGlobalInvalidationTimestampMs(timestamp_ms);
    if (!waiting_for_interprocess_lock_) {
      waiting_for_interprocess_lock_ = true;
      grab_lock = true;
    }
    pending_callbacks_.push_back(callback);
  }
  if (grab_lock) {
    WaitForTimerAndGrabLock();
  }
}

void PurgeContext::AddPurgeUrl(StringPiece url, int64 timestamp_ms,
                               PurgeCallback* callback) {
  if (!enable_purge_) {
    callback->Run(false, "EnableCachePurge is off");
  } else {
    bool grab_lock = false;
    {
      ScopedMutex lock(mutex_.get());
      pending_purges_.Put(url.as_string(), timestamp_ms);
      if (!waiting_for_interprocess_lock_) {
        waiting_for_interprocess_lock_ = true;
        grab_lock = true;
      }
      pending_callbacks_.push_back(callback);
    }
    if (grab_lock) {
      WaitForTimerAndGrabLock();
    }
  }
}

void PurgeContext::PollFileSystem() {
  int64 now_ms = timer_->NowMs();
  int64 delta_ms = now_ms - purge_poll_timestamp_ms_->Get();
  int64 global_purge_index = purge_index_->Get();
  mutex_->Lock();
  bool needs_update = (local_purge_index_ < global_purge_index);
  if (!reading_ &&
      (needs_update || (delta_ms >= kCheckCacheIntervalMs))) {
    if (needs_update) {
      local_purge_index_ = global_purge_index;
    }
    reading_ = true;
    mutex_->Unlock();
    purge_poll_timestamp_ms_->Set(now_ms);
    ReadFileAndCallCallbackIfChanged(needs_update);
    mutex_->Lock();
    reading_ = false;
  }
  mutex_->Unlock();
}

void PurgeContext::ReadFileAndCallCallbackIfChanged(bool needs_update) {
  CopyOnWrite<PurgeSet> purges_from_file;
  PurgeSet* mutable_purges_from_file = purges_from_file.MakeWriteable();
  mutable_purges_from_file->set_max_size(max_bytes_in_cache_);
  bool call_callback = false;

  // Note that we don't hold the global lock while reading the file.
  // But under mutex we have set reading_ so another thread doesn't
  // try a concurrent read.
  DCHECK(reading_);
  ReadPurgeFile(mutable_purges_from_file);

  {
    ScopedMutex lock(mutex_.get());
    if (!purge_set_->Equals(*purges_from_file)) {
      if (!needs_update) {
        // This update was induced by a timeout in this process, rather
        // than by a signal from another process or an UpdateCachePurgeFile
        // in this process.  This signals to other child processes that their
        // local_purge_index_ state is now stale and they must re-read.  We
        // only do this if the purge-set from disk actually changed.
        purge_index_->Add(1);
      }
      purge_set_ = purges_from_file;
      if (update_callback_ != NULL) {
        // We don't want to call the update callback while holding the
        // lock.  Also note that even though we will release the lock
        // before calling the callback, purge_set_ will not be mutated
        // until our caller sets reading_ to false.
        call_callback = true;
      }
    }
  }
  if (call_callback) {
    update_callback_->Run(purges_from_file);
  }

  // Note that we don't merge in pending_purges_ in this flow.  Only
  // when we grab the lock and write the file do we merge in the
  // pending_purges_.
}

void PurgeContext::SetUpdateCallback(PurgeSetCallback* cb) {
  update_callback_.reset(cb);
}

}  // namespace net_instaweb
