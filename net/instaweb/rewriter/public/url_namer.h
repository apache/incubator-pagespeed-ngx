/*
 * Copyright 2011 Google Inc.
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

// Author: matterbury@google.com (Matt Atterbury)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_URL_NAMER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_URL_NAMER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

class AsyncFetch;
class GoogleUrl;
class OutputResource;
class RequestHeaders;
class RewriteOptions;

// Provides an overridable URL naming interface. This isolation layer allows
// us to override the rules for converting the original URL of a rewritten
// resource to something other than the default.
// The default implementation performs sharding and adds to the leaf name:
// '.pagespeed.<filter>.<hash>.<extension>'
class UrlNamer {
 public:
  enum EncodeOption {
    kSharded,
    kUnsharded
  };

  // This encodes whether we do some sort of mapping of resources to a
  // separate proxy domain
  enum class ProxyExtent {
    kNone,
    kInputOnly,  // We see requests on this domain, but don't produce it
                 // ourselves.
    kFull,       // All resources are moved.
  };

  UrlNamer();
  virtual ~UrlNamer();

  // Given an output resource and an optional set of options, generate the URL
  // that will be embedded in the rewritten page.
  //
  // encode_options is used to determine whether sharding is applied in this
  // encoding.
  //
  // Note: the default implementation returns the url of the output resource.
  virtual GoogleString Encode(const RewriteOptions* rewrite_options,
                              const OutputResource& output_resource,
                              EncodeOption encode_option) const;

  // Given the request_url, generate the original url.
  //
  // Returns 'false' if request_url was not encoded via this namer.
  //
  // Note: the default implementation always returns false.
  // Note: rewrite_options may be NULL.
  virtual bool Decode(const GoogleUrl& request_url,
                      const RewriteOptions* rewrite_options,
                      GoogleString* decoded) const;

  // Determines whether the provided request URL is authorized given the
  // RewriteOptions.
  //
  // The default implementation uses the domain lawyer in the options.
  virtual bool IsAuthorized(const GoogleUrl& request_url,
                            const RewriteOptions& options) const;

  // Configure custom options. Note that options may be NULL.
  virtual void ConfigureCustomOptions(const RequestHeaders& request_headers,
                                      RewriteOptions* options) const {}

  // Determines whether the naming policy incorporates proxying resources
  // using a central proxy domain.
  virtual ProxyExtent ProxyMode() const { return ProxyExtent::kNone; }

  // Determines whether the specified URL has been mapped to that central
  // proxy domain.
  virtual bool IsProxyEncoded(const GoogleUrl& url) const { return false; }

  // Prepare Fetch for cross-domain request.
  virtual void PrepForCrossDomain(AsyncFetch*) const { }

  const GoogleString& proxy_domain() const { return proxy_domain_; }

  void set_proxy_domain(const GoogleString& proxy_domain) {
    proxy_domain_ = proxy_domain;
  }

 private:
  GoogleString proxy_domain_;

  DISALLOW_COPY_AND_ASSIGN(UrlNamer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_URL_NAMER_H_
