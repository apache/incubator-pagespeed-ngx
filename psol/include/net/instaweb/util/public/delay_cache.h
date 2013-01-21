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
//
// Contains DelayCache, which wraps a cache, but lets a test delay
// responses for specific cache-keys.  The callbacks are awakened
// by an explicit ReleaseKey API.
//
// By default, all cache lookups are transmitted immediately to
// the callback.
//
// Note: MockTimeCache also supports delayed callbacks, but they are all delayed
// by a fixed time.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_DELAY_CACHE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_DELAY_CACHE_H_

#include <map>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class SharedString;
class ThreadSystem;

// See file comment
class DelayCache : public CacheInterface {
 public:
  // Note: takes ownership of nothing.
  DelayCache(CacheInterface* cache, ThreadSystem* thread_system);
  virtual ~DelayCache();

  // Reimplementations of CacheInterface methods.
  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, SharedString* value);
  virtual void Delete(const GoogleString& key);
  virtual void MultiGet(MultiGetRequest* request);

  // Instructs the cache to delay delivery of callbacks for specific cache-key.
  // It is a fatal error -- reported at class destruction, to request delay of
  // a key that is never looked up and released.
  void DelayKey(const GoogleString& key);

  // Release the delay on the callback delivered for a specific key.  It is
  // ane error to attempt to release a key that was never delayed.
  void ReleaseKey(const GoogleString& key) { ReleaseKeyInSequence(key, NULL); }

  // See ReleaseKey.  If sequence is non-NULL, the callback is
  // delivered on the sequence, otherwise it is delivered directly
  // from ReleaseKey.
  void ReleaseKeyInSequence(const GoogleString& key,
                            QueuedWorkerPool::Sequence* sequence);

  virtual const char* Name() const { return name_.c_str(); }
  virtual bool IsBlocking() const { return false; }
  virtual bool IsHealthy() const { return cache_->IsHealthy(); }
  virtual void ShutDown() { cache_->ShutDown(); }

 private:
  class DelayCallback;
  friend class DelayCallback;

  void LookupComplete(DelayCallback* callback);

  typedef std::map<GoogleString, DelayCallback*> DelayMap;

  CacheInterface* cache_;
  scoped_ptr<AbstractMutex> mutex_;
  StringSet delay_requests_;
  DelayMap delay_map_;
  GoogleString name_;

  DISALLOW_COPY_AND_ASSIGN(DelayCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_DELAY_CACHE_H_
