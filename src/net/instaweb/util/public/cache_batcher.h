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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_CACHE_BATCHER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_CACHE_BATCHER_H_

#include <cstddef>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class AbstractMutex;
class SharedString;
class Statistics;
class Variable;

// Batches up cache lookups to exploit implementations that have MultiGet
// support.  A fixed limit of outstanding cache lookups are passed through
// as single-key Gets when received to avoid adding latency.  Above that,
// the keys & callbacks are queued until one of the outstanding Gets
// completes.  When that occurs, the queued requests are passed as
// a single MultiGet request.
//
// There is also a maximum queue size.  If Gets stream in faster than they
// are completed and the queue overflows, then we respond with a fast kNotFound.
//
// Note that this class is designed for use with an asynchronous cache
// implementation.  To use this with a blocking cache implementation, please
// wrap the blocking cache in an AsyncCache.
class CacheBatcher : public CacheInterface {
 public:
  // We are willing to only do a bounded number of parallel lookups.
  // Note that this is independent of the number of keys in each lookup.
  //
  // By setting the default at 1, we get maximum batching and minimize
  // the number of parallel lookups we do.  Note that independent of
  // this count, there is already substantial lookup parallelism
  // because each Apache process has its own batcher, and there can be
  // multiple Apache servers talking to the same cache.
  //
  // Further, the load-tests performed while developing this feature
  // indicated that the best value was '1'.
  static const int kDefaultMaxParallelLookups = 1;

  // We batch up cache lookups until outstanding ones are complete.
  // However, we bound the queue size in order to avoid exhausting
  // memory.  When the thread queues are saturated, we drop the
  // requests, calling the callback immediately with kNotFound.
  static const size_t kDefaultMaxQueueSize = 1000;

  // Takes ownership of the cache and mutex.
  CacheBatcher(CacheInterface* cache, AbstractMutex* mutex,
               Statistics* statistics);
  virtual ~CacheBatcher();

  // Startup-time (pre-construction) initialization of statistics
  // variables so the correct-sized shared memory can be constructed
  // in the root Apache process.
  static void Initialize(Statistics* statistics);

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, SharedString* value);
  virtual void Delete(const GoogleString& key);
  virtual const char* Name() const { return name_.c_str(); }

  int last_batch_size() const { return last_batch_size_; }  // for testing
  void set_max_queue_size(size_t n) { max_queue_size_ = n; }
  void set_max_parallel_lookups(size_t n) { max_parallel_lookups_ = n; }

  int Pending();  // This is used to help synchronize tests.

 private:
  class Group;
  class BatcherCallback;

  void GroupComplete();
  bool CanIssueGet() const;  // must be called with mutex_ held.

  scoped_ptr<CacheInterface> cache_;
  scoped_ptr<AbstractMutex> mutex_;
  GoogleString name_;
  MultiGetRequest queue_;
  int last_batch_size_;
  int pending_;
  int max_parallel_lookups_;
  size_t max_queue_size_;  // size_t so it can be compared to queue_.size().
  Variable* dropped_gets_;

  DISALLOW_COPY_AND_ASSIGN(CacheBatcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CACHE_BATCHER_H_
