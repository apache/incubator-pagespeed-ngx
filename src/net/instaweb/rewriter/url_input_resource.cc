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

#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

UrlInputResource::UrlInputResource(RewriteDriver* rewrite_driver,
                                   const ContentType* type,
                                   const StringPiece& url)
    : CacheableResourceBase("url_input_resource", url, url /* cache_key */,
                            type, rewrite_driver) {
  response_headers()->set_implicit_cache_ttl_ms(
      rewrite_options()->implicit_cache_ttl_ms());
  response_headers()->set_min_cache_ttl_ms(
      rewrite_options()->min_cache_ttl_ms());
  set_disable_rewrite_on_no_transform(
      rewrite_options()->disable_rewrite_on_no_transform());
}

UrlInputResource::~UrlInputResource() {
}

void UrlInputResource::InitStats(Statistics* stats) {
  CacheableResourceBase::InitStats("url_input_resource", stats);
}

}  // namespace net_instaweb
