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
// <img src="1.jpeg" pagespeed_low_res_src="data:base64...">.
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
//   <img src="1.jpeg" pagespeed_low_res_src="data:base64..." />
//  </body>
// </html>
//
// Above input html input looks like this because the image_rewrite_filter has
// already replaced <img src="1.jpeg" /> with
// <img src="1.jpeg" pagespeed_low_res_src="data:base64..." />.
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
//   <img pagespeed_high_res_src="1.jpeg" />
//   <script>
//    This block contains a map from url to their respective data urls and
//    script which put these inline_src to their respective img tags.
//   </script>
//  </body>
// </html>
//
// Bottom-of-page script actually includes the image data for the low-resolution
// images, and those are put in place before the page load completes, then 100ms
// after onload the image src gets replaced by original high resolution images
// by delay script.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_DELAY_IMAGES_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_DELAY_IMAGES_FILTER_H_

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;

class DelayImagesFilter : public EmptyHtmlFilter {
 public:
  static const char* kDelayScript;
  static const char* kInlineScript;
  explicit DelayImagesFilter(RewriteDriver* driver);
  virtual ~DelayImagesFilter();

  virtual void StartDocument();
  virtual void EndDocument();
  virtual void EndElement(HtmlElement* element);

  virtual const char* Name() const { return "DelayImages"; }

 private:
  RewriteDriver* driver_;
  ResourceTagScanner tag_scanner_;
  // pagespeed_low_res_src will be added to the low_res_data_map_ until
  // low_res_inserted is false. As soon as low_res_map_inserted_ is true, there
  // is no further addition to low_res_data_map_.
  bool low_res_map_inserted_;
  bool delay_script_inserted_;
  StringStringMap low_res_data_map_;
  GoogleString delay_images_js_;
  GoogleString delay_images_inline_js_;

  DISALLOW_COPY_AND_ASSIGN(DelayImagesFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_DELAY_IMAGES_FILTER_H_
