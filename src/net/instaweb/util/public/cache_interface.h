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
    SharedString* value() { return &value_; }

    // These methods are meant for use of callback subclasses that wrap
    // around other callbacks. Normal cache implementations should
    // just use CacheInterface::ValidateAndReportResult.
    bool DelegatedValidateCandidate(const GoogleString& key, KeyState state) {
      return ValidateCandidate(key, state);
    }

    void DelegatedDone(KeyState state) {
      Done(state);
    }

   protected:
    friend class CacheInterface;

    // This method exists to let cache clients do application-specific
    // validation of cache results. This is important for 2-level caches,
    // as with distributed setups it's possible that an entry in the L1 is
    // invalid (e.g. an HTTP resource past expiration), while the L2 cache
    // has a valid result.
    //
    // This method will be invoked for all potential cache results,
    // (with the value filled in into value()). Returning
    // 'false' lets the implementation effectively veto a value as
    // expired or invalid for semantic reasons.
    //
    // Note that implementations may not invoke any cache operations,
    // as it may be invoked with locks held.
    virtual bool ValidateCandidate(const GoogleString& key,
                                   KeyState state) { return true; }

    // This method is called once the cache implementation has found
    // a match that was accepted by ValidateCandidate (in which
    // case state == kAvailable) or it has failed to do so (state == kNotFound).
    //
    // Implementations are free to invoke cache operations, as all cache
    // locks are guaranteed to be released.
    virtual void Done(KeyState state) = 0;

   private:
    SharedString value_;
  };

  virtual ~CacheInterface();

  // Initiates a cache fetch, calling callback->ValidateCandidate()
  // and then callback->Done(state) when done.
  //
  // Note: implementations should normally invoke the callback via
  // ValidateAndReportResult, which will combine ValidateCandidate() and
  // Done() together properly.
  virtual void Get(const GoogleString& key, Callback* callback) = 0;

  // Puts a value into the cache.  The value that is passed in is not modified,
  // but the SharedString is passed by non-const pointer because its reference
  // count is bumped.
  virtual void Put(const GoogleString& key, SharedString* value) = 0;
  virtual void Delete(const GoogleString& key) = 0;

 protected:
  // Invokes callback->ValidateCandidate() and callback->Done() as appropriate.
  void ValidateAndReportResult(const GoogleString& key, KeyState state,
                               Callback* callback);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CACHE_INTERFACE_H_
