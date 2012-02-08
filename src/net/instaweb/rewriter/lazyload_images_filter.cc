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

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/image_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"

namespace net_instaweb {

extern const char* JS_lazyload_images;  // Not optimized.
extern const char* JS_lazyload_images_opt;

namespace {

const char kTrue[] = "true";
const char kFalse[] = "false";
const char kData[] = "data:";

}  // namespace


GoogleString* LazyloadImagesFilter::opt_lazyload_images_js_ = NULL;
GoogleString* LazyloadImagesFilter::debug_lazyload_images_js_ = NULL;

// base64 encoding of a blank 1x1 gif.
const char* LazyloadImagesFilter::kDefaultInlineImage = "data:image/gif;base64"
    ",R0lGODlhAQABAPAAAP///wAAACH5BAEAAAAALAAAAAABAAEAAAICRAEAOw==";

const char* LazyloadImagesFilter::kImageOnloadCode =
    "pagespeed.lazyLoadImages.loadIfVisible(this);";

void LazyloadImagesFilter::Initialize(Statistics* statistics) {
  if (opt_lazyload_images_js_ == NULL) {
    opt_lazyload_images_js_ = new GoogleString(JS_lazyload_images_opt);
  }
  if (debug_lazyload_images_js_ == NULL) {
    debug_lazyload_images_js_ = new GoogleString(JS_lazyload_images);
  }
}

void  LazyloadImagesFilter::Terminate() {
  delete opt_lazyload_images_js_;
  delete debug_lazyload_images_js_;
  opt_lazyload_images_js_ = NULL;
  debug_lazyload_images_js_ = NULL;
}

LazyloadImagesFilter::LazyloadImagesFilter(RewriteDriver* driver)
    : driver_(driver),
      tag_scanner_(new ImageTagScanner(driver)),
      script_inserted_(false),
      debug_(driver->options()->Enabled(RewriteOptions::kDebug)) {
}

LazyloadImagesFilter::~LazyloadImagesFilter() {}

void LazyloadImagesFilter::StartDocument() {
  script_inserted_ = false;
}

void LazyloadImagesFilter::EndElement(HtmlElement* element) {
  if (!script_inserted_ && element->keyword() == HtmlName::kHead) {
    // Insert the inlined script at the end of the document head.
    HtmlElement* script = driver_->NewElement(element, HtmlName::kScript);
    driver_->AddAttribute(script, HtmlName::kType, "text/javascript");
    const GoogleString& load_onload =
        driver_->options()->lazyload_images_after_onload() ? kTrue : kFalse;
    const GoogleString& lazyload_js =
        debug_ ? StrCat(*debug_lazyload_images_js_,
                        "\npagespeed.lazyLoadInit(",
                        load_onload, ");\n")
                        : StrCat(*opt_lazyload_images_js_,
                                 "\npagespeed.lazyLoadInit(",
                                 load_onload, ");\n");
    HtmlNode* script_code = driver_->NewCharactersNode(
        script, lazyload_js);
    driver_->InsertElementBeforeCurrent(script);
    driver_->AppendChild(script, script_code);
    script_inserted_ = true;
  } else if (script_inserted_ && driver_->IsRewritable(element)) {
    HtmlElement::Attribute* src = tag_scanner_->ParseImageElement(element);
    if (src != NULL) {
      StringPiece url(src->value());
      if (!url.starts_with(kData) &&
          element->FindAttribute(HtmlName::kOnload) == NULL &&
          element->FindAttribute(HtmlName::kPagespeedLazySrc) == NULL &&
          element->FindAttribute(HtmlName::kPagespeedLazySrc) == NULL) {
        // Check that the image has a src, does not have an onload and
        // pagespeed_lazy_src attribute and is not inlined. If so, replace the
        // src with pagespeed_lazy_src and set the onload appropriately.
        driver_->AddAttribute(element, HtmlName::kPagespeedLazySrc, url);
        driver_->AddAttribute(element, HtmlName::kOnload, kImageOnloadCode);
        driver_->AddAttribute(element, HtmlName::kSrc, kDefaultInlineImage);
        element->DeleteAttribute(HtmlName::kSrc);
      }
    }
  }
}

}  // namespace net_instaweb
