// Copyright 2013 Google Inc. All Rights Reserved.
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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DOWNSTREAM_CACHING_DIRECTIVES_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DOWNSTREAM_CACHING_DIRECTIVES_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest_prod.h"
#include "pagespeed/kernel/base/string.h"  // for GoogleString

namespace net_instaweb {

class RequestHeaders;

// This class keeps track of the properties that are specified via
// directives from the downstream caching layer (e.g. varnish/proxy_cache),
// to indicate whether certain optimizations are to be supported or not
// These directives are currently specified via the PS-CapabilityList
// request header value.
class DownstreamCachingDirectives {
 public:
  // A string that indicates that no UserAgent-dependent-optimization
  // constraints are specified for this request.
  static const char kNoCapabilitiesSpecified[];

  DownstreamCachingDirectives();
  virtual ~DownstreamCachingDirectives();

  // Parses the capability-list related request header value and stores this
  // for future queries regarding supported capabilities for the request.
  void ParseCapabilityListFromRequestHeaders(
      const RequestHeaders& request_headers);

  // TODO(anupama): Go through all DeviceProperties/RequestProperties methods
  // and re-evaluate which of them need to be supported here.
  bool SupportsImageInlining() const;
  bool SupportsLazyloadImages() const;
  // TODO(anupama): Incorporate the "allow_mobile" parameter used in
  // DeviceProperties for supportsJsDefer().
  bool SupportsJsDefer() const;
  bool SupportsWebp() const;
  bool SupportsWebpLosslessAlpha() const;
  bool SupportsWebpAnimated() const;

 private:
  // Helper method for figuring out support for a given capability based on
  // the following:
  // If the supports_property LazyBool attribute is set, its true/false value
  // is returned.
  // Else capability_list is checked for the presence of capability to decide
  // whether the capability is supported or not. This true/false value is also
  // stored in the LazyBool attribute for future uses.
  // Note: Presence of kNoCapabilitiesSpecified in the capability_list
  // indicates that no UserAgent-dependent-optimization constraints were
  // specified in the request. An empty string in the capability_list indicates
  // that no UserAgent dependent optimizations are to be allowed on this
  // request. All other values in the comma-separated parts of the
  // capability_list correspond to 2-letter filter ids identifying
  // capabilities to be supported in the response.
  static bool IsPropertySupported(LazyBool* supports_property,
                                  const GoogleString& capability,
                                  const GoogleString& capability_list);

  mutable LazyBool supports_image_inlining_;
  mutable LazyBool supports_js_defer_;
  mutable LazyBool supports_lazyload_images_;
  mutable LazyBool supports_webp_;
  mutable LazyBool supports_webp_lossless_alpha_;
  mutable LazyBool supports_webp_animated_;

  GoogleString capabilities_to_be_supported_;

  DISALLOW_COPY_AND_ASSIGN(DownstreamCachingDirectives);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DOWNSTREAM_CACHING_DIRECTIVES_H_
