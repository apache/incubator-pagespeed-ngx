// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_HTTP_PUBLIC_REQUEST_HEADERS_H_
#define NET_INSTAWEB_HTTP_PUBLIC_REQUEST_HEADERS_H_

#include "net/instaweb/http/public/headers.h"

namespace net_instaweb {

class HttpRequestHeaders;

// Read/write API for HTTP request headers.
class RequestHeaders : public Headers<HttpRequestHeaders> {
 public:
  enum Method { kOptions, kGet, kHead, kPost, kPut, kDelete, kTrace, kConnect };

  RequestHeaders();

  void Clear();
  void CopyFrom(const RequestHeaders& other);

  GoogleString ToString() const;
  Method method() const;
  const char* method_string() const;
  void set_method(Method method);

  using Headers<HttpRequestHeaders>::WriteAsHttp;
  bool WriteAsHttp(const StringPiece& url, Writer* writer,
                   MessageHandler* handler) const;

  // Determines whether a request header accepts gzipped content.
  bool AcceptsGzip() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(RequestHeaders);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_REQUEST_HEADERS_H_
