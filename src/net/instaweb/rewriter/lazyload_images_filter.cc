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

#include "net/instaweb/rewriter/public/lazyload_images_filter.h"

#include <cstddef>
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/image_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"

namespace net_instaweb {

extern const char* JS_lazyload_images;

// TODO(nikhilmadan): Minify this script. Also, consider outlining it.
const char* LazyloadImagesFilter::kImageLazyloadCode = JS_lazyload_images;

// base64 encoding of a blank 1x1 gif.
const char* LazyloadImagesFilter::kDefaultInlineImage = "data:image/gif;base64"
    ",R0lGODlhAQABAPAAAP///wAAACH5BAEAAAAALAAAAAABAAEAAAICRAEAOw==";

const char* LazyloadImagesFilter::kImageOnloadCode =
    "pagespeed.lazyLoadImages.loadIfVisible(this);";

LazyloadImagesFilter::LazyloadImagesFilter(RewriteDriver* driver)
    : driver_(driver),
      tag_scanner_(new ImageTagScanner(driver)),
      script_inserted_(false) {
  lazyload_js_ = StrCat(kImageLazyloadCode, "\npagespeed.lazyLoadInit();\n");
}

LazyloadImagesFilter::~LazyloadImagesFilter() {}

void LazyloadImagesFilter::EndElement(HtmlElement* element) {
  if (!script_inserted_ && element->keyword() == HtmlName::kHead) {
    // Insert the inlined script at the end of the document head.
    HtmlElement* script = driver_->NewElement(element, HtmlName::kScript);
    driver_->AddAttribute(script, HtmlName::kType, "text/javascript");
    HtmlNode* script_code = driver_->NewCharactersNode(
        script, lazyload_js_);
    driver_->InsertElementBeforeCurrent(script);
    driver_->AppendChild(script, script_code);
    script_inserted_ = true;
  } else if (script_inserted_ && driver_->IsRewritable(element)) {
    HtmlElement::Attribute* src = tag_scanner_->ParseImageElement(element);
    if (src != NULL &&
        element->FindAttribute(HtmlName::kOnload) == NULL &&
        element->FindAttribute(HtmlName::kPagespeedLazySrc) == NULL) {
      // Check that the image has an src, and does not have an onload and
      // pagespeed_lazy_src attribute. If so, replace the src with
      // pagespeed_lazy_src and set the onload appropriately.
      driver_->AddAttribute(element, HtmlName::kPagespeedLazySrc, src->value());
      driver_->AddAttribute(element, HtmlName::kOnload, kImageOnloadCode);
      driver_->AddAttribute(element, HtmlName::kSrc, kDefaultInlineImage);
      element->DeleteAttribute(HtmlName::kSrc);
    }
    // TODO(nikhilmadan): Do nothing if the src is a base64 encoding.
  }
}

}  // namespace net_instaweb
