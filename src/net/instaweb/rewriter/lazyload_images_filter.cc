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
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/fallback_property_page.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const char kTrue[] = "true";
const char kFalse[] = "false";
const char kJquerySlider[] = "jquery.sexyslider";

}  // namespace

const char* LazyloadImagesFilter::kImageOnloadCode =
    "pagespeed.lazyLoadImages.loadIfVisible(this);";

const char* LazyloadImagesFilter::kLoadAllImages =
    "pagespeed.lazyLoadImages.loadAllImages();";

const char* LazyloadImagesFilter::kOverrideAttributeFunctions =
    "pagespeed.lazyLoadImages.overrideAttributeFunctions();";

const char* LazyloadImagesFilter::kIsLazyloadScriptInsertedPropertyName =
    "is_lazyload_script_inserted";

LazyloadImagesFilter::LazyloadImagesFilter(RewriteDriver* driver)
    : CommonFilter(driver) {
  Clear();
  blank_image_url_ = GetBlankImageSrc(
      driver->options(),
      driver->server_context()->static_asset_manager());
}
LazyloadImagesFilter::~LazyloadImagesFilter() {}

void LazyloadImagesFilter::DetermineEnabled() {
  RewriterHtmlApplication::Status should_apply = ShouldApply(driver());
  set_is_enabled(should_apply == RewriterHtmlApplication::ACTIVE);
  if (!driver()->flushing_early()) {
    driver()->log_record()->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kLazyloadImages),
        should_apply);
  }
}

void LazyloadImagesFilter::StartDocumentImpl() {
  Clear();
}

void LazyloadImagesFilter::EndDocument() {
  driver()->UpdatePropertyValueInDomCohort(
      driver()->fallback_property_page(),
      kIsLazyloadScriptInsertedPropertyName,
      main_script_inserted_ ? "1" : "0");
}

void LazyloadImagesFilter::Clear() {
  skip_rewrite_ = NULL;
  main_script_inserted_ = false;
  abort_rewrite_ = false;
  abort_script_inserted_ = false;
  num_images_lazily_loaded_ = 0;
}

RewriterHtmlApplication::Status LazyloadImagesFilter::ShouldApply(
    RewriteDriver* driver) {
  if (!driver->request_properties()->SupportsLazyloadImages()) {
    return RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED;
  }
  if (driver->flushing_early() ||
      (driver->request_headers() != NULL &&
       driver->request_headers()->IsXmlHttpRequest())) {
    return RewriterHtmlApplication::DISABLED;
  }
  return RewriterHtmlApplication::ACTIVE;
}

void LazyloadImagesFilter::StartElementImpl(HtmlElement* element) {
  if (noscript_element() != NULL) {
    return;
  }
  if (skip_rewrite_ == NULL) {
    if (element->keyword() == HtmlName::kNoembed ||
        element->keyword() == HtmlName::kMarquee) {
      skip_rewrite_ = element;
      return;
    }
    // Check if lazyloading is enabled for the given class name. If not,
    // skip rewriting all images till we reach the end of this element.
    HtmlElement::Attribute* class_attribute = element->FindAttribute(
        HtmlName::kClass);
    if (class_attribute != NULL) {
      StringPiece class_value(class_attribute->DecodedValueOrNull());
      if (!class_value.empty()) {
        GoogleString class_string;
        class_value.CopyToString(&class_string);
        LowerString(&class_string);
        if (!driver()->options()->IsLazyloadEnabledForClassName(
            class_string)) {
          skip_rewrite_ = element;
          return;
        }
      }
    }
  }
  if (element->keyword() == HtmlName::kScript) {
    // This filter does not currently work with the jquery slider. We just don't
    // rewrite the page in this case.
    HtmlElement::Attribute* src = element->FindAttribute(HtmlName::kSrc);
    if (src != NULL) {
      StringPiece url(src->DecodedValueOrNull());
      if (url.find(kJquerySlider) != StringPiece::npos) {
        abort_rewrite_ = true;
        return;
      }
    }
    InsertOverrideAttributesScript(element, true);
  }
}

void LazyloadImagesFilter::EndElementImpl(HtmlElement* element) {
  if (noscript_element() != NULL) {
    return;
  }
  if (skip_rewrite_ == element) {
    skip_rewrite_ = NULL;
    return;
  } else if (skip_rewrite_ != NULL) {
    return;
  }
  if (abort_rewrite_) {
    if (!abort_script_inserted_ && main_script_inserted_) {
      // If we have already rewritten some elements on the page, insert a
      // script to load all previously rewritten images.
      HtmlElement* script = driver()->NewElement(element, HtmlName::kScript);
      driver()->AddAttribute(script, HtmlName::kType, "text/javascript");
      HtmlNode* script_code = driver()->NewCharactersNode(
          script, kLoadAllImages);
      driver()->InsertNodeAfterNode(element, script);
      driver()->AppendChild(script, script_code);
      abort_script_inserted_ = true;
    }
    return;
  }
  if (element->keyword() == HtmlName::kBody) {
    InsertOverrideAttributesScript(element, false);
    return;
  }
  // Only rewrite <img> tags. Don't rewrite <input> tags since the onload
  // event is not fired for them in some browsers.
  if (!driver()->IsRewritable(element) ||
      element->keyword() != HtmlName::kImg) {
    return;
  }

  HtmlElement::Attribute* src = element->FindAttribute(HtmlName::kSrc);
  if (src == NULL) {
    return;
  }

  StringPiece url(src->DecodedValueOrNull());
  if (url.empty() || IsDataUrl(url) ||
      element->FindAttribute(HtmlName::kPagespeedNoDefer) != NULL) {
    // TODO(rahulbansal): Log separately for pagespeed_no_defer.
    return;
  }
  AbstractLogRecord* log_record = driver_->log_record();
  if (element->FindAttribute(HtmlName::kOnload) != NULL ||
      element->FindAttribute(HtmlName::kDataSrc) != NULL ||
      element->FindAttribute(HtmlName::kPagespeedLazySrc) != NULL) {
    log_record->LogLazyloadFilter(
        RewriteOptions::FilterId(RewriteOptions::kLazyloadImages),
        RewriterApplication::NOT_APPLIED, false, false);
    return;
  }
  // Decode the url if it is rewritten.
  GoogleUrl gurl(base_url(), url);
  StringVector decoded_url_vector;
  if (driver()->DecodeUrl(gurl, &decoded_url_vector) &&
      decoded_url_vector.size() == 1) {
    // We only handle the case where the rewritten url corresponds to
    // a single original url which should be sufficient for all cases
    // other than image sprites.
    gurl.Reset(decoded_url_vector[0]);
  }
  if (!gurl.is_valid()) {
    // Do not lazily load images with invalid urls.
    return;
  }
  StringPiece full_url = gurl.Spec();
  if (full_url.empty()) {
    return;
  }
  if (!driver()->options()->IsAllowed(full_url)) {
    // Do not lazily load images with blacklisted urls.
    log_record->LogLazyloadFilter(
        RewriteOptions::FilterId(RewriteOptions::kLazyloadImages),
        RewriterApplication::NOT_APPLIED, true, false);
    return;
  }

  CriticalImagesFinder* finder =
      driver()->server_context()->critical_images_finder();
  // Note that if the platform lacks a CriticalImageFinder
  // implementation, we consider all images to be non-critical and try
  // to lazily load them.
  if (finder->IsMeaningful(driver())) {
    // Decode the url since the critical images in the finder are not
    // rewritten.
    if (finder->IsHtmlCriticalImage(full_url.data(), driver())) {
      log_record->LogLazyloadFilter(
          RewriteOptions::FilterId(RewriteOptions::kLazyloadImages),
          RewriterApplication::NOT_APPLIED, false, true);
      // Do not try to lazily load this image since it is critical.
      return;
    }
  }
  if (!main_script_inserted_) {
    InsertLazyloadJsCode(element);
  }
  // Replace the src with pagespeed_lazy_src.
  driver()->SetAttributeName(src, HtmlName::kPagespeedLazySrc);
  driver()->AddAttribute(element, HtmlName::kSrc, blank_image_url_);
  log_record->LogLazyloadFilter(
      RewriteOptions::FilterId(RewriteOptions::kLazyloadImages),
      RewriterApplication::APPLIED_OK, false, false);
  // Set the onload appropriately.
  driver()->AddAttribute(element, HtmlName::kOnload, kImageOnloadCode);
  ++num_images_lazily_loaded_;
}

void LazyloadImagesFilter::InsertLazyloadJsCode(HtmlElement* element) {
  if (!driver()->is_lazyload_script_flushed()) {
    HtmlElement* script = driver()->NewElement(element, HtmlName::kScript);
    driver()->InsertNodeBeforeNode(element, script);
    StaticAssetManager* static_asset_manager =
        driver()->server_context()->static_asset_manager();
    GoogleString lazyload_js = GetLazyloadJsSnippet(
        driver()->options(), static_asset_manager);
    static_asset_manager->AddJsToElement(lazyload_js, script, driver());
    driver()->AddAttribute(script, HtmlName::kPagespeedNoDefer, "");
  }
  main_script_inserted_ = true;
}

void LazyloadImagesFilter::InsertOverrideAttributesScript(
    HtmlElement* element, bool is_before_script) {
  if (num_images_lazily_loaded_ > 0) {
    HtmlElement* script = driver()->NewElement(element, HtmlName::kScript);
    driver()->AddAttribute(script, HtmlName::kType, "text/javascript");
    driver()->AddAttribute(script, HtmlName::kPagespeedNoDefer, "");
    HtmlNode* script_code = driver()->NewCharactersNode(
        script, kOverrideAttributeFunctions);
    if (is_before_script) {
      driver()->InsertNodeBeforeNode(element, script);
    } else {
      driver()->AppendChild(element, script);
    }
    driver()->AppendChild(script, script_code);
    num_images_lazily_loaded_ = 0;
  }
}

GoogleString LazyloadImagesFilter::GetBlankImageSrc(
    const RewriteOptions* options,
    const StaticAssetManager* static_asset_manager) {
  const GoogleString& options_url = options->lazyload_images_blank_url();
  if (options_url.empty()) {
    return static_asset_manager->GetAssetUrl(StaticAssetManager::kBlankGif,
                                             options);
  } else {
    return options_url;
  }
}

GoogleString LazyloadImagesFilter::GetLazyloadJsSnippet(
    const RewriteOptions* options,
    StaticAssetManager* static_asset_manager) {
  const GoogleString& load_onload =
      options->lazyload_images_after_onload() ? kTrue : kFalse;
  StringPiece lazyload_images_js =
      static_asset_manager->GetAsset(
          StaticAssetManager::kLazyloadImagesJs, options);
  const GoogleString& blank_image_url =
      GetBlankImageSrc(options, static_asset_manager);
  GoogleString lazyload_js =
      StrCat(lazyload_images_js, "\npagespeed.lazyLoadInit(",
             load_onload, ", \"", blank_image_url, "\");\n");
  return lazyload_js;
}

}  // namespace net_instaweb
