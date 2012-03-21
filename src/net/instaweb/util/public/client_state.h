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

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_client_state.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

#include "testing/gtest/include/gtest/gtest_prod.h"

namespace net_instaweb {

class ClientStateMsg;
class PropertyCache;
class PropertyPage;
class Timer;

// Basic implementation of AbstractClientState which uses a FIFO queue to track
// the most recently accessed URLs by a given client.
//
// See AbstractClientState for a description of the interface.
class ClientState : public AbstractClientState {
 public:
  // Cohort descriptor for PropertyCache lookups of ClientState objects.
  static const char kClientStateCohort[];
  // PropertyValue descriptor for PropertyCache lookups of ClientState objects.
  static const char kClientStatePropertyValue[];
  // Maximum number of URLs tracked for each client.
  static const uint32 kClientStateMaxUrls;
  // URLs with expiry times below this threshold will not be tracked.
  static const int64 kClientStateExpiryTimeThresholdMs;

  ClientState();
  virtual ~ClientState();

  virtual bool InCache(const GoogleString& url);
  virtual void Set(const GoogleString& url, int64 expire_ms);
  virtual void Clear();
  virtual const GoogleString& ClientId() const { return client_id_; }
  virtual bool InitFromPropertyCache(
      const GoogleString& client_id,
      PropertyCache* property_cache,
      PropertyPage* property_page,
      Timer* timer);
  virtual void WriteBackToPropertyCache();

 private:
  // Packs this ClientState into the given protobuffer.
  void Pack(ClientStateMsg* proto);
  // Unpacks state from the given protobuffer into this,
  // replacing any previous contents. Returns true if successful.
  bool Unpack(const ClientStateMsg& proto);

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

  friend class ClientStateTest;
  FRIEND_TEST(ClientStateTest, PackUnpackWorks);
  FRIEND_TEST(ClientStateTest, PropertyCacheWorks);

  DISALLOW_COPY_AND_ASSIGN(ClientState);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CLIENT_STATE_H_
