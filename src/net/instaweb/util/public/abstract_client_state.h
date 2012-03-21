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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_ABSTRACT_CLIENT_STATE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_ABSTRACT_CLIENT_STATE_H_

#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

// Represent state tracked on a per-client basis. For now, this interface
// estimates whether a given object is resident in the client's cache.
class AbstractClientState {
 public:
  virtual ~AbstractClientState() { }

  // Returns an estimate of whether the client is caching this URL.
  // Note that this is a best-effort guess and may not be accurate
  // with respect to the true client cache state.
  virtual bool InCache(const GoogleString& url) = 0;

  // Used to indicate that the given client is storing this URL for
  // up to expire_ms.
  virtual void Set(const GoogleString& url, int64 expire_ms) = 0;

  // Clears cached knowledge for this client.
  virtual void Clear() = 0;

  // Returns the client ID associated with this ClientState object.
  virtual const GoogleString& ClientId() const = 0;

  // Initialize a ClientState from a property cache read. If the PropertyPage
  // does not contain a ClientState object (e.g., due to a cache lookup
  // failure), this method returns false. Otherwise, returns true.
  // The ClientState takes ownership of the property_page in both cases.
  virtual bool InitFromPropertyCache(
      const GoogleString& client_id,
      PropertyCache* property_cache,
      PropertyPage* property_page,
      Timer* timer) = 0;

  // Write this ClientState back to the property cache.
  // It is an error to call this method without first having called
  // InitFromPropertyCache() and having 'true' returned from that method.
  virtual void WriteBackToPropertyCache() = 0;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_ABSTRACT_CLIENT_STATE_H_
