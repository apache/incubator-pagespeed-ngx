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

#ifndef PAGESPEED_KERNEL_CACHE_PURGE_CONTEXT_H_
#define PAGESPEED_KERNEL_CACHE_PURGE_CONTEXT_H_

#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/purge_set.h"
#include "pagespeed/kernel/util/copy_on_write.h"

namespace net_instaweb {

class AbstractMutex;
class FileSystem;
class MessageHandler;
class NamedLock;
class NamedLockManager;
class Scheduler;
class Statistics;
class ThreadSystem;
class UpDownCounter;
class Variable;

// Handles purging of URLs, atomically persisting them to disk, allowing
// multiple concurrent threads/processes to handle purge requests and
// propagate them to the other processes.
//
// All public methods in this class are thread-safe.
//
// This class depends on Statistics being functional.  If statistics are off,
// then cache purging may be slower, but it will still work.
class PurgeContext {
 public:
  typedef Callback2<bool, StringPiece> PurgeCallback;
  typedef Callback1<const CopyOnWrite<PurgeSet>&> PurgeSetCallback;

  // The source-of-truth of the purge data is kept in files.  These files
  // are checked for changes via stat every 5 seconds.
  //
  // TODO(jmarantz): make this settable.
  static const int kCheckCacheIntervalMs = 5 * Timer::kSecondMs;

  // Variable names.
  static const char kCancellations[];
  static const char kContentions[];
  static const char kFileParseFailures[];
  static const char kFileStats[];
  static const char kFileWriteFailures[];
  static const char kFileWrites[];
  static const char kPurgeIndex[];
  static const char kPurgePollTimestampMs[];
  static const char kStatCalls[];

  PurgeContext(StringPiece filename,
               FileSystem* file_system,
               Timer* timer,
               int max_bytes_in_cache,
               ThreadSystem* thread_system,
               NamedLockManager* lock_manager,
               Scheduler* scheduler,
               Statistics* statistics,
               MessageHandler* handler);
  ~PurgeContext();

  static void InitStats(Statistics* statistics);

  // By default, PurgeContext will try to acquire the lock and write the
  // cache.purge file as soon as it is called.  This may present a significant
  // load to the file system, causing delays.
  //
  // In a multi-threaded or asynchronous environment (e.g. any environment
  // other than Apache HTTPD pre-fork MPM), it is desirable to batch up
  // requests for a time (e.g. 1 second) before writing the updated cache file.
  void set_request_batching_delay_ms(int64 delay_ms) {
    request_batching_delay_ms_ = delay_ms;
  }

  // Adds a URL to the purge-set, atomically updating the purge
  // file, and transmitting the results to the system, attempting
  // to do so within a bounded amount of time.
  //
  // Calls callback(true, "") if the invalidation succeeded (was able to grab
  // the global lock in a finite amount of time), and callback(false, "reason")
  // if it failed.  If we fail to take the lock then the invalidation is dropped
  // on the floor, and statistics "purge_cancellations" is bumped.
  void AddPurgeUrl(StringPiece url, int64 timestamp_ms,
                   PurgeCallback* callback);

  // Sets the global invalidation timestamp.
  //
  // Calls callback(true, "") if the invalidation succeeded (was able to grab
  // the global lock in a finite amount of time), and callback(false, "reason")
  // if it failed. If we fail to take the lock then the invalidation is dropped
  // on the floor, and statistics "purge_cancellations" is bumped.
  void SetCachePurgeGlobalTimestampMs(int64 timestamp_ms,
                                      PurgeCallback* callback);

  // Periodically updates the purge-set from disk if enough time has
  // expired.  This can be called on every request.
  void PollFileSystem();

  // To test whether URLs are purged, you must capture a PurgeSet by registering
  // a callback.  The PurgeSet is passed to the callback using a CopyOnWrite
  // wrapper so that the callback can efficiently share the storage.
  //
  // IsValid calls can then be made using the PurgeSet captured by the callback.
  void SetUpdateCallback(PurgeSetCallback* cb);

  // Indicates whether individual URL purging is supported.  If false,
  // then we only take the cache.flush file timestamp to do full cache
  // flushes.  If true, then we read and parse the contents of the file
  // to find the global invalidation time and cache-flush times for
  // the individual entries.
  void set_enable_purge(bool x) { enable_purge_ = x; }

 private:
  friend class PurgeContextTest;

  typedef std::vector<PurgeCallback*> PurgeCallbackVector;

  // Having acquired the lock, merges all sources of purge information and
  // write the purge file.  This must be called with interprocess_lock_
  // held.
  void UpdateCachePurgeFile();

  // Reads the contents of filename_ into *purges_from_file.  Any invalid
  // data in the file is ignored.
  //
  // This method doesn't read or write dynamic data from the class other
  // than statistics, so it's thread-safe.  Also it does not need to be
  // called with interprocess_lock_ held as the writes are made atomic
  // via write-to-temp + rename.
  void ReadPurgeFile(PurgeSet* purges_from_file);
  void ReadFileAndCallCallbackIfChanged(bool needs_update);

  // Combines the purges_from_file with pending_purges_ and purge_set_,
  // serializes the result into *buffer for writing back to the file.
  //
  // Note: this does *not* update purge_set_ (it treats it as read-only).
  // However, it bumps purge_index_ to induce a file-read on
  // the next call to PollFileSystem().
  //
  // This helper method is used as part of a read/modify/write/verify
  // sequence, and it returns the callbacks that must be called when
  // that sequence is done.  Similarly, this method transfers
  // num_consecutive_failures_ into *failures, zeroing the former.
  //
  // return_purges gets populated with pending_purges_ via swap.  It
  // can be used to replace pending_purges_ in the event of a write
  // failure so we don't lose that data.
  //
  // return_callbacks contains the callbacks to call when the
  // transaction is complete.
  //
  // TODO(jmarantz): the return values from this method comprise a
  // transaction which should be made an explicit class or struct.
  //
  // This method is thread-safe; it grabs mutex_.
  void ModifyPurgeSet(PurgeSet* purges_from_file, GoogleString* buffer,
                      PurgeCallbackVector* return_callbacks,
                      PurgeSet* return_purges,
                      int* failures);

  // When a write fails, we must do one of these:
  //  a) restore the pending purges & callbacks and try to re-take the lock.
  //  b) call the callbacks with 'false' and drop return_purges.
  void HandleWriteFailure(int failures,
                          PurgeCallbackVector* callbacks,
                          PurgeSet* return_purges,
                          bool* lock_and_update);

  // Writes the serialized purge data into filename_.  This method must
  // be called with the interprocess_lock_ held, but does not reference
  // or update dynamic data in this.
  bool WritePurgeFile(const GoogleString& buffer);

  // Returns true if the contents of filename_ matches the specified buffer.
  bool Verify(const GoogleString& expected_purge_file_contents);

  // Returns the name used to create a new lock.  Visible for testing
  // to aid in testing lock contention.
  GoogleString LockName() const { return StrCat(filename_, "-lock"); }

  // Initiates a scheduler-alarm to call GrabLockAndUpdate after a
  // small delay.  The delay is used to batch bursts of cache-purge
  // file updates, thereby rate-limiting disk-writes.
  void WaitForTimerAndGrabLock();

  // Attempts to grab a lock for the cache-purge file, calling
  // UpdateCachePurgeFile if successful, and CancelCachePurgeFile
  // if we failed to grab the lock.
  void GrabLockAndUpdate();

  // There is a non-zero chance that this thread will be unable to acquire the
  // named-lock in the given time-limit.  When this occurs, we'll fail the
  // requests, calling the callbacks with 'false'.
  void CancelCachePurgeFile();

  // Ensures a timestamp has reasonable syntax and is not (too far) in the
  // future.
  //
  // The parsed timestamp is placed in *timestamp_ms, and true/false is
  // used to indicate parsing & validation success.
  bool ParseAndValidateTimestamp(StringPiece time_string, int64 now_ms,
                                 int64* timestamp_ms);

  GoogleString filename_;
  scoped_ptr<NamedLock> interprocess_lock_;
  FileSystem* file_system_;
  Timer* timer_;

  Statistics* statistics_;
  scoped_ptr<AbstractMutex> mutex_;
  CopyOnWrite<PurgeSet> purge_set_;        // protected by mutex_
  PurgeSet pending_purges_;                // protected by mutex_
  PurgeCallbackVector pending_callbacks_;  // protected by mutex_
  int64 local_purge_index_;                // protected by mutex_
  int num_consecutive_failures_;           // protected_by mutex_
  bool waiting_for_interprocess_lock_;     // protected_by mutex_
  bool reading_;                           // protected_by mutex_

  bool enable_purge_;           // When false, can only flush entire cache.
  int max_bytes_in_cache_;

  int64 request_batching_delay_ms_;

  Variable* cancellations_;
  Variable* contentions_;
  Variable* file_parse_failures_;
  Variable* file_stats_;
  Variable* file_write_failures_;
  Variable* file_writes_;
  Variable* purge_index_;
  scoped_ptr<UpDownCounter> purge_poll_timestamp_ms_;

  Scheduler* scheduler_;
  MessageHandler* message_handler_;

  scoped_ptr<PurgeSetCallback> update_callback_;

  DISALLOW_COPY_AND_ASSIGN(PurgeContext);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_CACHE_PURGE_CONTEXT_H_
