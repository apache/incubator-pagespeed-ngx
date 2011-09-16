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
  UrlNamer();
  virtual ~UrlNamer();

  // Given an output resource and an optional set of options, generate the URL
  // that will be embedded in the rewritten page.
  virtual GoogleString Encode(const RewriteOptions* rewrite_options,
                              const OutputResource& output_resource);

  // Given the request_url and request_headers, generate the orignial url.
  virtual GoogleString Decode(const GoogleUrl& request_url,
                              const RequestHeaders& request_headers,
                              MessageHandler* handler);

  // Given the request url and request headers, generate the rewrite options.
  virtual RewriteOptions* DecodeOptions(const GoogleUrl& request_url,
                                        const RequestHeaders& request_headers,
                                        MessageHandler* handler);

 private:
  DISALLOW_COPY_AND_ASSIGN(UrlNamer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_URL_NAMER_H_
