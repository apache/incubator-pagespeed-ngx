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
// respective image tag.

#include "net/instaweb/rewriter/public/delay_images_filter.h"

#include <map>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/http/semantic_type.h"
#include "pagespeed/opt/logging/enums.pb.h"

namespace net_instaweb {

const char DelayImagesFilter::kDelayImagesSuffix[] =
    "\npagespeed.delayImagesInit();";

const char DelayImagesFilter::kDelayImagesInlineSuffix[] =
    "\npagespeed.delayImagesInlineInit();";

const char DelayImagesFilter::kImageOnloadCode[] =
    "pagespeed.switchToHighResAndMaybeBeacon(this);";

// Js snippet with the code for image elements to load the high resolution
// image once onload triggers (for the low resolution data url). This code
// also adds the checkImageForCriticality logic when the page has been
// instrumented (i.e. when pagespeed.CriticalImages is defined).
const char DelayImagesFilter::kImageOnloadJsSnippet[] =
    "window['pagespeed'] = window['pagespeed'] || {};"
    "var pagespeed = window['pagespeed'];"
    "pagespeed.switchToHighResAndMaybeBeacon = function(elem) {"
    "setTimeout(function(){elem.onload = null;"
    // Set srcset before src to avoid potentially loading 2 sizes.
    "var srcset = elem.getAttribute('data-pagespeed-high-res-srcset');"
    "if (srcset) {elem.srcset = srcset;}"
    "elem.src = elem.getAttribute('data-pagespeed-high-res-src');"
    "if (pagespeed.CriticalImages) {elem.onload = "
    "pagespeed.CriticalImages.checkImageForCriticality(elem);}"
    "}, 0);"
    "};";

DelayImagesFilter::DelayImagesFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      num_low_res_inlined_images_(0),
      insert_low_res_images_inplace_(false),
      lazyload_highres_images_(false),
      is_script_inserted_(false),
      added_image_onload_js_(false) {
}

DelayImagesFilter::~DelayImagesFilter() {}

void DelayImagesFilter::StartDocumentImpl() {
  num_low_res_inlined_images_ = 0;
  // Low res images will be placed inside the respective image tag if the user
  // agent is not a mobile, or if mobile aggressive rewriters are turned off.
  // Otherwise, the low res images are inserted at the end of the flush window.
  insert_low_res_images_inplace_ = ShouldRewriteInplace();
  lazyload_highres_images_ = driver()->options()->lazyload_highres_images() &&
      driver()->request_properties()->IsMobile();
  is_script_inserted_ = false;
  added_image_onload_js_ = false;
}

void DelayImagesFilter::MaybeAddImageOnloadJsSnippet(HtmlElement* element) {
  if (added_image_onload_js_) {
    return;
  }
  added_image_onload_js_ = true;
  HtmlElement* script = driver()->NewElement(NULL, HtmlName::kScript);
  driver()->AddAttribute(script, HtmlName::kDataPagespeedNoDefer,
                         StringPiece());
  // Always add the image-onload js before the current node, because the
  // current node might be an img node that needs the image-onload js for
  // setting its onload handler.
  driver()->InsertNodeBeforeNode(element, script);
  AddJsToElement(kImageOnloadJsSnippet, script);
}

void DelayImagesFilter::EndDocument() {
  low_res_data_map_.clear();
}

void DelayImagesFilter::EndElementImpl(HtmlElement* element) {
  if (element->keyword() == HtmlName::kBody) {
    InsertLowResImagesAndJs(element, /* insert_after_element */ false);
    InsertHighResJs(element);
  } else if (driver()->IsRewritable(element) &&
             (element->keyword() == HtmlName::kImg ||
              element->keyword() == HtmlName::kInput)) {
    // We only handle img and input tag images.  Note that delay_images.js and
    // delay_images_inline.js must be modified to handle other possible tags.
    // We should probably specifically *not* include low res images for link
    // tags of various sorts (favicons, mobile desktop icons, etc.). Use of low
    // res for explicit background images is a more interesting case, but the
    // current DOM walk in the above js files would need to be modified to
    // handle the large number of tags that we can identify in
    // resource_tag_scanner::ScanElement.
    HtmlElement::Attribute* low_res_src =
        element->FindAttribute(HtmlName::kDataPagespeedLowResSrc);
    if (low_res_src == NULL || low_res_src->DecodedValueOrNull() == NULL) {
      return;
    }
    HtmlElement::Attribute* src = element->FindAttribute(HtmlName::kSrc);
    semantic_type::Category category =
        resource_tag_scanner::CategorizeAttribute(
            element, src, driver()->options());
    if (category != semantic_type::kImage ||
        src->DecodedValueOrNull() == NULL) {
      return;  // Failed to find valid Image-valued src attribute.
    }
    ++num_low_res_inlined_images_;
    if (CanAddPagespeedOnloadToImage(*element)) {
      driver()->log_record()->SetRewriterLoggingStatus(
          RewriteOptions::FilterId(RewriteOptions::kDelayImages),
          RewriterApplication::APPLIED_OK);
      // Rename src -> data-pagespeed-high-res-src
      driver()->SetAttributeName(src, HtmlName::kDataPagespeedHighResSrc);
      // Rename srcset -> data-pagespeed-high-res-srcset
      HtmlElement::Attribute* srcset =
          element->FindAttribute(HtmlName::kSrcset);
      if (srcset != NULL) {
        driver()->SetAttributeName(
            srcset, HtmlName::kDataPagespeedHighResSrcset);
      }
      if (insert_low_res_images_inplace_) {
        // Set the src as the low resolution image.
        driver()->AddAttribute(element, HtmlName::kSrc,
                              low_res_src->DecodedValueOrNull());
        // Add an onload function to set the high resolution image after
        // deleting any existing onload handler. Since we check
        // CanAddPagespeedOnloadToImage before coming here, the only onload
        // handler that we would delete would be the one added by our very own
        // beaconing code. We re-introduce this beaconing onload logic via
        // kImageOnloadCode.
        element->DeleteAttribute(HtmlName::kOnload);
        driver()->AddAttribute(element, HtmlName::kOnload, kImageOnloadCode);
        // Add onerror handler just in case the low res image doesn't load.
        element->DeleteAttribute(HtmlName::kOnerror);
        // Note: this.onerror=null to avoid infinitely repeating on failure:
        //   See: http://stackoverflow.com/questions/3984287
        driver()->AddAttribute(element, HtmlName::kOnerror,
                               StrCat("this.onerror=null;", kImageOnloadCode));
        MaybeAddImageOnloadJsSnippet(element);
      } else {
        // Low res image data is collected in low_res_data_map_ map. This
        // low_res_src will be moved just after last low res image in the flush
        // window.
        // It is better to move inlined low resolution data later in the DOM,
        // otherwise they will block further parsing and rendering of the html
        // page.
        // Note that the high resolution images are loaded at end of body.
        const GoogleString& src_content = src->DecodedValueOrNull();
        low_res_data_map_[src_content] = low_res_src->DecodedValueOrNull();
      }
    }
    if (num_low_res_inlined_images_ == driver()->num_inline_preview_images()) {
      if (!insert_low_res_images_inplace_) {
        InsertLowResImagesAndJs(element, /* insert_after_element */ true);
      }
    }
  }
  element->DeleteAttribute(HtmlName::kDataPagespeedLowResSrc);
}

void DelayImagesFilter::InsertLowResImagesAndJs(HtmlElement* element,
                                                bool insert_after_element) {
  if (low_res_data_map_.empty()) {
    return;
  }
  GoogleString inline_script;
  HtmlElement* current_element = element;
  // Check script for changing src to low res data url is inserted once.
  if (!is_script_inserted_) {
    StaticAssetManager* manager =
        driver()->server_context()->static_asset_manager();
    inline_script = StrCat(
        manager->GetAsset(
            StaticAssetEnum::DELAY_IMAGES_INLINE_JS,
            driver()->options()),
        kDelayImagesInlineSuffix,
        manager->GetAsset(
            StaticAssetEnum::DELAY_IMAGES_JS,
            driver()->options()),
        kDelayImagesSuffix);
    HtmlElement* script_element =
        driver()->NewElement(element, HtmlName::kScript);
    driver()->AddAttribute(script_element, HtmlName::kDataPagespeedNoDefer,
                           StringPiece());
    if (insert_after_element) {
      DCHECK(element->keyword() == HtmlName::kImg ||
             element->keyword() == HtmlName::kInput);
      driver()->InsertNodeAfterNode(current_element, script_element);
      current_element = script_element;
    } else {
      DCHECK(element->keyword() == HtmlName::kBody);
      driver()->AppendChild(element, script_element);
    }
    AddJsToElement(inline_script, script_element);
    is_script_inserted_ = true;
  }

  // Generate javascript map for inline data urls where key is url and
  // base64 encoded data url as its value. This map is added to the
  // html at the end of last low res image.
  GoogleString inline_data_script;
  for (StringStringMap::iterator it = low_res_data_map_.begin();
       it != low_res_data_map_.end(); ++it) {
    inline_data_script = StrCat(
        "\npagespeed.delayImagesInline.addLowResImages('",
        it->first, "', '", it->second, "');");
    StrAppend(&inline_data_script,
              "\npagespeed.delayImagesInline.replaceWithLowRes();\n");
    HtmlElement* low_res_element =
        driver()->NewElement(current_element, HtmlName::kScript);
    driver()->AddAttribute(low_res_element, HtmlName::kDataPagespeedNoDefer,
                           StringPiece());
    if (insert_after_element) {
      driver()->InsertNodeAfterNode(current_element, low_res_element);
      current_element = low_res_element;
    } else {
      driver()->AppendChild(element, low_res_element);
    }
    AddJsToElement(inline_data_script, low_res_element);
  }
  low_res_data_map_.clear();
}

void DelayImagesFilter::InsertHighResJs(HtmlElement* body_element) {
  if (insert_low_res_images_inplace_ || !is_script_inserted_) {
    return;
  }
  GoogleString js;
  if (lazyload_highres_images_) {
    StrAppend(&js,
              "\npagespeed.delayImages.registerLazyLoadHighRes();\n");
  } else {
    StrAppend(&js,
              "\npagespeed.delayImages.replaceWithHighRes();\n");
  }
  HtmlElement* script = driver()->NewElement(body_element, HtmlName::kScript);
  driver()->AddAttribute(script, HtmlName::kDataPagespeedNoDefer,
                         StringPiece());
  driver()->AppendChild(body_element, script);
  AddJsToElement(js, script);
}

bool DelayImagesFilter::ShouldRewriteInplace() const {
  const RewriteOptions* options = driver()->options();
  return (options->use_blank_image_for_inline_preview() ||
          !(options->enable_aggressive_rewriters_for_mobile() &&
            driver()->request_properties()->IsMobile()));
}

void DelayImagesFilter::DetermineEnabled(GoogleString* disabled_reason) {
  AbstractLogRecord* log_record = driver()->log_record();
  if (!driver()->request_properties()->SupportsImageInlining()) {
    log_record->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kDelayImages),
        RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED);
    set_is_enabled(false);
    return;
  }
  CriticalImagesFinder* finder =
      driver()->server_context()->critical_images_finder();
  if (finder->Available(driver()) == CriticalImagesFinder::kNoDataYet) {
    log_record->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kDelayImages),
        RewriterHtmlApplication::PROPERTY_CACHE_MISS);
    set_is_enabled(false);
    return;
  }
  log_record->LogRewriterHtmlStatus(
      RewriteOptions::FilterId(RewriteOptions::kDelayImages),
      RewriterHtmlApplication::ACTIVE);
  set_is_enabled(true);
}

}  // namespace net_instaweb
