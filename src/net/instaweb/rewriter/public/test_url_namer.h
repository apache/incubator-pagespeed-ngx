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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_TEST_URL_NAMER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_TEST_URL_NAMER_H_

#include "net/instaweb/rewriter/public/url_namer.h"

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class GoogleUrl;
class OutputResource;
class ResourceNamer;
class RewriteOptions;

// Implements a non-standard URL naming scheme that changes the domain and
// the path, thereby testing various code path flows.
class TestUrlNamer : public UrlNamer {
 public:
  TestUrlNamer();
  virtual ~TestUrlNamer();

  virtual GoogleString Encode(const RewriteOptions* rewrite_options,
                              const OutputResource& output_resource) const;

  virtual bool Decode(const GoogleUrl& request_url,
                      GoogleUrl* owner_domain,
                      GoogleString* decoded) const;

  static GoogleString EncodeUrl(const StringPiece& original_base,
                                const StringPiece& unmapped_base,
                                const StringPiece& resolved_path,
                                const ResourceNamer& leaf_details);

  // Determines whether the naming policy incorporates proxying resources
  // using a central proxy domain.
  virtual bool ProxyMode() const { return proxy_mode_; }

  // Determines whether the specified URL has been mapped to our proxy domain.
  virtual bool IsProxyEncoded(const GoogleUrl& url) const;

  // Set whether a test needs the URL namer to act in proxy mode.
  static void SetProxyMode(bool value) { proxy_mode_ = value; }

  // If a test needs normal encoding, even when TestUrlNamer is wired in,
  // they can set this on and Encode() will redirect to UrlNamer::Encode().
  static void UseNormalEncoding(bool yes) {
    use_normal_encoding_ = yes;
  }

  static bool UseNormalEncoding() { return use_normal_encoding_; }

 private:
  bool IsOriginEncoded(const GoogleUrl& url) const;
  bool IsPathEncoded(const GoogleUrl& url) const;

  static bool use_normal_encoding_;
  static bool proxy_mode_;

  DISALLOW_COPY_AND_ASSIGN(TestUrlNamer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_TEST_URL_NAMER_H_
