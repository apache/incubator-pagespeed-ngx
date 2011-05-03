// Copyright 2010 Google Inc.
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
// Author: morlovich@google.com (Maksim Orlovich)
//
// Contains SharedMemLifecycle<T> template class which
// helps make sure we initialize and cleanup things like shared memory
// locks, etc. the right number of times when various factories
// share them and multiple processes are involved.


#include "net/instaweb/apache/shared_mem_lifecycle.h"

#include "net/instaweb/apache/apache_rewrite_driver_factory.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/shared_mem_lock_manager.h"

namespace net_instaweb {

template<typename T>
SharedMemLifecycle<T>::SharedMemLifecycle(
    ApacheRewriteDriverFactory* owner, FactoryMethod creator, const char* name,
    SharedMemOwnerMap** owner_map)
    : owner_(owner),
      creator_(creator),
      owner_map_(owner_map),
      name_(name) {
}

template<typename T>
void SharedMemLifecycle<T>::RootInit() {
  GoogleString cache_path = owner_->file_cache_path().as_string();
  value_.reset((owner_->*creator_)());

  SharedMemOwnerMap::iterator prev_creator = AccessOwnerMap()->find(cache_path);
  bool ok;
  if (prev_creator == AccessOwnerMap()->end()) {
    owner_->message_handler()->Message(
        kInfo, "Initializing shared memory %s for path: %s.", name_,
        cache_path.c_str());
    ok = value_->Initialize();
    if (ok) {
      (*AccessOwnerMap())[cache_path] = owner_;
    }
  } else {
    ok = value_->Attach();
    owner_->message_handler()->Message(
        kInfo, "Reusing shared memory %s for prefix: %s.", name_,
        cache_path.c_str());
  }
  if (!ok) {
    owner_->message_handler()->Message(
        kWarning, "Unable to initialize shared memory %s. "
                  "Falling back to file system.", name_);
    value_.reset();
  }
}

template<typename T>
void SharedMemLifecycle<T>::ChildInit() {
  value_.reset((owner_->*creator_)());
  if (!value_->Attach()) {
    value_.reset();
  }
}

template<typename T>
void SharedMemLifecycle<T>::GlobalCleanup(MessageHandler* handler) {
  SharedMemOwnerMap* owners = *owner_map_;
  if (owners != NULL) {
    GoogleString cache_path = owner_->file_cache_path().as_string();
    SharedMemOwnerMap::iterator i = owners->find(cache_path);
    if (i != owners->end() && i->second == owner_) {
      T::GlobalCleanup(owner_->shared_mem_runtime(), cache_path, handler);
      owners->erase(i);
      if (owners->empty()) {
        DestroyOwnerMap();
      }
    }
  }
}

template<typename T>
SharedMemOwnerMap* SharedMemLifecycle<T>::AccessOwnerMap() {
  if (*owner_map_ == NULL) {
    *owner_map_ = new SharedMemOwnerMap;
  }
  return *owner_map_;
}

template<typename T>
void SharedMemLifecycle<T>::DestroyOwnerMap() {
  if (*owner_map_ != NULL) {
    delete *owner_map_;
    *owner_map_ = NULL;
  }
}

template class SharedMemLifecycle<SharedMemLockManager>;

}  // namespace net_instaweb
