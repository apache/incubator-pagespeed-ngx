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

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class Function;
class GoogleUrl;
class OutputResource;
class MessageHandler;
class RequestHeaders;
class RewriteOptions;

// Provides an overridable URL naming interface. This isolation layer allows
// us to override the rules for converting the original URL of a rewritten
// resource to something other than the default.
// The default implementation performs sharding and adds to the leaf name:
// '.pagespeed.<filter>.<hash>.<extension>'
class UrlNamer {
 public:
  class Callback {
   public:
    Callback() {}
    virtual ~Callback();
    // Provide the Callback function which will be executed once we have
    // rewrite_options. It is the responsibility of Done() function to
    // delete the Callback.
    virtual void Done(RewriteOptions* rewrite_options) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Callback);
  };
  UrlNamer();
  virtual ~UrlNamer();

  // Given an output resource and an optional set of options, generate the URL
  // that will be embedded in the rewritten page.
  //
  // Note: the default implementation returns the url of the output resource.
  virtual GoogleString Encode(const RewriteOptions* rewrite_options,
                              const OutputResource& output_resource) const;

  // Given the request_url, generate the original url.  If the URL naming
  // syntax supports an "owner" domain, and 'owner_domain' is non-null, then
  // this method writes the owner domain into that pointer.
  //
  // Returns 'false' if request_url was not encoded via this namer.
  //
  // Note: the default implementation always returns false.
  virtual bool Decode(const GoogleUrl& request_url,
                      GoogleUrl* owner_domain,
                      GoogleString* decoded) const;

  // Determines whether the provided request URL is authorized given the
  // RewriteOptions.
  //
  // The default implementation always return 'true'.
  virtual bool IsAuthorized(const GoogleUrl& request_url,
                            const RewriteOptions& options) const;

  // Given the request url and request headers, generate the rewrite options.
  virtual void DecodeOptions(const GoogleUrl& request_url,
                             const RequestHeaders& request_headers,
                             Callback* callback,
                             MessageHandler* handler) const;

  // Modifies the request prior to dispatch to the underlying fetcher.
  virtual void PrepareRequest(const RewriteOptions* rewrite_options,
                              GoogleString* url,
                              RequestHeaders* request_headers,
                              bool* success,
                              Function* func, MessageHandler* handler);

  // Determines whether the naming policy incorporates proxying resources
  // using a central proxy domain.
  virtual bool ProxyMode() const { return false; }

  // Determines whether the specified URL has been mapped to that central
  // proxy domain.
  virtual bool IsProxyEncoded(const GoogleUrl& url) const { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(UrlNamer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_URL_NAMER_H_
