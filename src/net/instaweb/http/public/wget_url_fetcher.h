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

// Authors: jmarantz@google.com (Joshua Marantz)
//          vchudnov@google.com (Victor Chudnovsky)

#ifndef NET_INSTAWEB_HTTP_PUBLIC_WGET_URL_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_WGET_URL_FETCHER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/http/public/external_url_fetcher.h"

namespace net_instaweb {

class WgetUrlFetcher : public ExternalUrlFetcher {
 public:
  WgetUrlFetcher();
  virtual ~WgetUrlFetcher() {}

 private:
  virtual GoogleString ConstructFetchCommand(
      const GoogleString& escaped_url,
      const char* user_agent,
      const StringVector& escaped_headers);
  virtual const char* GetFetchLabel();

  DISALLOW_COPY_AND_ASSIGN(WgetUrlFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_WGET_URL_FETCHER_H_
