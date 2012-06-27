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
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

const char DelayImagesFilter::kDelayImagesSuffix[] =
    "\npagespeed.delayImagesInit()";

const char DelayImagesFilter::kDelayImagesInlineSuffix[] =
      "\npagespeed.delayImagesInlineInit();";

DelayImagesFilter::DelayImagesFilter(RewriteDriver* driver)
    : driver_(driver),
      static_js_manager_(
          driver->resource_manager()->static_javascript_manager()),
      low_res_map_inserted_(false),
      num_low_res_inlined_images_(0) {
  // Low res images will be placed inside the respective image tag if any one of
  // kDeferJavascript or kLazyloadImages is turned off. Otherwise, low res
  // images will be blocked by javascript or images which are not critical.
  insert_low_res_images_inplace_ =
      !driver_->options()->Enabled(RewriteOptions::kDeferJavascript) ||
      !driver_->options()->Enabled(RewriteOptions::kLazyloadImages);
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
  } else if (driver_->IsRewritable(element)) {
    ContentType::Category category;
    HtmlElement::Attribute* src = resource_tag_scanner::ScanElement(
        element, driver_, &category);

    if (src != NULL && src->DecodedValueOrNull() != NULL &&
        category == ContentType::kImage) {
      // Remove the inline_src which is low quality base64 encoded data url and
      // add them to a map so that all inline data urls will be available at the
      // end of body tag.
      HtmlElement::Attribute* low_res_src =
          element->FindAttribute(HtmlName::kPagespeedLowResSrc);
      if ((low_res_src != NULL) &&
          (low_res_src->DecodedValueOrNull() != NULL)) {
        ++num_low_res_inlined_images_;
        // TODO(pulkitg): Add support for input tag.
        if (element->keyword() == HtmlName::kImg) {
          // Low res image data is collected in low_res_data_map_ map. This
          // low_res_src will be moved just after last low res image in the
          // html DOM.
          // It is better to move inlined low resolution data later in the DOM,
          // otherwise they will block further parsing and rendering of
          // the html page.
          // High res src is added and original img src attribute is removed
          // from img tag.
          driver_->SetAttributeName(src, HtmlName::kPagespeedHighResSrc);
          if (insert_low_res_images_inplace_) {
            driver_->AddAttribute(element, HtmlName::kSrc,
                                  low_res_src->DecodedValueOrNull());
          } else {
            const GoogleString& src_content = src->DecodedValueOrNull();
            low_res_data_map_[src_content] = low_res_src->DecodedValueOrNull();
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
  // Generate javascript map for inline data urls where key is url and
  // base64 encoded data url as its value. This map is added to the
  // html at the end of last low res image.
  GoogleString inline_data_script;
  for (StringStringMap::iterator it = low_res_data_map_.begin();
      it != low_res_data_map_.end(); ++it) {
    StrAppend(&inline_data_script,
              "\npagespeed.delayImagesInline.addLowResImages('",
              it->first, "', '", it->second, "');");
  }
  low_res_data_map_.clear();
  GoogleString inline_script;
  // Check script for changing src to low res data url is inserted once.
  if (!low_res_map_inserted_) {
    inline_script = StrCat(
        static_js_manager_->GetJsSnippet(
            StaticJavascriptManager::kDelayImagesInlineJs,
            driver_->options()),
        kDelayImagesInlineSuffix);
  }
  StrAppend(&inline_script,
            inline_data_script,
            "\npagespeed.delayImagesInline.replaceWithLowRes();\n");
  HtmlElement* script = driver_->NewElement(element,
                                            HtmlName::kScript);
  driver_->AddAttribute(script, HtmlName::kType, "text/javascript");
  HtmlCharactersNode* script_content = driver_->NewCharactersNode(
      script, inline_script);
  driver_->InsertElementAfterElement(element, script);
  driver_->AppendChild(script, script_content);
  InsertDelayImagesJS(script);
}

void DelayImagesFilter::InsertDelayImagesJS(HtmlElement* element) {
  HtmlElement* script = driver_->NewElement(element,
                                            HtmlName::kScript);
  driver_->AddAttribute(script, HtmlName::kType, "text/javascript");
  GoogleString delay_images_js;
  // Check script for changing src to high res src is inserted once.
  if (!low_res_map_inserted_) {
  delay_images_js = StrCat(
      static_js_manager_->GetJsSnippet(
          StaticJavascriptManager::kDelayImagesJs,
          driver_->options()),
      kDelayImagesSuffix);
  } else {
    delay_images_js = "\npagespeed.delayImages.replaceWithHighRes();\n";
  }
  HtmlCharactersNode* script_content = driver_->NewCharactersNode(
      script, delay_images_js);
  driver_->InsertElementAfterElement(element, script);
  driver_->AppendChild(script, script_content);
  low_res_map_inserted_ = true;
}

}  // namespace net_instaweb
