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
#include "net/instaweb/rewriter/public/output_resource.h"

namespace net_instaweb {

// A helper base class for RewriteFilters which only convert one input resource
// to one output resource. This class helps implement both HTML rewriting
// and Fetch in terms of a single RewriteLoadedResource method, and takes
// care of resource management and caching.
//
// Subclasses should implement RewriteLoadedResource and call
// Rewrite*WithCaching when rewriting HTML using the returned
// CachedResult (which may be NULL) to get rewrite results.
class RewriteSingleResourceFilter : public RewriteFilter {
 public:
  explicit RewriteSingleResourceFilter(
      RewriteDriver* driver, StringPiece filter_prefix)
      : RewriteFilter(driver, filter_prefix) {
  }
  virtual ~RewriteSingleResourceFilter();

  virtual bool Fetch(OutputResource* output_resource,
                     Writer* response_writer,
                     const RequestHeaders& request_header,
                     ResponseHeaders* response_headers,
                     MessageHandler* message_handler,
                     UrlAsyncFetcher::Callback* callback);

 protected:
  // Rewrite the given resource using this filter's RewriteLoadedResource,
  // taking  advantage of various caching techniques to avoid recomputation
  // whenever possible.
  //
  // If your filter code and the original URL are enough to produce your output
  // pass in resource_manager_->url_escaper() into encoder. If not, pass in
  // an encoder that incorporates any other settings into the output URL.
  //
  // If nothing can be done (as the input data hasn't been fetched in time
  // and we do not have cached output) returns NULL. Otherwise returns
  // a CachedResult stating whether the resource is optimizable, and if so at
  // what URL the output is, along with any metadata that was stored when
  // examining it.
  //
  // Note: the metadata may be useful even when optimizable() is false.
  // For example a filter could store dimensions of an image in them, even
  // if it chose to not change it, so any <img> tags can be given appropriate
  // width and height.
  //
  // Precondition: in != NULL, in is security-checked
  OutputResource::CachedResult* RewriteResourceWithCaching(
      Resource* in, UrlSegmentEncoder* encoder);

  // Variant of the above that makes and cleans up input resource for in_url.
  // Note that the URL will be expanded and security checked with respect to the
  // current base URL for the HTML parser.
  OutputResource::CachedResult* RewriteWithCaching(const StringPiece& in_url,
                                                   UrlSegmentEncoder* encoder);

  // Derived classes must implement this function instead of Fetch.
  //
  // If rewrite succeeds, make sure to call ResourceManager::Write and
  // set the content-type on the output resource.
  //
  // If rewrite fails, simply return false.
  virtual bool RewriteLoadedResource(const Resource* input_resource,
                                     OutputResource* output_resource) = 0;

 private:
  class FetchCallback;
  friend class RewriteSingleResourceFilterTest;

  // Metadata key we use to store the input timestamp.
  static const char kInputTimestampKey[];

  // Tries to rewrite input_resource to output_resource, and if successful
  // updates the cache as appropriate. Does not call WriteUnoptimizable on
  // failure.
  bool RewriteLoadedResourceAndCacheIfOk(const Resource* input_resource,
                                         OutputResource* output_resource);

  // Records that rewrite of input -> output failed (either due to
  // unavailability of input or failed conversion).
  void CacheRewriteFailure(const Resource* input_resource,
                           OutputResource* output_resource,
                           MessageHandler* message_handler);

  DISALLOW_COPY_AND_ASSIGN(RewriteSingleResourceFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_SINGLE_RESOURCE_FILTER_H_
