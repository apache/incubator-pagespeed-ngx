/**
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_WGET_URL_FETCHER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_WGET_URL_FETCHER_H_

#include <stdio.h>
#include "base/basictypes.h"
#include <string>
#include "net/instaweb/util/public/url_fetcher.h"

namespace net_instaweb {

// Runs 'wget' via popen for blocking URL fetches.
class WgetUrlFetcher : public UrlFetcher {
 public:
  WgetUrlFetcher() { }
  virtual ~WgetUrlFetcher();

  // TODO(sligocki): Allow protocol version number (e.g. HTTP/1.1)
  // and request type (e.g. GET, POST, etc.) to be specified.
  virtual bool StreamingFetchUrl(const std::string& url,
                                 const MetaData& request_headers,
                                 MetaData* response_headers,
                                 Writer* writer,
                                 MessageHandler* message_handler);

  // Default user agent to use.
  static const char kDefaultUserAgent[];

 private:
  DISALLOW_COPY_AND_ASSIGN(WgetUrlFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_WGET_URL_FETCHER_H_
