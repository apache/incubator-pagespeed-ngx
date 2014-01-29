/*
 * Copyright 2013 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)
//
// Special input resource for http://fonts.googleapis.com CSS,
// needed due to the UA dependence. The font service delivers different loader
// CSS for different user agents (optimizing the font differently), and
// therefore delivers its output as cache-control: private, making it normally
// untouchable for us. This class overrides that restriction by instead
// incorporating the UA string into the cache key we use and stripping the
// cache-control: private header.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_GOOGLE_FONT_SERVICE_INPUT_RESOURCE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_GOOGLE_FONT_SERVICE_INPUT_RESOURCE_H_

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/cacheable_resource_base.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class GoogleUrl;
class RequestHeaders;
class ResponseHeaders;
class RewriteDriver;
class Statistics;

class GoogleFontServiceInputResource : public CacheableResourceBase {
 public:
  // Returns NULL if not recognized as a valid font service URL.
  static GoogleFontServiceInputResource* Make(const GoogleUrl& url,
                                              RewriteDriver* rewrite_driver);
  virtual ~GoogleFontServiceInputResource();
  static void InitStats(Statistics* stats);

  // Returns true if the URL looks like one from font service.
  static bool IsFontServiceUrl(const GoogleUrl& url);

 protected:
  // Overrides of CacheableResourceBase API.
  virtual void PrepareRequest(const RequestContextPtr& request_context,
                              RequestHeaders* headers);
  virtual void PrepareResponseHeaders(ResponseHeaders* headers);

 private:
  GoogleFontServiceInputResource(RewriteDriver* rewrite_driver,
                                 bool is_https,
                                 const StringPiece& url,
                                 const StringPiece& cache_key,
                                 const GoogleString& user_agent);
  GoogleString user_agent_;
  bool is_https_;

  DISALLOW_COPY_AND_ASSIGN(GoogleFontServiceInputResource);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_GOOGLE_FONT_SERVICE_INPUT_RESOURCE_H_
