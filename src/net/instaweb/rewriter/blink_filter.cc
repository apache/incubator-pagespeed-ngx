/*
 * Copyright 2012 Google Inc.
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

// Author: rahulbansal@google.com (Rahul Bansal)

#include "net/instaweb/rewriter/public/blink_filter.h"

#include <vector>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/writer.h"
#include "net/instaweb/util/public/json.h"
#include "net/instaweb/util/public/proto_util.h"

namespace net_instaweb {

const char BlinkFilter::kBlinkCriticalLineDataPropertyName[] =
    "blink_critical_line_data";
const char BlinkFilter::kBlinkCohort[] = "blink";
const char BlinkFilter::kRefreshPageJs[] =
    "<script type='text/javascript'>document.location.reload(true);</script>";

BlinkFilter::BlinkFilter(RewriteDriver* rewrite_driver)
    : HtmlWriterFilter(rewrite_driver),
      rewrite_driver_(rewrite_driver),
      rewrite_options_(rewrite_driver->options()),
      string_writer_(&buffer_),
      current_non_cacheable_element_(NULL),
      cohort_(NULL),
      abort_filter_(false) {
}

BlinkFilter::~BlinkFilter() {}

void BlinkFilter::StartDocument() {
  if (rewrite_options_->passthrough_blink_for_last_invalid_response_code()) {
    rewrite_driver_->UpdatePropertyValueInDomCohort(
        BlinkUtil::kBlinkResponseCodePropertyName,
        IntegerToString(rewrite_driver_->response_headers()->status_code()));
  }

  buffer_.clear();
  current_non_cacheable_element_ = NULL;
  num_children_stack_.clear();

  set_writer(&string_writer_);
  GoogleUrl url(rewrite_driver_->google_url().Spec());
  BlinkUtil::PopulateAttributeToNonCacheableValuesMap(
      rewrite_options_, url, &attribute_non_cacheable_values_map_,
      &panel_number_num_instances_);

  ObtainBlinkCriticalLineData();
  if (!rewrite_options_->enable_blink_html_change_detection()) {
    HandleLastModifiedChange();
  }

  if (!abort_filter_) {
    SendCookies();
  }
}

void BlinkFilter::ObtainBlinkCriticalLineData() {
  cohort_ = rewrite_driver_->server_context()->page_property_cache()
      ->GetCohort(kBlinkCohort);
  PropertyValue* property_value = rewrite_driver_->property_page()->GetProperty(
      cohort_, kBlinkCriticalLineDataPropertyName);
  ArrayInputStream input(property_value->value().data(),
                         property_value->value().size());
  blink_critical_line_data_.ParseFromZeroCopyStream(&input);
}

void BlinkFilter::HandleLastModifiedChange() {
  abort_filter_ = false;
  const char* last_modified_date_in_fetch =
      rewrite_driver_->response_headers()->Lookup1(kPsaLastModified);
  bool has_last_modified_date_in_cache =
      blink_critical_line_data_.has_last_modified_date();
  if (last_modified_date_in_fetch == NULL) {
    if (has_last_modified_date_in_cache) {
      // Header was there earlier, but not there now.
      LOG(ERROR) << "Header " << kPsaLastModified << " is not there in "
                 << "response anymore for " << rewrite_driver_->url();
    }
  } else if (!has_last_modified_date_in_cache ||
             blink_critical_line_data_.last_modified_date() !=
             last_modified_date_in_fetch) {
    // TODO(sriharis):  Change the above check to a '>' comparison of dates.
    PropertyPage* page = rewrite_driver_->property_page();
    page->DeleteProperty(cohort_, kBlinkCriticalLineDataPropertyName);
    rewrite_driver_->server_context()->page_property_cache()->WriteCohort(
        cohort_, page);
    if (has_last_modified_date_in_cache) {
      abort_filter_ = true;
      // TODO(sriharis):  Should we redirect to ?ModPagespeed=off instead?
      WriteString(kRefreshPageJs);
    }
  }
}

void BlinkFilter::StartElement(HtmlElement* element) {
  if (abort_filter_) {
    return;
  }
  if (!num_children_stack_.empty()) {
    // Don't increment the count for noscript since the cached html doesn't
    // have it.
    if (element->keyword() != HtmlName::kNoscript) {
      num_children_stack_.back()++;
    }
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

void BlinkFilter::EndElement(HtmlElement* element) {
  if (abort_filter_) {
    return;
  }
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

GoogleString BlinkFilter::GetXpathOfCurrentElement(HtmlElement* element) {
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

void BlinkFilter::EndDocument() {
  if (!abort_filter_ && rewrite_driver_->serve_blink_non_critical()) {
    ServeNonCriticalPanelContents();
  }
  WriteString("\n</body></html>\n");
}

void BlinkFilter::SendCookies() {
  ConstStringStarVector cookies;
  const ResponseHeaders* response_headers = rewrite_driver_->response_headers();
  if (!response_headers->Lookup(HttpAttributes::kSetCookie, &cookies)) {
    return;
  }

  Json::Value cookie_array = Json::arrayValue;
  for (int i = 0, n = cookies.size(); i < n; ++i) {
    cookie_array.append(cookies[i]->c_str());
  }

  Json::FastWriter fast_writer;
  GoogleString cookie_str = fast_writer.write(cookie_array);
  BlinkUtil::StripTrailingNewline(&cookie_str);

  WriteString("<script>pagespeed.panelLoader.loadCookies(");
  WriteString(cookie_str);
  WriteString(");</script>");
}

void BlinkFilter::ServeNonCriticalPanelContents() {
  SendNonCriticalJson(blink_critical_line_data_.mutable_non_critical_json());
}

void BlinkFilter::SendNonCacheableObject(const Json::Value& json) {
  Json::FastWriter fast_writer;
  GoogleString json_str = fast_writer.write(json);
  BlinkUtil::EscapeString(&json_str);
  GoogleString script_str =
      StrCat("<script>pagespeed.panelLoader.loadNonCacheableObject(",
             json_str, ");</script>");
  WriteString(script_str);
  Flush();
}

void BlinkFilter::SendNonCriticalJson(GoogleString* str) {
  WriteString("<script>pagespeed.panelLoader.bufferNonCriticalData(");
  BlinkUtil::EscapeString(str);
  WriteString(*str);
  WriteString(");</script>");
  Flush();
}

void BlinkFilter::WriteString(StringPiece str) {
  rewrite_driver_->writer()->Write(str, rewrite_driver_->message_handler());
}

void BlinkFilter::Flush() {
  rewrite_driver_->writer()->Flush(rewrite_driver_->message_handler());
}

}  // namespace net_instaweb
