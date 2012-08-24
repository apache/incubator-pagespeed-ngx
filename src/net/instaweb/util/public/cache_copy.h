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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_CACHE_COPY_H_
#define NET_INSTAWEB_UTIL_PUBLIC_CACHE_COPY_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class SharedString;

// Makes a new cache object based on an existing one, adding no new
// functionality.  This is used for memory management purposes only,
// so that a cache can be shared between multiple consumers that each
// want to take ownership.
class CacheCopy : public CacheInterface {
 public:
  // Does *not* take ownership of the cache passed in.
  explicit CacheCopy(CacheInterface* cache)
      : cache_(cache),
        name_(StrCat("Copy of ", cache_->Name())) {
  }
  virtual ~CacheCopy();

  virtual void Get(const GoogleString& key, Callback* callback) {
    cache_->Get(key, callback);
  }
  virtual void Put(const GoogleString& key, SharedString* value) {
    cache_->Put(key, value);
  }
  virtual void Delete(const GoogleString& key) { cache_->Delete(key); }
  virtual const char* Name() const { return name_.c_str(); }
  virtual void MultiGet(MultiGetRequest* request) { cache_->MultiGet(request); }

 private:
  CacheInterface* cache_;
  GoogleString name_;

  DISALLOW_COPY_AND_ASSIGN(CacheCopy);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CACHE_COPY_H_
