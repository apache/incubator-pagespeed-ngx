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
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

// Script content which converts low res inline_src images to high res images
// after onload event.
// TODO(pulkitg): Use data2c and remove duplicate code in lines 54-66.
const char* DelayImagesFilter::kDelayScript =
    "\n"
    "function pagespeed_delay_images_onload_handler() {\n"
    "  var pagespeed_img_tags = document.getElementsByTagName('img');\n"
    "  for (var i = 0; i < pagespeed_img_tags.length; ++i) {\n"
    "    var src = pagespeed_img_tags[i].getAttribute(\n"
    "        'pagespeed_high_res_src');\n"
    "    if (src) {\n"
    "      pagespeed_img_tags[i].setAttribute('src', src);\n"
    "    }\n"
    "  }\n"
    "}\n"
    "function pagespeed_delay_func() {\n"
    "  setTimeout(pagespeed_delay_images_onload_handler,\n"
    "             100);\n"
    "}\n"
    "if (window.addEventListener) {\n"
    "  window.addEventListener('load', pagespeed_delay_func, false);\n"
    "} else if (window.attachEvent) {\n"
    "  window.attachEvent('onload', pagespeed_delay_func);\n"
    "} else {\n"
    "  var oldOnload = window.onload;\n"
    "  window.onload = function() {\n"
    "    if (oldOnload) {\n"
    "      oldOnload.call(this);\n"
    "    }\n"
    "    pagespeed_delay_func();\n"
    "  }\n"
    "}\n";

// Script content which puts low quality/resolution images' base64 encoded data
// url into the respective image tag.
const char* DelayImagesFilter::kShowInlineScript =
    "var pagespeed_img_tags = document.getElementsByTagName('img');\n"
    "for (var i = 0; i < pagespeed_img_tags.length; ++i) {\n"
    "  var src = pagespeed_img_tags[i].getAttribute(\n"
    "      'pagespeed_high_res_src');\n"
    "  if (src) {\n"
    "    pagespeed_img_tags[i].setAttribute('src',\n"
    "                                       pagespeed_inline_map[src]);\n"
    "  }\n"
    "}\n";

DelayImagesFilter::DelayImagesFilter(RewriteDriver* driver)
    : driver_(driver),
      tag_scanner_(driver),
      low_res_map_inserted_(false),
      delay_script_inserted_(false) {}

DelayImagesFilter::~DelayImagesFilter() {}

void DelayImagesFilter::StartDocument() {
  low_res_map_inserted_ = false;
  delay_script_inserted_ = false;
}

void DelayImagesFilter::EndDocument() {
  low_res_data_map_.clear();
}

void DelayImagesFilter::EndElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kHead && !delay_script_inserted_) {
    // Append kDelayScript at the end of the Head.
    HtmlElement* script = driver_->NewElement(element, HtmlName::kScript);
    driver_->AddAttribute(script, HtmlName::kType, "text/javascript");
    HtmlCharactersNode* script_content = driver_->NewCharactersNode(
        script, kDelayScript);
    driver_->AppendChild(element, script);
    driver_->AppendChild(script, script_content);
    delay_script_inserted_ = true;
  } else if (element->keyword() == HtmlName::kImg &&
      driver_->IsRewritable(element)) {
    // Remove the inline_src which is low quality base64 encoded data url and
    // add them to a map so that all inline data urls will be available at the
    // end of body tag.
    HtmlElement::Attribute* low_res_src =
        element->FindAttribute(HtmlName::kPagespeedLowResSrc);
    if (!low_res_map_inserted_ && delay_script_inserted_) {
      HtmlElement::Attribute* src =
          element->FindAttribute(HtmlName::kSrc);
      if (src != NULL && low_res_src != NULL) {
        // Low res image data is collected in low_res_data_map_ map. This
        // low_res_src will be moved to the end of body tag in next else
        // condition.
        // It is better to move inlined low resolution data at the end of body
        // tag, otherwise they will block further parsing and rendering of
        // the html page.
        GoogleString src_content = src->value();
        low_res_data_map_[src_content] = low_res_src->value();
        // High res src is added and original img src attribute is removed from
        // img tag.
        driver_->AddAttribute(element, HtmlName::kPagespeedHighResSrc,
                              src_content);
        element->DeleteAttribute(HtmlName::kSrc);
      }
    }
    element->DeleteAttribute(HtmlName::kPagespeedLowResSrc);
  } else if (!low_res_map_inserted_ &&
      element->keyword() == HtmlName::kBody && !low_res_data_map_.empty()) {
    // Generate javascript map for inline data urls where key is url and
    // base64 encoded data url as its value. This map is added to the html at
    // the end of body tag.
    GoogleString inline_data_script = "var pagespeed_inline_map = {};\n";
    for (StringStringMap::iterator it = low_res_data_map_.begin();
        it != low_res_data_map_.end(); ++it) {
      StrAppend(&inline_data_script, "pagespeed_inline_map['", it->first,
                "'] = '", it->second, "';\n");
    }

    StrAppend(&inline_data_script, kShowInlineScript);
    HtmlElement* script = driver_->NewElement(element, HtmlName::kScript);
    driver_->AddAttribute(script, HtmlName::kType, "text/javascript");
    HtmlCharactersNode* script_content = driver_->NewCharactersNode(
        script, inline_data_script);
    driver_->AppendChild(element, script);
    driver_->AppendChild(script, script_content);
    low_res_map_inserted_ = true;
  }
}

}  // namespace net_instaweb
