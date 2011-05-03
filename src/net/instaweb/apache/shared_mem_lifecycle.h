// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: morlovich@google.com (Maksim Orlovich)
//
// Contains SharedMemLifecycle<T> template class which
// helps make sure we initialize and cleanup things like shared memory
// locks, etc. the right number of times when various factories
// share them and multiple processes are involved.

#ifndef NET_INSTAWEB_APACHE_SHARED_MEM_LIFECYCLE_H_
#define NET_INSTAWEB_APACHE_SHARED_MEM_LIFECYCLE_H_

#include <map>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class ApacheRewriteDriverFactory;
class MessageHandler;

typedef std::map<GoogleString, ApacheRewriteDriverFactory*> SharedMemOwnerMap;

// Helper class for managing initialization & attachment of subsystems
// (in particular lock manager and cache) that use shared memory.
//
// It ensures we create the underlying shared memory segment exactly once for
// each cache path (further instances will attach to the existing segment) and
// that every segment is cleaned up exactly once, even if multiple vhosts
// share it.
//
// The reason cache paths matter is that they are effectively the sharing
// domain for cache data when using the traditional file-system based setup,
// and we want to scope the locks the same (as they are used for creation
// or fetch of data resources which will end up in the cache).
template<typename T> class SharedMemLifecycle {
 public:
  typedef T* (ApacheRewriteDriverFactory::*FactoryMethod)();
  SharedMemLifecycle(ApacheRewriteDriverFactory* owner, FactoryMethod creator,
                     const char* name, SharedMemOwnerMap** owner_map);
  void RootInit();
  void ChildInit();
  void GlobalCleanup(MessageHandler* handler);  // only called in root process

  // Hands over ownership of any instance of T that got created thus far,
  // clearing our pointer. Not that this object will still be responsible for
  // calling GlobalCleanup. If initialization failed, will return NULL.
  T* Release() { return value_.release(); }
  T* Get() { return value_.get(); }

 private:
  SharedMemOwnerMap* AccessOwnerMap();
  void DestroyOwnerMap();

  ApacheRewriteDriverFactory* owner_;
  FactoryMethod creator_;

  // Pointer to where we store the pointer to the map describing which instances
  // of ApacheRewriteDriverFactory are responsible for our module in which path.
  SharedMemOwnerMap** owner_map_;

  // We hold on to the actual lock manager/cache/etc., temporarily before
  // handing it over to RewriteDriver's ownership via Release()
  scoped_ptr<T> value_;

  // Description of what T does to use for log messages.
  const char* const name_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemLifecycle<T>);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_SHARED_MEM_LIFECYCLE_H_
