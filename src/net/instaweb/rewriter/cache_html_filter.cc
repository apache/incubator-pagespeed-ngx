/*
 * Copyright 2013 Google Inc.
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

// Authors: mmohabey@google.com (Megha Mohabey)
//          rahulbansal@google.com (Rahul Bansal)

#include "net/instaweb/rewriter/public/cache_html_filter.h"

#include <vector>

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/writer.h"
#include "net/instaweb/util/public/json.h"

namespace net_instaweb {

CacheHtmlFilter::CacheHtmlFilter(RewriteDriver* rewrite_driver)
    : HtmlWriterFilter(rewrite_driver),
      rewrite_driver_(rewrite_driver),
      rewrite_options_(rewrite_driver->options()),
      string_writer_(&buffer_),
      current_non_cacheable_element_(NULL),
      cohort_(NULL) {
}

CacheHtmlFilter::~CacheHtmlFilter() {}

void CacheHtmlFilter::StartDocument() {
  buffer_.clear();
  current_non_cacheable_element_ = NULL;
  num_children_stack_.clear();

  set_writer(&string_writer_);
  GoogleUrl url(rewrite_driver_->google_url().Spec());
  BlinkUtil::PopulateAttributeToNonCacheableValuesMap(
      rewrite_options_, url, &attribute_non_cacheable_values_map_,
      &panel_number_num_instances_);

  SendCookies();
}

void CacheHtmlFilter::StartElement(HtmlElement* element) {
  if (!num_children_stack_.empty()) {
    num_children_stack_.back()++;
    num_children_stack_.push_back(0);
  } else if (element->keyword() == HtmlName::kBody) {
    // Start the stack only once body is encountered.
    num_children_stack_.push_back(0);
  }
  if (current_non_cacheable_element_ == NULL) {
    int panel_number = BlinkUtil::GetPanelNumberForNonCacheableElement(
        attribute_non_cacheable_values_map_, element);
    if (panel_number != -1) {
      current_panel_id_ = BlinkUtil::GetPanelId(
          panel_number, panel_number_num_instances_[panel_number]);
      panel_number_num_instances_[panel_number]++;
      current_non_cacheable_element_ = element;
      buffer_.clear();
    }
  }
  HtmlWriterFilter::StartElement(element);
}

void CacheHtmlFilter::EndElement(HtmlElement* element) {
  if (!num_children_stack_.empty()) {
    num_children_stack_.pop_back();
  }
  HtmlWriterFilter::EndElement(element);
  if (current_non_cacheable_element_ == element) {
    Json::Value non_cacheable_json;
    non_cacheable_json[current_panel_id_][BlinkUtil::kInstanceHtml] =
        buffer_.c_str();
    non_cacheable_json[current_panel_id_][BlinkUtil::kXpath] =
        GetXpathOfCurrentElement(element).c_str();
    SendNonCacheableObject(non_cacheable_json);
    current_non_cacheable_element_ = NULL;
  }
}

GoogleString CacheHtmlFilter::GetXpathOfCurrentElement(HtmlElement* element) {
  if (num_children_stack_.empty()) {
    return "";
  }

  int child_number = num_children_stack_.back();
  GoogleString xpath = StrCat(element->name_str(), "[",
                              IntegerToString(child_number), "]");

  // i tracks the index of the parent among its siblings.
  int i = num_children_stack_.size() - 2;
  for (HtmlElement* parent = element->parent(); i >=0;
      parent = parent->parent(), --i) {
    StringPiece id = parent->AttributeValue(HtmlName::kId);
    if (id != NULL) {
      xpath = StrCat(parent->name_str(), "[@id=\"", id, "\"]/", xpath);
      break;
    } else {
      int child_number = num_children_stack_[i];
      xpath = StrCat(parent->name_str(), "[",
                     IntegerToString(child_number), "]", "/", xpath);
    }
    // TODO(rahulbansal): Handle the case when there is no body tag,
    // multiple body tags etc.
    if (parent->keyword() == HtmlName::kBody) {
      break;
    }
  }
  return StrCat("//", xpath);
}

void CacheHtmlFilter::EndDocument() {
  WriteString("<script>pagespeed.panelLoader.bufferNonCriticalData({})"
              ";</script>");
  Flush();
}

void CacheHtmlFilter::SendCookies() {
  GoogleString cookie_str;
  const ResponseHeaders* response_headers = rewrite_driver_->response_headers();
  if (response_headers->GetCookieString(&cookie_str)) {
    WriteString("<script>pagespeed.panelLoader.loadCookies(");
    WriteString(cookie_str);
    WriteString(");</script>");
  }
}

void CacheHtmlFilter::SendNonCacheableObject(const Json::Value& json) {
  Json::FastWriter fast_writer;
  GoogleString json_str = fast_writer.write(json);
  BlinkUtil::EscapeString(&json_str);
  GoogleString script_str =
      StrCat("<script>pagespeed.panelLoader.loadNonCacheableObject(",
             json_str, ");</script>");
  WriteString(script_str);
  Flush();
}

void CacheHtmlFilter::WriteString(StringPiece str) {
  rewrite_driver_->writer()->Write(str, rewrite_driver_->message_handler());
}

void CacheHtmlFilter::Flush() {
  rewrite_driver_->writer()->Flush(rewrite_driver_->message_handler());
}

}  // namespace net_instaweb
