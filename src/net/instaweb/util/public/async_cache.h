/*
 * Copyright 2012 Google Inc.
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_ASYNC_CACHE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_ASYNC_CACHE_H_

#include "net/instaweb/util/public/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class SharedString;

// Employs a QueuedWorkerPool to turn a synchronous cache implementation
// into an asynchronous one.  This makes sense to do only if the cache
// implemention is potentially slow, due to network latency or disk seek time.
//
// In those situations, this class enables multiple cache lookups to be
// threaded against one another, and potentially batched.
//
// Currently, Put and Delete are not made asynchronous by this class; just Get.
class AsyncCache : public CacheInterface {
 public:
  // The default maximum number of QueuedWorkerPool sequences to make.
  // Fetches that exceed this limit are dropped immediately reporting
  // kNotFound.  Note that QueuedWorkerPool itself may force queueing of
  // lookups depending on how many worker threads are available.
  //
  // Most importantly, note that when this is used with CacheBatcher, the
  // number of concurrent lookups will also be limited by
  // CacheBatcher::set_max_parallel_lookups.
  static const int kDefaultNumThreads = 10;

  // Takes ownership of the synchronous cache and mutex that are passed
  // in.  Does not take ownership of the pool, which might be shared
  // with other users.
  AsyncCache(CacheInterface* cache, AbstractMutex* mutex,
             QueuedWorkerPool* pool)
      : cache_(cache),
        mutex_(mutex),
        name_(StrCat("AsyncCache using ", cache_->Name())),
        num_threads_(kDefaultNumThreads),
        active_threads_(0),
        pool_(pool) {
  }
  virtual ~AsyncCache();

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, SharedString* value);
  virtual void Delete(const GoogleString& key);
  virtual void MultiGet(MultiGetRequest* request);
  virtual const char* Name() const { return name_.c_str(); }

  // Sets the maximum number of parallel lookups that AsyncCache will
  // attempt to make.
  void set_num_threads(int num_threads) { num_threads_ = num_threads; }

  // Returns whether the AsyncCache would accept new Get requests or
  // reject them immediately.  This entry-point is intended for tests
  // because in a live server, another thread might interleave a Get
  // request after this returns and before your Get is issued.
  bool CanIssueGet();

  // Prevent the AsyncCache from issuing any more Gets.  Any subsequent
  // Gets will have their callback invoked immediately with kNotFound.
  // Outstanding Gets may be completed depending on timing.
  //
  // This can be called during the process Shutdown flow to avoid
  // introducing more work asynchronously that will have to be
  // completed prior to Shutdown.
  void StopCacheGets();

 private:
  // Attempts to reserve a thread-count to initiate a lookup, returning true
  // if success, false, if the lookup needs to be dropped.
  bool InitiateLookup();

  // Function to execute a single-key Get in its own thread.
  void DoGet(GoogleString key, Callback* callback,
             QueuedWorkerPool::Sequence* sequence);
  void CancelGet(GoogleString key, Callback* callback,
                 QueuedWorkerPool::Sequence* sequence);

  // Function to execute a multi-key Get in its own thread.
  void DoMultiGet(MultiGetRequest* request,
                  QueuedWorkerPool::Sequence* sequence);
  void CancelMultiGet(MultiGetRequest* request,
                      QueuedWorkerPool::Sequence* sequence);
  bool CanIssueGetMutexHeld() const;  // Must be called with mutex_ held.

  void MultiGetReportNotFound(MultiGetRequest* request);

  scoped_ptr<CacheInterface> cache_;
  scoped_ptr<AbstractMutex> mutex_;
  GoogleString name_;
  int num_threads_;
  int active_threads_;
  QueuedWorkerPool* pool_;

  DISALLOW_COPY_AND_ASSIGN(AsyncCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_ASYNC_CACHE_H_
