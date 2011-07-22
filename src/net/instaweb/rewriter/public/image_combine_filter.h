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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_COMBINE_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_COMBINE_FILTER_H_

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/util/public/basictypes.h"

namespace Css {

class Declarations;
class Values;

}  // namespace Css

namespace net_instaweb {

class GoogleUrl;
class HtmlElement;
class MessageHandler;
class RequestHeaders;
class ResponseHeaders;
class RewriteContext;
class RewriteDriver;
class Statistics;
class Writer;

/*
 * The ImageCombineFilter combines multiple images into a single image (a process
 * called "spriting".  This reduces the total number of round-trips, and reduces
 * bytes downloaded by consolidating image headers and improving compression.
 *
 * Right now this is only used on CSS background-images, so it doesn't need to
 * be in the HTML filter chain.  In the future it will rewrite img tags as well.
 */
class ImageCombineFilter : public RewriteFilter {
 public:
  ImageCombineFilter(RewriteDriver* rewrite_driver, const char* path_prefix);
  virtual ~ImageCombineFilter();

  static void Initialize(Statistics* statistics);
  virtual const char* Name() const { return "ImageCombine"; }
  virtual bool Fetch(const OutputResourcePtr& resource,
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
  // new width and height values; url_value must point to the URL value to be
  // replaced. Will not actually change anything until you call Realize().
  // This will succeed even in cases when it turns out (later) the image
  // can not be sprited.
  bool AddCssBackgroundContext(const GoogleUrl& original_url,
                               Css::Values* values,
                               int value_index,
                               CssFilter::Context* parent,
                               Css::Declarations* decls,
                               MessageHandler* handler);

  // Create the combination with the current combiner.
  OutputResourcePtr MakeOutput();

  void Reset();
  void Reset(CssFilter::Context* context);
  virtual RewriteContext* MakeRewriteContext();
  virtual bool HasAsyncFlow() const;

 private:
  class Combiner;
  class Context;

  Context* context_;
  Context* MakeContext();
  Context* MakeNestedContext(RewriteContext* parent);
  bool GetDeclarationDimensions(Css::Declarations* declarations,
                                int* width, int* height);

  DISALLOW_COPY_AND_ASSIGN(ImageCombineFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_IMAGE_COMBINE_FILTER_H_
