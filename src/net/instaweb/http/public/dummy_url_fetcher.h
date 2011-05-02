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

// Author: sligocki@google.com (Shawn Ligocki)
//
// Dummy implementation that aborts if used (useful for tests).

#ifndef NET_INSTAWEB_HTTP_PUBLIC_DUMMY_URL_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_DUMMY_URL_FETCHER_H_

#include "net/instaweb/http/public/url_fetcher.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class MessageHandler;
class RequestHeaders;
class ResponseHeaders;
class Writer;

class DummyUrlFetcher : public UrlFetcher {
 public:
  virtual bool StreamingFetchUrl(const GoogleString& url,
                                 const RequestHeaders& request_headers,
                                 ResponseHeaders* response_headers,
                                 Writer* fetched_content_writer,
                                 MessageHandler* message_handler);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_DUMMY_URL_FETCHER_H_
