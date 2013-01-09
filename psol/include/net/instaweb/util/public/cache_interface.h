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

#include <vector>

#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"
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

  // Helper class for use with implementations for which IsBlocking is true.
  // It simply saves the state, value, and whether Done() has been called.
  class SynchronousCallback : public Callback {
   public:
    SynchronousCallback() { Reset(); }

    bool called() const { return called_; }
    KeyState state() const { return state_; }
    // super.value() is used to get/set the value.

    void Reset() {
      called_ = false;
      state_ = CacheInterface::kNotFound;
      SharedString empty;
      *value() = empty;
    }

    virtual void Done(CacheInterface::KeyState state) {
      called_ = true;
      state_ = state;
    }

   private:
    bool called_;
    CacheInterface::KeyState state_;

    DISALLOW_COPY_AND_ASSIGN(SynchronousCallback);
  };

  // Vector of structures used to initiate a MultiGet.
  struct KeyCallback {
    KeyCallback(const GoogleString& k, Callback* c) : key(k), callback(c) {}
    GoogleString key;
    Callback* callback;
  };
  typedef std::vector<KeyCallback> MultiGetRequest;

  virtual ~CacheInterface();

  // Initiates a cache fetch, calling callback->ValidateCandidate()
  // and then callback->Done(state) when done.
  //
  // Note: implementations should normally invoke the callback via
  // ValidateAndReportResult, which will combine ValidateCandidate() and
  // Done() together properly.
  virtual void Get(const GoogleString& key, Callback* callback) = 0;

  // Gets multiple keys, calling multiple callbacks.  Default implementation
  // simply loops over all the keys and calls Get.
  //
  // MultiGetRequest, declared above, is a vector of structs of keys
  // and callbacks.
  //
  // Ownership of the request is transferred to this function.
  virtual void MultiGet(MultiGetRequest* request);

  // Puts a value into the cache.  The value that is passed in is not modified,
  // but the SharedString is passed by non-const pointer because its reference
  // count is bumped.
  virtual void Put(const GoogleString& key, SharedString* value) = 0;
  virtual void Delete(const GoogleString& key) = 0;

  // Convenience method to do a Put from a GoogleString* value.  The
  // bytes will be swapped out of the value and into a temp
  // SharedString.
  void PutSwappingString(const GoogleString& key, GoogleString* value) {
    SharedString shared_string;
    shared_string.SwapWithString(value);
    Put(key, &shared_string);
  }

  // The name of this CacheInterface -- used for logging and debugging.
  virtual const char* Name() const = 0;

  // Returns true if this cache is guaranteed to call its callbacks before
  // returning from Get and MultiGet.
  virtual bool IsBlocking() const = 0;

  // Returns true if the cache is in a healthy state.  Memory and
  // file-based caches can simply return 'true'.  But for server-based
  // caches, it is handy to be able to query to see whether it is in a
  // good state.  It should be safe to call this frequently -- the
  // implementation shouldn't do much more than check a bool flag
  // under mutex.
  virtual bool IsHealthy() const = 0;

  // Stops all cache activity.  Further Put/Delete calls will be dropped, and
  // MultiGet/Get will call the callback with kNotFound immediately.  Note there
  // is no Enable(); once the cache is stopped it is stopped forever.  This
  // function is intended for use during process shutdown.
  virtual void ShutDown() = 0;

  // To deal with underlying cache systems (e.g. memcached) that
  // cannot tolerate arbitrary-sized keys, we use a hash of the
  // key and put the key in the value, using the functions in
  // key_value_codec.h.
  //
  // To do this without pointlessly copying the value bytes, we
  // use SharedString::Append().   However, that's not thread-safe.
  // So when making a cache Asynchronous with AsyncCache, we must
  // do the SharedString::Append call in the thread that initiates
  // the Put, before queuing a threaded Put.
  //
  // This method indicates whether a cache implementation requires
  // encoding the keys in the value using key_value_codec.
  virtual bool MustEncodeKeyInValueOnPut() const { return false; }

  // Performs a cache Put, but assumes the key has already been
  // encoded into the value with key_value_codec.  It is only valid
  // to call this when MustEncodeKeyInValueOnPut() returns true.
  virtual void PutWithKeyInValue(const GoogleString& key,
                                 SharedString* key_and_value) {
    CHECK(false);
  }

 protected:
  // Invokes callback->ValidateCandidate() and callback->Done() as appropriate.
  void ValidateAndReportResult(const GoogleString& key, KeyState state,
                               Callback* callback);

  // Helper method to report a NotFound on each MultiGet key.  Deletes
  // the request.
  void ReportMultiGetNotFound(MultiGetRequest* request);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CACHE_INTERFACE_H_
