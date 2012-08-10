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

#include "net/instaweb/util/public/cache_interface.h"

namespace net_instaweb {

CacheInterface::~CacheInterface() {
}

CacheInterface::Callback::~Callback() {
}

void CacheInterface::ValidateAndReportResult(const GoogleString& key,
                                             KeyState state,
                                             Callback* callback) {
  if (!callback->ValidateCandidate(key, state)) {
    state = kNotFound;
  }
  callback->Done(state);
}

void CacheInterface::MultiGet(MultiGetRequest* request) {
  for (int i = 0, n = request->size(); i < n; ++i) {
    KeyCallback* key_callback = &(*request)[i];
    Get(key_callback->key, key_callback->callback);
  }
  delete request;
}

}  // namespace net_instaweb
