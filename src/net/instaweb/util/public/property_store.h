/*
 * Copyright 2013 Google Inc.
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

// Author: pulkitg@google.com (Pulkit Goyal)
//
// Retrieves property values stored in the storage system and populate them
// in PropertyPage after validation of the properties.
#ifndef NET_INSTAWEB_UTIL_PUBLIC_PROPERTY_STORE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_PROPERTY_STORE_H_

#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/callback.h"

namespace net_instaweb {

class PropertyCacheValues;

// Abstract interface for implementing PropertyStore which helps to
// retrieve and put properties into the storage system.
class PropertyStore {
 public:
  PropertyStore();
  virtual ~PropertyStore();

  typedef Callback1<bool> BoolCallback;

  // Populates the values field and call the BoolCallback after lookup is done.
  // BoolCallback is called with true if values are populated successfully
  // in PropertyPage.
  // PropertyPage object is used to validate the entries looked up from cache.
  virtual void Get(
      const GoogleString& url,
      const GoogleString& options_signature_hash,
      UserAgentMatcher::DeviceType device_type,
      const PropertyCache::Cohort* cohort,
      PropertyPage* page,
      BoolCallback* done) = 0;

  // Write to storage system for the given key.
  virtual void Put(
      const GoogleString& url,
      const GoogleString& options_signature_hash,
      UserAgentMatcher::DeviceType device_type,
      const PropertyCache::Cohort* cohort,
      const PropertyCacheValues* values) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PropertyStore);
};

}  // namespace net_instaweb
#endif  // NET_INSTAWEB_UTIL_PUBLIC_PROPERTY_STORE_H_
