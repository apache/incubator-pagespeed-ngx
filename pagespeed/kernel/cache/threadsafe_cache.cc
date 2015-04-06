/*
 * Copyright 2010 Google Inc.
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

#include "pagespeed/kernel/cache/threadsafe_cache.h"

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/cache/delegating_cache_callback.h"

namespace net_instaweb {

ThreadsafeCache::~ThreadsafeCache() {
}

GoogleString ThreadsafeCache::FormatName(StringPiece name) {
  return StrCat("ThreadsafeCache(", name, ")");
}

namespace {

class ThreadsafeCallback : public DelegatingCacheCallback {
 public:
  ThreadsafeCallback(AbstractMutex* mutex, CacheInterface::Callback* callback)
      EXCLUSIVE_LOCK_FUNCTION(mutex)
      : DelegatingCacheCallback(callback), mutex_(mutex) {
    mutex_->Lock();
  }

  virtual ~ThreadsafeCallback() {
  }

  virtual void Done(CacheInterface::KeyState state) UNLOCK_FUNCTION(mutex_) {
    mutex_->Unlock();
    DelegatingCacheCallback::Done(state);
  }

  AbstractMutex* mutex_;
};

}  // namespace

void ThreadsafeCache::Get(const GoogleString& key, Callback* callback) {
  ThreadsafeCallback* cb = new ThreadsafeCallback(mutex_.get(), callback);
  cache_->Get(key, cb);
}

void ThreadsafeCache::Put(const GoogleString& key, SharedString* value) {
  ScopedMutex mutex(mutex_.get());
  cache_->Put(key, value);
}

void ThreadsafeCache::Delete(const GoogleString& key) {
  ScopedMutex mutex(mutex_.get());
  cache_->Delete(key);
}

bool ThreadsafeCache::IsHealthy() const {
  ScopedMutex mutex(mutex_.get());
  return cache_->IsHealthy();
}

void ThreadsafeCache::ShutDown() {
  ScopedMutex mutex(mutex_.get());
  return cache_->ShutDown();
}

}  // namespace net_instaweb
