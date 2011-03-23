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

// Author: abliss@google.com (Adam Bliss)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_IMG_COMBINE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_IMG_COMBINE_FILTER_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"

namespace Css {

class Declarations;
class Values;

}  // namespace Css

namespace net_instaweb {

class Statistics;

/**
 * The ImgCombineFilter combines multiple images into a single image (a process
 * called "spriting".  This reduces the total number of round-trips, and reduces
 * bytes downloaded by consolidating image headers and improving compression.
 *
 * Right now this is only used on CSS background-images, so it doesn't need to
 * be in the HTML filter chain.  In the future it will rewrite img tags as well.
 */
class ImgCombineFilter : public RewriteFilter {
 public:
  ImgCombineFilter(RewriteDriver* rewrite_driver, const char* path_prefix);
  virtual ~ImgCombineFilter();

  static void Initialize(Statistics* statistics);
  virtual const char* Name() const { return "ImgCombine"; }
  virtual bool Fetch(OutputResource* resource,
                     Writer* writer,
                     const RequestHeaders& request_header,
                     ResponseHeaders* response_headers,
                     MessageHandler* message_handler,
                     UrlAsyncFetcher::Callback* callback);
  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element) {}

  // Attempt to add the CSS background image with (resolved) url original_url to
  // this partnership.  We do not take ownership of declarations; it must live
  // until you call Realize() or Reset().  declarations is where we will add the
  // new width and height values; values[value_index] must be the URL value.
  // Will not actually change anything until you call Realize().
  bool AddCssBackground(const GoogleUrl& original_url,
                        Css::Declarations* declarations,
                        Css::Values* values,
                        int value_index,
                        MessageHandler* handler);

  // Visit all CSS background images that have been added, replacing their urls
  // with the url of the sprite, and adding CSS declarations to position them
  // correctly.  Returns true if anything was changed.
  bool DoCombine(MessageHandler* handler);

  void Reset();

 private:
  class Combiner;
  scoped_ptr<Combiner> combiner_;

  DISALLOW_COPY_AND_ASSIGN(ImgCombineFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_IMG_COMBINE_FILTER_H_
