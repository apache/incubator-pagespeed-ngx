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

#ifndef PAGESPEED_KERNEL_CACHE_CACHE_BATCHER_H_
#define PAGESPEED_KERNEL_CACHE_CACHE_BATCHER_H_

#include <cstddef>

#include <unordered_map>
#include <vector>

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/cache/cache_interface.h"

namespace net_instaweb {

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

  // We batch up cache lookups until outstanding ones are complete.  However, we
  // bound the number of pending lookups in order to avoid exhausting memory.
  // When the "queues" are saturated, we drop the requests, calling the callback
  // immediately with kNotFound.
  static const size_t kDefaultMaxPendingGets = 1000;

  struct Options {
    Options()
        : max_parallel_lookups(kDefaultMaxParallelLookups),
          max_pending_gets(kDefaultMaxPendingGets) {
    }

    int max_parallel_lookups;
    int max_pending_gets;
    // Copy-construction and assign are allowed.
  };

  // Does not take ownership of the cache. Takes ownership of the mutex.
  CacheBatcher(const Options& options, CacheInterface* cache,
               AbstractMutex* mutex, Statistics* statistics);
  virtual ~CacheBatcher();

  // Startup-time (pre-construction) initialization of statistics
  // variables so the correct-sized shared memory can be constructed
  // in the root Apache process.
  static void InitStats(Statistics* statistics);

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, const SharedString& value);
  virtual void Delete(const GoogleString& key);
  virtual GoogleString Name() const;
  static GoogleString FormatName(StringPiece cache, int parallelism, int max);

  // Note: CacheBatcher cannot do any batching if given a blocking cache,
  // however it is still functional so pass on the bit.
  virtual bool IsBlocking() const { return cache_->IsBlocking(); }

  virtual bool IsHealthy() const { return cache_->IsHealthy(); }
  virtual void ShutDown();

 private:
  typedef std::unordered_map<GoogleString, std::vector<Callback*>> CallbackMap;

  class Group;
  class MultiCallback;

  bool CanIssueGet() const EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool CanQueueCallback() const EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void GroupComplete();

  MultiGetRequest* ConvertMapToRequest(const CallbackMap& map)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  MultiGetRequest* CreateRequestForQueuedKeys()
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void MoveQueuedKeys() EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void ExtractInFlightKeys(const GoogleString& key,
                   std::vector<CacheInterface::Callback*>* callbacks)
      LOCKS_EXCLUDED(mutex_);

  void DecrementInFlightGets(int n) LOCKS_EXCLUDED(mutex_);

  // For testing use only (instrumentation, synchronization).
  friend class CacheBatcherTestingPeer;
  int last_batch_size() const LOCKS_EXCLUDED(mutex_);
  int num_in_flight_keys() LOCKS_EXCLUDED(mutex_);

  CacheInterface* cache_;
  Variable* dropped_gets_;
  Variable* coalesced_gets_;
  Variable* queued_gets_;
  CallbackMap in_flight_ GUARDED_BY(mutex_);
  int last_batch_size_ GUARDED_BY(mutex_);
  scoped_ptr<AbstractMutex> mutex_;
  int num_in_flight_groups_ GUARDED_BY(mutex_);
  int num_in_flight_keys_ GUARDED_BY(mutex_);
  int num_pending_gets_ GUARDED_BY(mutex_);
  const Options options_;
  CallbackMap queued_ GUARDED_BY(mutex_);
  bool shutdown_ GUARDED_BY(mutex_);

  DISALLOW_COPY_AND_ASSIGN(CacheBatcher);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_CACHE_CACHE_BATCHER_H_
