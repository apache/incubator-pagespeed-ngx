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

#include "base/logging.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/public/fallback_property_page.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/data_url.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/opt/logging/enums.pb.h"

namespace net_instaweb {

namespace {

const char kTrue[] = "true";
const char kFalse[] = "false";
const char kJquerySlider[] = "jquery.sexyslider";

}  // namespace

const char* LazyloadImagesFilter::kImageOnloadCode =
    "pagespeed.lazyLoadImages.loadIfVisibleAndMaybeBeacon(this);";

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

void LazyloadImagesFilter::DetermineEnabled(GoogleString* disabled_reason) {
  RewriterHtmlApplication::Status should_apply = ShouldApply(driver());
  set_is_enabled(should_apply == RewriterHtmlApplication::ACTIVE);
  driver()->log_record()->LogRewriterHtmlStatus(
      RewriteOptions::FilterId(RewriteOptions::kLazyloadImages),
      should_apply);
}

void LazyloadImagesFilter::StartDocumentImpl() {
  Clear();
}

void LazyloadImagesFilter::EndDocument() {
  // TODO(jmaessen): Fix filter to insert this script
  // conditionally.
  driver()->UpdatePropertyValueInDomCohort(
      driver()->fallback_property_page(),
      kIsLazyloadScriptInsertedPropertyName,
      main_script_inserted_ ? "1" : "0");
}

void LazyloadImagesFilter::Clear() {
  skip_rewrite_ = NULL;
  head_element_ = NULL;
  main_script_inserted_ = false;
  abort_rewrite_ = false;
  abort_script_inserted_ = false;
  num_images_lazily_loaded_ = 0;
}

RewriterHtmlApplication::Status LazyloadImagesFilter::ShouldApply(
    RewriteDriver* driver) {
  // Note: there's similar UA logic in
  // DedupInlinedImagedFilter::DetermineEnabled, so if this logic changes that
  // logic may well require alteration too.
  if (!driver->request_properties()->SupportsLazyloadImages()) {
    return RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED;
  }
  if (driver->request_headers() != nullptr &&
      driver->request_headers()->IsXmlHttpRequest()) {
    return RewriterHtmlApplication::DISABLED;
  }
  CriticalImagesFinder* finder =
      driver->server_context()->critical_images_finder();
  if (finder->Available(driver) == CriticalImagesFinder::kNoDataYet) {
    // Don't lazyload images on a page that's waiting for critical image data.
    // However, this page should later be rewritten when data arrives.  Contrast
    // this with the case where beaconing is explicitly disabled, and all images
    // are lazy loaded.
    return RewriterHtmlApplication::DISABLED;
  }
  return RewriterHtmlApplication::ACTIVE;
}

void LazyloadImagesFilter::StartElementImpl(HtmlElement* element) {
  if (noscript_element() != NULL) {
    return;
  }
  if (!main_script_inserted_ && head_element_ == NULL) {
    switch (element->keyword()) {
      case HtmlName::kHtml:
      case HtmlName::kLink:
      case HtmlName::kMeta:
      case HtmlName::kScript:
      case HtmlName::kStyle:
        break;
      case HtmlName::kHead:
        head_element_ = element;
        break;
      default:
        InsertLazyloadJsCode(element);
        break;
    }
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
  if (noscript_element() != NULL || skip_rewrite_ != NULL) {
    if (skip_rewrite_ == element) {
      skip_rewrite_ = NULL;
    }
    return;
  }
  if (head_element_ == element) {
    InsertLazyloadJsCode(NULL);
    head_element_ = NULL;
  }
  if (abort_rewrite_) {
    if (!abort_script_inserted_ && main_script_inserted_ &&
        num_images_lazily_loaded_ > 0) {
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
      element->FindAttribute(HtmlName::kDataPagespeedNoDefer) != NULL ||
      element->FindAttribute(HtmlName::kPagespeedNoDefer) != NULL) {
    // TODO(rahulbansal): Log separately for pagespeed_no_defer.
    return;
  }
  AbstractLogRecord* log_record = driver()->log_record();
  if (!CanAddPagespeedOnloadToImage(*element) ||
      element->FindAttribute(HtmlName::kDataPagespeedLazySrc) != NULL ||
      element->FindAttribute(HtmlName::kDataSrc) != NULL) {
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
  if (!gurl.IsAnyValid()) {
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
  // Note that if the platform lacks a CriticalImageFinder implementation, we
  // consider all images to be non-critical and try to lazily load them.
  // Similarly, if we have disabled data gathering for lazy load, we again lazy
  // load all images.  If, however, we simply haven't gathered enough data yet,
  // we consider all images to be critical and disable lazy loading (in
  // ShouldApply above) in order to provide better above-the-fold loading.
  if (finder->Available(driver()) == CriticalImagesFinder::kAvailable) {
    // Decode the url since the critical images in the finder are not
    // rewritten.
    if (finder->IsHtmlCriticalImage(full_url, driver())) {
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
  // Replace the src with data-pagespeed-lazy-src.
  driver()->SetAttributeName(src, HtmlName::kDataPagespeedLazySrc);
  // Rename srcset -> data-pagespeed-high-res-srcset
  HtmlElement::Attribute* srcset =
      element->FindAttribute(HtmlName::kSrcset);
  if (srcset != NULL) {
    driver()->SetAttributeName(srcset, HtmlName::kDataPagespeedLazySrcset);
  }
  driver()->AddAttribute(element, HtmlName::kSrc, blank_image_url_);
  log_record->LogLazyloadFilter(
      RewriteOptions::FilterId(RewriteOptions::kLazyloadImages),
      RewriterApplication::APPLIED_OK, false, false);
  // Add an onload function to load the image if it is visible and then do
  // the criticality check. Since we check CanAddPagespeedOnloadToImage
  // before coming here, the only onload handler that we would delete would
  // be the one added by our very own beaconing code. We re-introduce this
  // beaconing onload logic via kImageOnloadCode.
  // TODO(jud): Add these with addEventListener rather than with the attributes.
  element->DeleteAttribute(HtmlName::kOnload);
  driver()->AddAttribute(element, HtmlName::kOnload, kImageOnloadCode);
  // Add onerror handler just in case the temporary pixel doesn't load.
  element->DeleteAttribute(HtmlName::kOnerror);
  // Note: this.onerror=null to avoid infinitely repeating on failure:
  //   See: http://stackoverflow.com/questions/3984287
  driver()->AddAttribute(element, HtmlName::kOnerror,
                         StrCat("this.onerror=null;", kImageOnloadCode));
  ++num_images_lazily_loaded_;
}

void LazyloadImagesFilter::InsertLazyloadJsCode(HtmlElement* element) {
  if (!driver()->is_lazyload_script_flushed() &&
      (!abort_rewrite_ || num_images_lazily_loaded_ > 0)) {
    HtmlElement* script = driver()->NewElement(element, HtmlName::kScript);
    if (element != NULL) {
      driver()->InsertNodeBeforeNode(element, script);
    } else if (driver()->CanAppendChild(head_element_)) {
      // insert at end of head.
      driver()->AppendChild(head_element_, script);
    } else {
      // Could not insert at end of head even though we just saw the end of head
      // event!  Should not happen, but this will ensure that we insert the
      // script before the next tag we see.
      LOG(DFATAL) << "Can't append child to <head> at the </head> event!";
      main_script_inserted_ = false;
      return;
    }
    StaticAssetManager* static_asset_manager =
        driver()->server_context()->static_asset_manager();
    GoogleString lazyload_js = GetLazyloadJsSnippet(
        driver()->options(), static_asset_manager);
    AddJsToElement(lazyload_js, script);
    driver()->AddAttribute(script, HtmlName::kDataPagespeedNoDefer,
                           StringPiece());
  }
  main_script_inserted_ = true;
}

void LazyloadImagesFilter::InsertOverrideAttributesScript(
    HtmlElement* element, bool is_before_script) {
  if (num_images_lazily_loaded_ > 0) {
    HtmlElement* script = driver()->NewElement(element, HtmlName::kScript);
    driver()->AddAttribute(script, HtmlName::kType, "text/javascript");
    driver()->AddAttribute(script, HtmlName::kDataPagespeedNoDefer,
                           StringPiece());
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
    return static_asset_manager->GetAssetUrl(StaticAssetEnum::BLANK_GIF,
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
          StaticAssetEnum::LAZYLOAD_IMAGES_JS, options);
  const GoogleString& blank_image_url =
      GetBlankImageSrc(options, static_asset_manager);
  GoogleString lazyload_js =
      StrCat(lazyload_images_js, "\npagespeed.lazyLoadInit(",
             load_onload, ", \"", blank_image_url, "\");\n");
  return lazyload_js;
}

}  // namespace net_instaweb
