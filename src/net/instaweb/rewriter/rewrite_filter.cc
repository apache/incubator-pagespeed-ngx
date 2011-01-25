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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/rewrite_filter.h"

namespace net_instaweb {

RewriteFilter::~RewriteFilter() {
}

OutputResource* RewriteFilter::CreateOutputResourceFromResource(
    const ContentType* content_type,
    UrlSegmentEncoder* encoder,
    Resource* input_resource) {
  ResourceManager* resource_manager = driver_->resource_manager();
  return resource_manager->CreateOutputResourceFromResource(
      filter_prefix_, content_type,
      encoder, input_resource, driver_->options(),
      driver_->html_parse()->message_handler());
}

}  // namespace net_instaweb
