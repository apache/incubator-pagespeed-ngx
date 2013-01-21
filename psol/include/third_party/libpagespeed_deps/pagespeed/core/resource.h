// Copyright 2009 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_CORE_RESOURCE_H_
#define PAGESPEED_CORE_RESOURCE_H_

#include <string>

#include "base/basictypes.h"
#include "pagespeed/core/string_util.h"
#include "pagespeed/proto/resource.pb.h"

namespace pagespeed {

class Resource;

/**
 * Represents an individual input resource.
 */
class Resource {
 public:
  typedef pagespeed::string_util::CaseInsensitiveStringStringMap HeaderMap;

  Resource();
  virtual ~Resource();

  // Setter methods
  void SetRequestUrl(const std::string& value);
  void SetRequestMethod(const std::string& value);
  void AddRequestHeader(const std::string& name, const std::string& value);
  void SetRequestBody(const std::string& value);
  void SetResponseStatusCode(int code);
  void AddResponseHeader(const std::string& name, const std::string& value);
  void RemoveResponseHeader(const std::string& name);
  void SetResponseBody(const std::string& value);
  void SetResponseBodyModified(bool modified) {
    response_body_modified_ = modified;
  }
  void SetResponseProtocol(const std::string& protocol);
  void SetResponseProtocol(Protocol protocol) {
    response_protocol_ = protocol;
  }

  // In some cases, the Cookie header can differ from the cookie(s)
  // that would be associated with a resource. For instance, if a resource
  // is fetched before a Set-Cookie is applied, the cookies in that
  // Set-Cookie will not be included in the request for the resource. Some
  // rules want to know about the cookies that would be applied to a
  // resource. You can use the SetCookies method to specify the set of
  // cookies that are associated with a given resource. This is optional;
  // if unspecified, GetCookies will return the contents of the Cookie
  // header.
  void SetCookies(const std::string& cookies);

  // In some cases, the mime type specified in the Content-Type header
  // can differ from the actual resource type. For instance, some sites
  // serve JavaScript files with Content-Type: text/html. In those cases,
  // call SetResourceType() to explicitly specify the resource type.
  // Note that the status code is always preferred when determining
  // the resource type. A redirect status code will always cause
  // GetResourceType() to return REDIRECT, and a non-success code
  // (e.g. 500) will always cause GetResourceType() to return OTHER,
  // even if SetResourceType() has been explicitly called.
  void SetResourceType(ResourceType type);

  // Set the time that this resource was requested, in milliseconds,
  // relative to the request time of the first request. Thus the first
  // request's start time will be 0.
  void SetRequestStartTimeMillis(int start_millis);

  // Accessor methods
  const std::string& GetRequestUrl() const;

  // Get the HTTP method used when issuing the request, e.g. GET,
  // POST, etc.
  const std::string& GetRequestMethod() const;

  // Get a specific HTTP request header. The lookup is
  // case-insensitive. If the header is not present, the empty string
  // is returned.
  const std::string& GetRequestHeader(const std::string& name) const;

  // Get the body sent with the request. This only makes sense for
  // POST requests.
  const std::string& GetRequestBody() const;

  // Get the status code (e.g. 200) for the response.
  int GetResponseStatusCode() const;

  // Get a specific HTTP response header. The lookup is
  // case-insensitive. If the header is not present, the empty string
  // is returned.
  const std::string& GetResponseHeader(const std::string& name) const;

  // Get the body sent with the response (e.g. the HTML, CSS,
  // JavaScript, etc content). This is the body after applying any
  // content decodings (e.g. post ungzipping the response).
  const std::string& GetResponseBody() const;

  // Check if the response body modified for the purpose of analysis. We should
  // not save optimized content if the response body is modified. Note: the
  // response body may be modified to fix invalid Unicode code points.
  bool IsResponseBodyModified() const {
    return response_body_modified_;
  }


  // Get the cookies specified via SetCookies. If SetCookies was
  // unspecified, this will fall back to the Cookie request header. If that
  // header is empty, this method falls back to the Set-Cookie response
  // header.
  const std::string& GetCookies() const;

  // Do we have a request start time for this resource? Note that we
  // do not provide a getter for the request start time, because we do not
  // want rules to be implemented in terms of timing data from a single
  // page speed run. Timing data can vary greatly between page loads so
  // using timing data in a rule could introduce nondeterminism in the
  // results.
  bool has_request_start_time_millis() const {
    return request_start_time_millis_ >= 0;
  }

  // Is the request start time of this resource less than the request
  // start time of the specified resource? It is an error to call this
  // method if either this resource or other resource do not have a request
  // start time specified.
  bool IsRequestStartTimeLessThan(const Resource& other) const;

  // For serialization purposes only.
  // Use GetRequestHeader/GetResponseHeader methods above for key lookup.
  const HeaderMap* GetRequestHeaders() const;
  const HeaderMap* GetResponseHeaders() const;

  // Helper methods

  // extract the host std::string from the request url
  std::string GetHost() const;

  // extract the protocol std::string from the request url
  std::string GetProtocol() const;

  // Get the protocol string from response, e.g., HTTP/1.1.
  const char* GetResponseProtocolString() const;

  // Get the protocol from response, e.g., HTTP_11.
  Protocol GetResponseProtocol() const {
    return response_protocol_;
  }

  // Extract resource type from the Content-Type header.
  ResourceType GetResourceType() const;
  ImageType GetImageType() const;

  bool SerializeData(ResourceData* data) const;

 private:
  // We let PagespeedInput access our private data in order to inspect
  // the request_start_time_millis_ field. We do not want to expose
  // this field to rules since we don't want rules implemented in
  // terms of absolute resource timing information, since it would
  // lead to nondeterminism in results.
  friend class PagespeedInput;

  std::string request_url_;
  std::string request_method_;
  HeaderMap request_headers_;
  std::string request_body_;
  bool response_body_modified_;
  int status_code_;
  Protocol response_protocol_;
  HeaderMap response_headers_;
  std::string response_body_;
  std::string cookies_;
  ResourceType type_;
  int request_start_time_millis_;

  DISALLOW_COPY_AND_ASSIGN(Resource);
};

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_RESOURCE_H_
