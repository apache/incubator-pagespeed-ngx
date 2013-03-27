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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_CACHE_STATS_H_
#define NET_INSTAWEB_UTIL_PUBLIC_CACHE_STATS_H_

#include "net/instaweb/util/public/atomic_bool.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class Histogram;
class SharedString;
class Statistics;
class Timer;
class Variable;

// Wrapper around a CacheInterface that adds statistics and histograms
// for hit-rate, latency, etc.  As there can be multiple caches in a
// system (l1, l2, etc), the constructor takes a string prefix so they
// can be measured independently.
class CacheStats : public CacheInterface {
 public:
  // Takes ownership of the cache.
  CacheStats(StringPiece prefix,
             CacheInterface* cache,
             Timer* timer,
             Statistics* statistics);
  virtual ~CacheStats();

  // This must be called once for every unique cache prefix.
  static void InitStats(StringPiece prefix, Statistics* statistics);

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void MultiGet(MultiGetRequest* request);
  virtual void Put(const GoogleString& key, SharedString* value);
  virtual void Delete(const GoogleString& key);
  virtual const char* Name() const { return name_.c_str(); }
  virtual CacheInterface* Backend() { return cache_.get(); }
  virtual bool IsBlocking() const { return cache_->IsBlocking(); }

  virtual bool IsHealthy() const {
    return !shutdown_.value() && cache_->IsHealthy();
  }

  virtual void ShutDown() {
    shutdown_.set_value(true);
    cache_->ShutDown();
  }

 private:
  class StatsCallback;
  friend class StatsCallback;

  scoped_ptr<CacheInterface> cache_;
  Timer* timer_;
  Histogram* get_count_histogram_;
  Histogram* hit_latency_us_histogram_;
  Histogram* insert_latency_us_histogram_;
  Histogram* insert_size_bytes_histogram_;
  Histogram* lookup_size_bytes_histogram_;
  Variable* deletes_;
  Variable* hits_;
  Variable* inserts_;
  Variable* misses_;
  GoogleString name_;
  AtomicBool shutdown_;

  DISALLOW_COPY_AND_ASSIGN(CacheStats);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CACHE_STATS_H_
