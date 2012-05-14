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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class OutputResource;
class RewriteContext;
class RewriteDriver;
class UrlSegmentEncoder;

class RewriteFilter : public CommonFilter {
 public:
  explicit RewriteFilter(RewriteDriver* driver)
      : CommonFilter(driver) {
  }
  virtual ~RewriteFilter();

  virtual const char* id() const = 0;

  // Create an input resource by decoding output_resource using the
  // filter's. Assures legality by explicitly permission-checking the result.
  ResourcePtr CreateInputResourceFromOutputResource(
      OutputResource* output_resource);

  // All RewriteFilters define how they encode URLs and other
  // associated information needed for a rewrite into a URL.
  // The default implementation handles a single URL with
  // no extra data.  The filter owns the encoder.
  virtual const UrlSegmentEncoder* encoder() const;

  // If this method returns true, the data output of this filter will not be
  // cached, and will instead be recomputed on the fly every time it is needed.
  // (However, the transformed URL and similar metadata in CachedResult will be
  //  kept in cache).
  //
  // The default implementation returns false.
  virtual bool ComputeOnTheFly() const;

  // Generates a RewriteContext appropriate for this filter.
  // Default implementation returns NULL.  This must be overridden by
  // filters.  This is used to implement Fetch.
  virtual RewriteContext* MakeRewriteContext();

  // Generates a nested RewriteContext appropriate for this filter.
  // Default implementation returns NULL.
  // This is used to implement ajax rewriting.
  virtual RewriteContext* MakeNestedRewriteContext(
      RewriteContext* parent, const ResourceSlotPtr& slot);

 private:
  DISALLOW_COPY_AND_ASSIGN(RewriteFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_FILTER_H_
