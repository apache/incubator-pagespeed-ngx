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

// Author: pulkitg@google.com (Pulkit Goyal)
//
// Contains implementation of DelayImagesFilter, which delays all the high
// quality images whose low quality inlined data url are available within their
// respective image tag like
// <img src="1.jpeg" data-pagespeed-low-res-src="data:base64...">.
//
// This filter extracts such low res data urls and generates a map from them.
// This map will be embedded inside HTML at the end of body tag with a script
// whose function is to put low res src into respective image tag. Another
// script which replaces low quality images with high quality images is also
// embedded.
//
// This filter will work in conjunction with image_rewrite_filter which
// generates data url for low quality images and embeds them with their
// respective img tags.
//
// To avoid drastic reflows, we also need to switch on insert_image_dimensions.
//
// Html input to this filter looks like:
// <html>
//  <head>
//  </head>
//  <body>
//   <img src="1.jpeg" data-pagespeed-low-res-src="data:base64..." />
//  </body>
// </html>
//
// Above input html input looks like this because the image_rewrite_filter has
// already replaced <img src="1.jpeg" /> with
// <img src="1.jpeg" data-pagespeed-low-res-src="data:base64..." />.
//
// Output for the above html will be:
// <html>
//  <head>
//   <script>
//    Script code registers an onload event handler which replaces low res
//    images with high res images.
//   </script>
//  </head>
//  <body>
//   <img data-pagespeed-high-res-src="1.jpeg" />
//   <script>
//    This block contains a map from url to their respective data urls and
//    script which put these inline_src to their respective img tags.
//   </script>
//  </body>
// </html>
//
// Bottom-of-page script actually includes the image data for the low-resolution
// images, and those are put in place as soon as control reaches there. High
// quality images are downloaded after all the low quality images are placed
// by delay script.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DELAY_IMAGES_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DELAY_IMAGES_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"

namespace net_instaweb {

class Statistics;

class DelayImagesFilter : public CommonFilter {
 public:
  static const char kDelayImagesSuffix[];
  static const char kDelayImagesInlineSuffix[];
  static const char kImageOnloadCode[];
  static const char kImageOnloadJsSnippet[];

  explicit DelayImagesFilter(RewriteDriver* driver);
  virtual ~DelayImagesFilter();

  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element) { }
  virtual void EndElementImpl(HtmlElement* element);

  virtual void EndDocument();

  virtual const char* Name() const { return "DelayImages"; }

  virtual void DetermineEnabled(GoogleString* disabled_reason);

  static void InitStats(Statistics* statistics);
  static void Terminate();
  ScriptUsage GetScriptUsage() const override { return kWillInjectScripts; }

 private:
  // Add the js snippet to be used for image-onload if it has not already been
  // added.
  void MaybeAddImageOnloadJsSnippet(HtmlElement* element);

  // Insert low resolution images, and the script needed to load them if any.
  void InsertLowResImagesAndJs(HtmlElement* element, bool insert_after_element);

  // Insert the script to load the high resolution images.
  void InsertHighResJs(HtmlElement* element);

  // Returns a boolean indicating whether we should insert low resolution images
  // in place.
  bool ShouldRewriteInplace() const;

  int num_low_res_inlined_images_;
  StringStringMap low_res_data_map_;

  // Replace the image url with low res base64 encoded url inplace if it is
  // true, else low_res_data_map_ containing low res images is inserted at the
  // end of body tag.
  bool insert_low_res_images_inplace_;

  // lazyload_highres_images_ is set to true if lazyload_highres flag is true.
  // It enables the feature that lazily loads the high res images after their
  // low res versions are rendered. This flag is used especially in the case
  // of mobile.
  bool lazyload_highres_images_;

  bool is_script_inserted_;

  bool added_image_onload_js_;

  DISALLOW_COPY_AND_ASSIGN(DelayImagesFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DELAY_IMAGES_FILTER_H_
