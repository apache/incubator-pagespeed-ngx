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
//         morlovich@google.com (Maksim Orlovich)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_DELEGATING_CACHE_CALLBACK_H_
#define NET_INSTAWEB_UTIL_PUBLIC_DELEGATING_CACHE_CALLBACK_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

// Helper class for implementing Caches that wrap other caches, adding
// functionality in callbacks.
class DelegatingCacheCallback : public CacheInterface::Callback {
 public:
  explicit DelegatingCacheCallback(CacheInterface::Callback* callback);
  virtual ~DelegatingCacheCallback();

  // Note that we have to forward validity faithfully here, as if we're
  // wrapping a 2-level cache it will need to know accurately if the value
  // is valid or not.
  virtual bool ValidateCandidate(const GoogleString& key,
                                 CacheInterface::KeyState state);

  virtual void Done(CacheInterface::KeyState state);

 private:
  CacheInterface::Callback* callback_;
  bool validate_candidate_called_;

  DISALLOW_COPY_AND_ASSIGN(DelegatingCacheCallback);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_DELEGATING_CACHE_CALLBACK_H_
