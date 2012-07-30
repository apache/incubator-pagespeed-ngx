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

// Author: nikhilmadan@google.com (Nikhil Madan)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_LAZYLOAD_IMAGES_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_LAZYLOAD_IMAGES_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;
class Statistics;

// Filter to lazyload images by replacing the src with a pagespeed_lazy_src
// attribute and injecting a javascript to detect which images are in the
// user's viewport and swapping the src back.
//
// This filter only works if the document has a head. It adds some javascript to
// the head that determines if an image is visible and adds a listener to the
// window scroll event. If an image is visible, it replaces the src and the
// pagespeed_lazy_src attributes.
//
// In order to immediately load images that are above the fold, we attach an
// onload event to each image. This onload event determines if the image is
// visible and immediately replaces the src with the pagespeed_lazy_src.
// Otherwise, the image is added to the deferred queue. Since the onload event
// is only fired if the image src is valid, we add a fixed inlined image to
// each image node we are deferring.
//
// When the user scrolls, we scan through the deferred queue and determine which
// images are now visible, and switch the src and pagespeed_lazy_src.
//
// Given the following input html:
// <html>
//  <head>
//  </head>
//  <body>
//   <img src="1.jpeg" />
//  </body>
// </html>
//
// The output will be
// <html>
//  <head>
//   <script>
//    Javascript that determines which images are visible and attaches a
//    window.scroll event.
//   </script>
//  </head>
//  <body>
//   <img pagespeed_lazy_src="1.jpeg" onload="kImageOnloadCode"
//    src="kBlankImageSrc" />
//  </body>
//
class LazyloadImagesFilter : public CommonFilter {
 public:
  static const char* kImageLazyloadCode;
  static const char* kBlankImageSrc;
  static const char* kImageOnloadCode;
  static const char* kLoadAllImages;

  explicit LazyloadImagesFilter(RewriteDriver* driver);
  virtual ~LazyloadImagesFilter();

  virtual const char* Name() const { return "Lazyload Images"; }

  static void Initialize(Statistics* statistics);
  static void Terminate();

 protected:
  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);

 private:
  // Clears all state associated with the filter.
  void Clear();

  // Inserts the lazyload JS code before the given element.
  void InsertLazyloadJsCode(HtmlElement* element);

  // The initial image url to be used.
  GoogleString blank_image_url_;
  // If non-NULL, we skip rewriting till we reach
  // LazyloadImagesFilter::EndElement(skip_rewrite_).
  HtmlElement* skip_rewrite_;
  // Indicates if the main javascript has been inserted into the page.
  bool main_script_inserted_;
  // Indicates whether we should abort rewriting the page.
  bool abort_rewrite_;
  // Indicates if the javascript to abort the rewrite has been inserted into the
  // page.
  bool abort_script_inserted_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_LAZYLOAD_IMAGES_FILTER_H_
