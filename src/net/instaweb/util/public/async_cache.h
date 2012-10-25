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
#include "net/instaweb/util/public/atomic_bool.h"
#include "net/instaweb/util/public/atomic_int32.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class SharedString;

// Employs a QueuedWorkerPool to turn a synchronous cache implementation
// into an asynchronous one.  This makes sense to do only if the cache
// implemention is potentially slow, due to network latency or disk seek time.
//
// This class also serves to serialize access to the passed-in cache, ensuring
// that it is accessed from only one thread at a time.
class AsyncCache : public CacheInterface {
 public:
  // The maximum number of operations that can be queued up while a
  // server is slow.  When this is reached, old Deletes/Puts get
  // dropped, and old Gets are retired with a kNotFound.
  //
  // This helps bound the amount of memory consumed by queued operations
  // when the cache gets wedged.  Note that when CacheBatcher is layered
  // above AsyncCache, it will queue up its Gets at a level above this one,
  // and ultimately send those using a MultiGet.
  //
  // TODO(jmarantz): Analyze whether we drop operations under load with
  // a non-wedged cache.  If it looks like we are dropping Puts the first
  // time we encounter a page then I think we may need to bump this up.
  static const int64 kMaxQueueSize = 20;

  // Takes ownership of the synchronous cache that is passed in.
  // Does not take ownership of the pool, which might be shared with
  // other users.
  //
  // Note that in the future we may try to add multi-threaded access
  // to the underlying cache (e.g. AprMemCache supports this), so we
  // take the pool as the constructor arg.
  AsyncCache(CacheInterface* cache, QueuedWorkerPool* pool);
  virtual ~AsyncCache();

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, SharedString* value);
  virtual void Delete(const GoogleString& key);
  virtual void MultiGet(MultiGetRequest* request);
  virtual const char* Name() const { return name_.c_str(); }
  virtual bool IsBlocking() const { return false; }
  virtual bool IsMachineLocal() const { return cache_->IsMachineLocal(); }

  // Prevent the AsyncCache from issuing any more Gets.  Any subsequent
  // Gets will have their callback invoked immediately with kNotFound.
  // Outstanding Gets may be completed depending on timing.
  //
  // This can be called during the process Shutdown flow to avoid
  // introducing more work asynchronously that will have to be
  // completed prior to Shutdown.
  void StopCacheActivity();

  // Cancels all pending cache operations.  Puts and Deletes are dropped.
  // Gets and MultiGets are retired by calling their callbacks with
  // kNotFound.
  void CancelPendingOperations() { sequence_->CancelPendingFunctions(); }

  virtual bool IsHealthy() const {
    return !stopped_.value() && cache_->IsHealthy();
  }
  int32 outstanding_operations() { return outstanding_operations_.value(); }

 private:
  // Function to execute a single-key Get in sequence_.  Canceling
  // a Get calls the callback with kNotFound.
  void DoGet(GoogleString* key, Callback* callback);
  void CancelGet(GoogleString* key, Callback* callback);

  // Function to execute a multi-key Get in sequence_.  Canceling
  // a MultiGet calls all the callbacks with kNotFound.
  void DoMultiGet(MultiGetRequest* request);
  void CancelMultiGet(MultiGetRequest* request);

  // Functions to execute Put/Delete in sequence_.  Canceling
  // a Put/Delete just drops the request.
  void DoPut(GoogleString* key, SharedString* value);
  void CancelPut(GoogleString* key, SharedString* value);
  void DoDelete(GoogleString* key);
  void CancelDelete(GoogleString* key);

  void MultiGetReportNotFound(MultiGetRequest* request);

  scoped_ptr<CacheInterface> cache_;
  GoogleString name_;
  QueuedWorkerPool::Sequence* sequence_;
  AtomicBool stopped_;
  AtomicInt32 outstanding_operations_;

  DISALLOW_COPY_AND_ASSIGN(AsyncCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_ASYNC_CACHE_H_
