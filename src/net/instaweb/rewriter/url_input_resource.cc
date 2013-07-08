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
//         jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/url_input_resource.h"

#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

UrlInputResource::UrlInputResource(RewriteDriver* rewrite_driver,
                                   const RewriteOptions* options,
                                   const ContentType* type,
                                   const StringPiece& url)
    : CacheableResourceBase("url_input_resource", rewrite_driver, type),
      url_(url.data(), url.size()),
      respect_vary_(rewrite_options()->respect_vary()) {
  response_headers()->set_implicit_cache_ttl_ms(
      options->implicit_cache_ttl_ms());
  set_enable_cache_purge(options->enable_cache_purge());
  set_disable_rewrite_on_no_transform(
      options->disable_rewrite_on_no_transform());
}

UrlInputResource::~UrlInputResource() {
}

void UrlInputResource::InitStats(Statistics* stats) {
  CacheableResourceBase::InitStats("url_input_resource", stats);
}

bool UrlInputResource::IsValidAndCacheableImpl(
    const ResponseHeaders& headers) const {
  if (headers.status_code() != HttpStatus::kOK) {
    return false;
  }

  bool cacheable = true;
  if (respect_vary_) {
    // Conservatively assume that the request has cookies, since the site may
    // want to serve different content based on the cookie. If we consider the
    // response to be cacheable here, we will serve the optimized version
    // without contacting the origin which would be against the webmaster's
    // intent. We also don't have cookies available at lookup time, so we
    // cannot try to use this response only when the request doesn't have a
    // cookie.
    cacheable = headers.VaryCacheable(true);
  } else {
    cacheable = headers.IsProxyCacheable();
  }
  // If we are setting a TTL for HTML, we cannot rewrite any resource
  // with a shorter TTL.
  cacheable &= (headers.cache_ttl_ms() >=
                rewrite_options()->min_resource_cache_time_to_rewrite_ms());

  if (!cacheable && !http_cache()->force_caching()) {
    return false;
  }

  // NULL is OK here since we make the request_headers ourselves.
  return !http_cache()->IsAlreadyExpired(NULL, headers);
}

}  // namespace net_instaweb
