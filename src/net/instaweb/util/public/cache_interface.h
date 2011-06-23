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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_CACHE_INTERFACE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_CACHE_INTERFACE_H_

#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

// Abstract interface for a cache.
class CacheInterface {
 public:
  enum KeyState {
    kAvailable,    // Requested key is available for serving
    kNotFound      // Requested key needs to be written
  };

  class Callback {
   public:
    virtual ~Callback();
    virtual void Done(KeyState state) = 0;
    SharedString* value() { return &value_; }

   private:
    SharedString value_;
  };

  virtual ~CacheInterface();

  // Initiates a cache fetch, calling callback->Done(state) when done.
  virtual void Get(const GoogleString& key, Callback* callback) = 0;
  //   virtual bool Get(const GoogleString& key, SharedString* value) = 0;

  // Puts a value into the cache.  The value that is passed in is not modified,
  // but the SharedString is passed by non-const pointer because its reference
  // count is bumped.
  virtual void Put(const GoogleString& key, SharedString* value) = 0;
  virtual void Delete(const GoogleString& key) = 0;

  // TODO(sligocki): Kill Query, it is unused.
  virtual void Query(const GoogleString& key, Callback* callback) = 0;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CACHE_INTERFACE_H_
