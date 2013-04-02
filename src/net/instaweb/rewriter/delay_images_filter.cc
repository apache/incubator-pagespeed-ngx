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

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/http/public/device_properties.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

const char DelayImagesFilter::kDelayImagesSuffix[] =
    "\npagespeed.delayImagesInit();";

const char DelayImagesFilter::kDelayImagesInlineSuffix[] =
    "\npagespeed.delayImagesInlineInit();";

const char DelayImagesFilter::kOnloadFunction[] =
    "var elem=this;"
    "setTimeout(function(){elem.onload = null;"
    "elem.src=elem.getAttribute('pagespeed_high_res_src');}, 0);";

DelayImagesFilter::DelayImagesFilter(RewriteDriver* driver)
    : driver_(driver),
      static_asset_manager_(
          driver->server_context()->static_asset_manager()),
      low_res_map_inserted_(false),
      num_low_res_inlined_images_(0),
      is_experimental_inline_preview_enabled_(
          driver_->options()->enable_inline_preview_images_experimental()) {
  // Low res images will be placed inside the respective image tag if any one of
  // kDeferJavascript or kLazyloadImages is turned off or
  // enable_inline_preview_images_experimental is set to true. Otherwise, low
  // res images will be blocked by javascript or images which are not critical.
  // If mobile aggressive rewriters are turned on, the low res images are NOT
  // inlined in the image tag.
  insert_low_res_images_inplace_ =
      !DisableInplaceLowResForMobile() &&
          (is_experimental_inline_preview_enabled_ ||
           !driver_->options()->Enabled(RewriteOptions::kDeferJavascript) ||
           !driver_->options()->Enabled(RewriteOptions::kLazyloadImages));
  lazyload_highres_images_ = driver_->options()->lazyload_highres_images() &&
      driver_->device_properties()->IsMobile();
}

DelayImagesFilter::~DelayImagesFilter() {}

void DelayImagesFilter::StartDocument() {
  low_res_map_inserted_ = false;
  num_low_res_inlined_images_ = 0;
}

void DelayImagesFilter::EndDocument() {
  low_res_data_map_.clear();
}

void DelayImagesFilter::EndElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kBody && !low_res_map_inserted_ &&
      !low_res_data_map_.empty()) {
    InsertDelayImagesInlineJS(element);
  } else if (driver_->IsRewritable(element) &&
             (element->keyword() == HtmlName::kImg ||
              element->keyword() == HtmlName::kInput)) {
    // We only handle img and input tag images.  Note that delay_images.js and
    // delay_images_inline.js must be modified to handle other possible tags.
    // We should probably specifically *not* include low res images for link
    // tags of various sorts (favicons, mobile desktop icons, etc.).  Use of low
    // res for explicit background images is a more interesting case, but the
    // current DOM walk in the above js files would need to be modified to
    // handle the large number of tags that we can identify in
    // resource_tag_scanner::ScanElement.
    semantic_type::Category category;
    HtmlElement::Attribute* src = resource_tag_scanner::ScanElement(
        element, driver_, &category);

    if (src != NULL && src->DecodedValueOrNull() != NULL &&
        category == semantic_type::kImage) {
      // Remove the inline_src which is low quality base64 encoded data url and
      // add them to a map so that all inline data urls will be available at the
      // end of body tag.
      HtmlElement::Attribute* low_res_src =
          element->FindAttribute(HtmlName::kPagespeedLowResSrc);
      if ((low_res_src != NULL) &&
          (low_res_src->DecodedValueOrNull() != NULL)) {
        ++num_low_res_inlined_images_;
        // Experimental mode adds an onload attribute of the image tag. So, low
        // res image is only added if onload function is not already present.
        if (!is_experimental_inline_preview_enabled_ ||
            element->FindAttribute(HtmlName::kOnload) == NULL) {
          // Low res image data is collected in low_res_data_map_ map. This
          // low_res_src will be moved just after last low res image in the html
          // DOM.
          // It is better to move inlined low resolution data later in the DOM,
          // otherwise they will block further parsing and rendering of the html
          // page.
          // High res src is added and original img src attribute is removed
          // from img tag.
          driver_->log_record()->SetRewriterLoggingStatus(
              RewriteOptions::FilterId(RewriteOptions::kDelayImages),
              RewriterInfo::APPLIED_OK);
          driver_->SetAttributeName(src, HtmlName::kPagespeedHighResSrc);
          if (insert_low_res_images_inplace_) {
            driver_->AddAttribute(element, HtmlName::kSrc,
                                  low_res_src->DecodedValueOrNull());
            // Add the image onload tag if experimental preview flag is enabled
            // lazyload_highres flag is disabled. If the lazyload is set,
            // we want to lazy load the high res images and not part of image
            // onload.
            if (is_experimental_inline_preview_enabled_ &&
                !lazyload_highres_images_) {
              driver_->AddEscapedAttribute(
                  element, HtmlName::kOnload, kOnloadFunction);
            }
          } else {
            const GoogleString& src_content = src->DecodedValueOrNull();
            low_res_data_map_[src_content] =
                low_res_src->DecodedValueOrNull();
          }
        }
        if (num_low_res_inlined_images_ ==
            driver_->num_inline_preview_images()) {
          if (insert_low_res_images_inplace_) {
            InsertDelayImagesJS(element);
          } else {
            InsertDelayImagesInlineJS(element);
          }
        }
      }
      element->DeleteAttribute(HtmlName::kPagespeedLowResSrc);
    }
  }
}

void DelayImagesFilter::InsertDelayImagesInlineJS(HtmlElement* element) {
  GoogleString inline_script;
  HtmlElement *current_element = element;
  // Check script for changing src to low res data url is inserted once.
  if (!low_res_map_inserted_) {
    inline_script = StrCat(
        static_asset_manager_->GetAsset(
            StaticAssetManager::kDelayImagesInlineJs,
            driver_->options()),
        kDelayImagesInlineSuffix);
    HtmlElement* script_element =
        driver_->NewElement(element, HtmlName::kScript);
    driver_->InsertElementAfterElement(current_element, script_element);
    static_asset_manager_->AddJsToElement(
        inline_script, script_element, driver_);
    current_element = script_element;
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
        driver_->NewElement(current_element, HtmlName::kScript);
    driver_->InsertElementAfterElement(current_element, low_res_element);
    static_asset_manager_->AddJsToElement(
        inline_data_script, low_res_element, driver_);
    current_element = low_res_element;
  }
  low_res_data_map_.clear();

  InsertDelayImagesJS(current_element);
}

void DelayImagesFilter::InsertDelayImagesJS(HtmlElement* element) {
  if (is_experimental_inline_preview_enabled_ && !lazyload_highres_images_) {
    return;
  }
  GoogleString delay_images_js;
  // Check script for changing src to high res src is inserted once.
  if (!low_res_map_inserted_) {
    delay_images_js = StrCat(
        static_asset_manager_->GetAsset(
            StaticAssetManager::kDelayImagesJs,
            driver_->options()),
        kDelayImagesSuffix);
  }

  if (lazyload_highres_images_) {
    StrAppend(&delay_images_js,
              "\npagespeed.delayImages.registerLazyLoadHighRes();\n");
  } else {
    StrAppend(&delay_images_js,
              "\npagespeed.delayImages.replaceWithHighRes();\n");
  }
  HtmlElement* script = driver_->NewElement(element, HtmlName::kScript);
  driver_->InsertElementAfterElement(element, script);
  static_asset_manager_->AddJsToElement(delay_images_js, script, driver_);
  low_res_map_inserted_ = true;
}

bool DelayImagesFilter::DisableInplaceLowResForMobile() const {
  const RewriteOptions* options = driver_->options();
  return options->enable_aggressive_rewriters_for_mobile() &&
      driver_->device_properties()->IsMobile();
}

void DelayImagesFilter::DetermineEnabled() {
  LogRecord* log_record = driver_->log_record();
  if (!driver_->device_properties()->SupportsImageInlining()) {
    log_record->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kDelayImages),
        RewriterStats::USER_AGENT_NOT_SUPPORTED);
    set_is_enabled(false);
    return;
  }
  log_record->LogRewriterHtmlStatus(
      RewriteOptions::FilterId(RewriteOptions::kDelayImages),
      RewriterStats::ACTIVE);
  set_is_enabled(true);
}

}  // namespace net_instaweb
