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
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const char kTrue[] = "true";
const char kFalse[] = "false";
const char kData[] = "data:";
const char kJquerySlider[] = "jquery.sexyslider";
const char kMetabox[] = "dfcg-metabox";

}  // namespace

// base64 encoding of a blank 1x1 gif with GIF comment "PSA".
const char* LazyloadImagesFilter::kBlankImageSrc = "data:image/gif;base64,"
    "R0lGODlhAQABAIAAAP///////yH+A1BTQQAsAAAAAAEAAQAAAgJEAQA7";

const char* LazyloadImagesFilter::kImageOnloadCode =
    "pagespeed.lazyLoadImages.loadIfVisible(this);";

const char* LazyloadImagesFilter::kLoadAllImages =
    "pagespeed.lazyLoadImages.loadAllImages();";

LazyloadImagesFilter::LazyloadImagesFilter(RewriteDriver* driver)
    : driver_(driver),
      main_script_inserted_(false),
      abort_rewrite_(false),
      abort_script_inserted_(false) {}

LazyloadImagesFilter::~LazyloadImagesFilter() {}

void LazyloadImagesFilter::StartDocument() {
  main_script_inserted_ = false;
  abort_rewrite_ = false;
  abort_script_inserted_ = false;
}

void LazyloadImagesFilter::StartElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kScript) {
    // This filter does not currently work with the jquery slider. We just don't
    // rewrite the page in this case.
    HtmlElement::Attribute* src = element->FindAttribute(HtmlName::kSrc);
    if (src != NULL) {
      StringPiece url(src->DecodedValueOrNull());
      if (url.find(kJquerySlider) != StringPiece::npos) {
        abort_rewrite_ = true;
      }
    }
  }
}

void LazyloadImagesFilter::EndElement(HtmlElement* element) {
  if (abort_rewrite_) {
    if (!abort_script_inserted_ && main_script_inserted_) {
      // If we have already rewritten some elements on the page, insert a
      // script to load all previously rewritten images.
      HtmlElement* script = driver_->NewElement(element, HtmlName::kScript);
      driver_->AddAttribute(script, HtmlName::kType, "text/javascript");
      HtmlNode* script_code = driver_->NewCharactersNode(
          script, kLoadAllImages);
      driver_->InsertElementAfterElement(element, script);
      driver_->AppendChild(script, script_code);
      abort_script_inserted_ = true;
    }
    return;
  }
  if (!main_script_inserted_ && element->keyword() == HtmlName::kHead) {
    // Insert the inlined script at the end of the document head.
    HtmlElement* script = driver_->NewElement(element, HtmlName::kScript);
    driver_->AddAttribute(script, HtmlName::kType, "text/javascript");
    const GoogleString& load_onload =
        driver_->options()->lazyload_images_after_onload() ? kTrue : kFalse;
    StaticJavascriptManager* static_js__manager =
        driver_->resource_manager()->static_javascript_manager();
    StringPiece lazyload_images_js =
        static_js__manager->GetJsSnippet(
            StaticJavascriptManager::kLazyloadImagesJs, driver_->options());
    const GoogleString& lazyload_js =
        StrCat(lazyload_images_js, "\npagespeed.lazyLoadInit(",
               load_onload, ", \"", kBlankImageSrc, "\");\n");
    HtmlNode* script_code = driver_->NewCharactersNode(
        script, lazyload_js);
    driver_->InsertElementBeforeCurrent(script);
    driver_->AppendChild(script, script_code);
    main_script_inserted_ = true;
  } else if (main_script_inserted_ && driver_->IsRewritable(element) &&
             element->keyword() == HtmlName::kImg) {
    // Don't rewrite images with class="dfcg-metabox" since they are sliding
    // animations.
    HtmlElement::Attribute* class_attribute = element->FindAttribute(
        HtmlName::kClass);
    if (class_attribute != NULL) {
      StringPiece class_value(class_attribute->DecodedValueOrNull());
      if (class_value.find(kMetabox) != StringPiece::npos) {
        return;
      }
    }
    // Only rewrite <img> tags. Don't rewrite <input> tags since the onload
    // event is not fired for them in some browsers.
    HtmlElement::Attribute* src = element->FindAttribute(HtmlName::kSrc);
    if (src != NULL) {
      StringPiece url(src->DecodedValueOrNull());
      if (!url.empty() && !url.starts_with(kData) &&
          element->FindAttribute(HtmlName::kOnload) == NULL &&
          element->FindAttribute(HtmlName::kPagespeedLazySrc) == NULL &&
          !element->DeleteAttribute(HtmlName::kPagespeedNoDefer)) {
        // Lazily load the image if it has a src, does not have an onload /
        // pagespeed_lazy_src attribute / pagespeed_no_defer attribute and is
        // not inlined.
        // Note that we remove the pagespeed_no_defer if it was present.
        CriticalImagesFinder* finder =
            driver_->resource_manager()->critical_images_finder();
        // Note that if the platform lacks a CriticalImageFinder
        // implementation, we consider all images to be non-critical and try
        // to lazily load them.
        if (finder != NULL) {
          // Decode the url since the critical images in the finder are not
          // rewritten.
          GoogleUrl gurl(driver_->base_url(), url);
          StringVector decoded_url_vector;
          if (driver_->DecodeUrl(gurl, &decoded_url_vector) &&
              decoded_url_vector.size() == 1) {
            // We only handle the case where the rewritten url corresponds to a
            // single original url which should be sufficient for all cases
            // other than image sprites.
            gurl.Reset(decoded_url_vector[0]);
          }
          if (finder->IsCriticalImage(gurl.spec_c_str(), driver_)) {
            // Do not try to lazily load this image since it is critical.
            return;
          }
        }
        // Replace the src with pagespeed_lazy_src and set the onload
        // appropriately.
        driver_->SetAttributeName(src, HtmlName::kPagespeedLazySrc);
        driver_->AddAttribute(element, HtmlName::kSrc, kBlankImageSrc);
        driver_->AddAttribute(element, HtmlName::kOnload, kImageOnloadCode);
      }
    }
  }
}

}  // namespace net_instaweb
