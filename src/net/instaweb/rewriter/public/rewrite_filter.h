/**
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

#include "base/basictypes.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_async_fetcher.h"

namespace net_instaweb {

class HtmlParse;
class OutputResource;
class ResourceManager;
class RewriteDriver;
class UrlAsyncFetcher;
class Writer;

class RewriteFilter : public CommonFilter {
 public:
  explicit RewriteFilter(RewriteDriver* driver, StringPiece filter_prefix)
      : CommonFilter(driver),
        filter_prefix_(filter_prefix.data(), filter_prefix.size()),
        driver_(driver) {
  }
  virtual ~RewriteFilter();

  // Fetches a resource written using the filter.  Filters that
  // encode all the data (URLs, meta-data) needed to reconstruct
  // a rewritten resource in a URL component, this method is the
  // mechanism for the filter to serve the rewritten resource.
  //
  // The flow is that a RewriteFilter is instantiated with
  // a path prefix, e.g. a two letter abbreviation of the
  // filter, like "ce" for CacheExtender.  When it rewrites a
  // resource, it replaces the href with a url constructed as
  //   HOST://PATH/ENCODED_NAME.pagespeed.FILTER_ID.HASH.EXT
  // Most ENCODED_NAMEs are just the original URL with a few
  // characters, notably '?' and '&' esacped.  For "ic" (ImgRewriterFilter)
  // the encoding includes the original image URL, plus the pixel-dimensions
  // to which the image was resized.  For combine_css it includes
  // all the original URLs separated by '+'.
  virtual bool Fetch(OutputResource* output_resource,
                     Writer* response_writer,
                     const RequestHeaders& request_header,
                     ResponseHeaders* response_headers,
                     MessageHandler* message_handler,
                     UrlAsyncFetcher::Callback* callback) = 0;

  const std::string& id() const { return filter_prefix_; }
  HtmlParse* html_parse() { return driver_->html_parse(); }
  ResourceManager* resource_manager() { return driver_->resource_manager(); }

 protected:
  std::string filter_prefix_;  // Prefix that should be used in front of all
                                // rewritten URLs
  RewriteDriver* driver_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RewriteFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_FILTER_H_
