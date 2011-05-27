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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_THREADSAFE_CACHE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_THREADSAFE_CACHE_H_

#include "net/instaweb/util/public/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class AbstractMutex;
class SharedString;

// Composes a cache with a Mutex to form a threadsafe cache.  Note
// that cache callbacks will be run in a thread that is dependent
// on the cache implementation.  This wrapper class just guarantees
// the thread safety of the cache itself, not the callbacks.
class ThreadsafeCache : public CacheInterface {
 public:
  // Takes ownership of the cache and mutex that's passed in.
  ThreadsafeCache(CacheInterface* cache, AbstractMutex* mutex)
      : cache_(cache),
        mutex_(mutex) {
  }
  virtual ~ThreadsafeCache();

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, SharedString* value);
  virtual void Delete(const GoogleString& key);
  virtual void Query(const GoogleString& key, Callback* callback);

 private:
  scoped_ptr<CacheInterface> cache_;
  scoped_ptr<AbstractMutex> mutex_;

  DISALLOW_COPY_AND_ASSIGN(ThreadsafeCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_THREADSAFE_CACHE_H_
