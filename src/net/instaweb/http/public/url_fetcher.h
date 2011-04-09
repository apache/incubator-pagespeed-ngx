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
// UrlFetcher is an interface for fetching urls.
//
// TODO(jmarantz): Consider asynchronous fetches.  This may not require
// a change in interface; we would simply always return 'false' if the
// url contents is not already cached.  We may want to consider a richer
// return-value enum to distinguish illegal ULRs from invalid ones, from
// ones where the fetch is in-progress.  Or maybe the caller doesn't care.

#ifndef NET_INSTAWEB_HTTP_PUBLIC_URL_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_URL_FETCHER_H_

#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class MessageHandler;
class RequestHeaders;
class ResponseHeaders;
class Writer;

class UrlFetcher {
 public:
  virtual ~UrlFetcher();

  // Fetch a URL, streaming the output to fetched_content_writer, and
  // returning the headers.  Returns true if the fetch was successful.
  virtual bool StreamingFetchUrl(const GoogleString& url,
                                 const RequestHeaders& request_headers,
                                 ResponseHeaders* response_headers,
                                 Writer* response_writer,
                                 MessageHandler* message_handler) = 0;

  // Convenience method for fetching URL into a string, with no headers in
  // our out.  This is primarily for upward compatibility.
  //
  // TODO(jmarantz): change callers to use StreamingFetchUrl and remove this.
  bool FetchUrl(const GoogleString& url,
                GoogleString* content,
                MessageHandler* message_handler);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_URL_FETCHER_H_
