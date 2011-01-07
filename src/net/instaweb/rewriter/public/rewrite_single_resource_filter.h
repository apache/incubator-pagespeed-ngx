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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_SINGLE_RESOURCE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_SINGLE_RESOURCE_FILTER_H_

#include "base/basictypes.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"

namespace net_instaweb {

// Simpler interface for RewriteFilters which only convert one input resource
// to one output resource.
//
// To derive from this class, implement RewriteLoadedResource.
class RewriteSingleResourceFilter : public RewriteFilter {
 public:
  explicit RewriteSingleResourceFilter(
      RewriteDriver* driver, StringPiece filter_prefix)
      : RewriteFilter(driver, filter_prefix),
        resource_manager_(driver->resource_manager()) {
  }
  virtual ~RewriteSingleResourceFilter();

  virtual bool Fetch(OutputResource* output_resource,
                     Writer* response_writer,
                     const RequestHeaders& request_header,
                     ResponseHeaders* response_headers,
                     MessageHandler* message_handler,
                     UrlAsyncFetcher::Callback* callback);

 protected:
  // Derived classes must implement this function instead of Fetch.
  virtual bool RewriteLoadedResource(const Resource* input_resource,
                                     OutputResource* output_resource) = 0;

 private:
  class FetchCallback;

  ResourceManager* resource_manager_;

  DISALLOW_COPY_AND_ASSIGN(RewriteSingleResourceFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_SINGLE_RESOURCE_FILTER_H_
