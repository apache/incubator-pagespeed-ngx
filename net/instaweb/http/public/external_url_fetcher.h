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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_EXTERNAL_URL_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_EXTERNAL_URL_FETCHER_H_

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class AsyncFetch;
class MessageHandler;
class RequestHeaders;

// Runs an external command ('wget' by default, or 'curl') via popen
// for blocking URL fetches.

// TODO(vchudnov): Incorporate NetcatUrlFetcher functionality into
// this class.
class ExternalUrlFetcher : public UrlAsyncFetcher {
 public:
  ExternalUrlFetcher() {}
  virtual ~ExternalUrlFetcher() {}

  // TODO(sligocki): Allow protocol version number (e.g. HTTP/1.1)
  // and request type (e.g. GET, POST, etc.) to be specified.
  virtual void Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch);

  // Default user agent to use.
  static const char kDefaultUserAgent[];

  // Sets the path to "binary" when fetching using "how".
  void set_binary(const GoogleString& binary);


 protected:
  // Appends to escaped_headers one header line for each Name, Value
  // pair in request_headers.
  virtual void AppendHeaders(const RequestHeaders& request_headers,
                             StringVector* escaped_headers);

  GoogleString binary_;

 private:
  virtual const char* GetFetchLabel() = 0;

  // Returns the external command to run in order to fetch a URL. The
  // URL and the vector of header lines must be already escaped in
  // escaped_url and escaped_headers, respectively. In addition to the
  // specified headers, the User-Agent is also explicitly set to the
  // value of user_agent, unless the latter is NULL.
  virtual GoogleString ConstructFetchCommand(
      const GoogleString& escaped_url,
      const char* user_agent,
      const StringVector& escaped_headers) = 0;

  DISALLOW_COPY_AND_ASSIGN(ExternalUrlFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_EXTERNAL_URL_FETCHER_H_
