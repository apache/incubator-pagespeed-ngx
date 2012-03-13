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

// Author: mdw@google.com (Matt Welsh)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_CLIENT_STATE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_CLIENT_STATE_H_

#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/client_state.pb.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

// Represent state tracked on a per-client basis. For now, this class
// estimates whether a given object is resident in the client's cache.
//
// Note that this class makes no attempt at thread safety; it is up to
// the caller to ensure that operations on a ClientState object are
// performed safely.
class ClientState {
 public:
  // Cohort descriptor for PropertyCache lookups of ClientState objects.
  static const char kClientStateCohort[];
  // PropertyValue descriptor for PropertyCache lookups of ClientState objects.
  static const char kClientStatePropertyValue[];
  // Maximum number of URLs tracked for each client.
  static const uint32 kClientStateMaxUrls;
  // URLs with expiry times below this threshold will not be tracked.
  static const int64 kClientStateExpiryTimeThresholdMs;

  // Returns a new (empty) ClientState object for this client ID.
  ClientState(const GoogleString& client_id, Timer* timer);

  virtual ~ClientState();

  // Returns an estimate of whether the client is caching this URL.
  // Note that this is a best-effort guess and may not be accurate
  // with respect to the true client cache state.
  virtual bool InCache(const GoogleString& url);

  // Used to indicate that the given client is storing this URL for
  // up to expire_ms.
  virtual void Set(const GoogleString& url, int64 expire_ms);

  // Clears cached knowledge for this client.
  virtual void Clear();

  // Returns the client ID.
  const GoogleString& client_id() { return client_id_; }

  // Packs this ClientState into the given ClientStateMsg protobuf.
  void Pack(ClientStateMsg* message);

  // Unpacks the ClientStateMsg protobuf and returns a new ClientState
  // object. If the protobuf is incorrectly formatted, will return NULL.
  // The caller is responsible for deleting the returned object.
  static ClientState* Unpack(const ClientStateMsg& proto, Timer* timer);

  // Used to initialize a ClientState from a property cache read.
  // Unpack ClientState from the given PropertyPage, and returns a new
  // ClientState object. If the PropertyPage does not contain a
  // ClientState object (e.g., due to a cache lookup failure),
  // returns a new ClientState object for the associated client ID.
  // Otherwise, takes ownership of the property_page.
  static ClientState* UnpackFromPropertyCache(
      const GoogleString& client_id,
      PropertyCache* property_cache,
      PropertyPage* property_page,
      Timer* timer);

  // Write this ClientState back to the property cache.
  // It is an error to call this method on a ClientState object not
  // returned from UnpackFromPropertyPage.
  virtual void WriteBackToPropertyCache();

 private:
  // Internal function to initialize a ClientState from a pcache read.
  static ClientState* InitFromPropertyCache(
      PropertyCache* property_cache,
      PropertyPage* property_page,
      Timer* timer);

  // Client ID.
  GoogleString client_id_;
  // Timer.
  Timer* timer_;
  // Creation time in msec since the epoch.
  int64 create_time_ms_;
  // Property cache page from whence this ClientState came.
  // The following two fields will be NULL if this ClientState
  // was not initialized from a property cache lookup.
  scoped_ptr<PropertyPage> property_page_;
  PropertyCache* property_cache_;
  // Maintains a FIFO queue of recently-seen URLs.
  StringVector recent_urls_;

  DISALLOW_COPY_AND_ASSIGN(ClientState);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CLIENT_STATE_H_
