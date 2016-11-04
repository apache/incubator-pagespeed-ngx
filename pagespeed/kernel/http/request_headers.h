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

#ifndef PAGESPEED_KERNEL_HTTP_REQUEST_HEADERS_H_
#define PAGESPEED_KERNEL_HTTP_REQUEST_HEADERS_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/headers.h"

namespace net_instaweb {

class HttpRequestHeaders;
class MessageHandler;
class Writer;

// Read/write API for HTTP request (RequestHeaders is a misnomer).
class RequestHeaders : public Headers<HttpRequestHeaders> {
 public:
  enum Method { kOptions, kGet, kHead, kPost, kPut, kDelete, kTrace, kConnect,
                kPatch, kPurge, kError };

  // To compute cacheability, we have to know a few properties of the request
  // headers, potentially carrying them through cache lookups.  The request
  // headers themselves can be expensive and we don't need (for example) the
  // entire contents of cookies to understand whether there were cookies.  In
  // fact we can store the request properties we need in the space of a single
  // int (for now).
  struct Properties {
    Properties()                 // The default constructor assumes all
        : has_cookie(true),      // anti-caching signals are present.
          has_cookie2(true),
          has_authorization(false) {  // But we assume no authorization
                                      // unless populated.
    }
    Properties(bool cookie, bool cookie2, bool authorization)
        : has_cookie(cookie),
          has_cookie2(cookie2),
          has_authorization(authorization) {
    }
    bool has_cookie;
    bool has_cookie2;
    bool has_authorization;
  };

  RequestHeaders();

  virtual void Clear();
  void CopyFromProto(const HttpRequestHeaders& p);
  void CopyFrom(const RequestHeaders& other);

  GoogleString ToString() const;
  Method method() const;
  const char* method_string() const;
  void set_method(Method method);

  // This is encoded message body, a rewriter or fetcher
  // may opt to translate to entity-body only after removing
  // header which has encoding information.
  // TODO(atulvasu): Support something like a Rope for larger post bodies.
  // TODO(atulvasu): Use a Writer instead of a string.
  const GoogleString& message_body() const;
  void set_message_body(const GoogleString& data);

  using Headers<HttpRequestHeaders>::WriteAsHttp;
  bool WriteAsHttp(const StringPiece& url, Writer* writer,
                   MessageHandler* handler) const;

  // Determines whether a request header accepts gzipped content.
  bool AcceptsGzip() const;

  // Returns true if these request headers are for an XmlHttp request (i.e. ajax
  // request).  This mechanism is not reliable because sometimes this header is
  // not set even for XmlHttp requests.
  bool IsXmlHttpRequest() const;

  // Calling this method on an object that will not have any mutating
  // operations called on it afterwards will ensure that it will not do any
  // lazy initialization behind the scenes.
  void PopulateLazyCaches() { PopulateMap(); }

  Properties GetProperties() const;

  // Return a const reference to the multimap of cookies. The mapping is:
  //   cookie name -> (cookie value, empty StringPiece)
  // [the empty StringPiece is for cookie attributes; since Cookie headers
  //  don't have attributes it's empty; it's really for SetCookie headers]
  // It's a multimap to cater for the same cookie being set multiple times;
  // how this is handled is up to the caller.
  const CookieMultimap& GetAllCookies() const;

  // Determines whether the specified Cookie is present in the request.
  bool HasCookie(StringPiece cookie_name) const;

  // Determines whether the specified Cookie and value are present in the
  // request.
  bool HasCookieValue(StringPiece cookie_name, StringPiece cookie_value) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(RequestHeaders);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTTP_REQUEST_HEADERS_H_
