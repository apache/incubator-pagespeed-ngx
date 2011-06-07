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

#include <cstddef>                     // for NULL

#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_segment_encoder.h"

namespace net_instaweb {

class RewriteContext;

RewriteFilter::~RewriteFilter() {
}

ResourcePtr RewriteFilter::CreateInputResourceFromOutputResource(
    OutputResource* output_resource) {
  ResourcePtr input_resource;
  StringVector urls;
  ResourceContext data;
  if (encoder()->Decode(output_resource->name(), &urls, &data,
                        driver_->message_handler()) &&
      (urls.size() == 1)) {
    GoogleUrl base_gurl(output_resource->resolved_base());
    GoogleUrl resource_url(base_gurl, urls[0]);
    if (driver_->base_url().AllExceptLeaf() !=
        output_resource->resolved_base()) {
      if (driver_->MayRewriteUrl(base_gurl, resource_url)) {
        input_resource = driver_->CreateInputResource(resource_url);
      }
    } else {
      input_resource = driver_->CreateInputResource(resource_url);
    }
  }
  return input_resource;
}

const UrlSegmentEncoder* RewriteFilter::encoder() const {
  return driver_->default_encoder();
}

bool RewriteFilter::ComputeOnTheFly() const {
  return false;
}

bool RewriteFilter::HasAsyncFlow() const {
  return false;
}

RewriteContext* RewriteFilter::MakeRewriteContext() {
  return NULL;
}

}  // namespace net_instaweb
