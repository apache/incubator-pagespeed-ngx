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

#include "net/instaweb/rewriter/public/split_html_filter.h"

#include <map>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/critical_line_info.pb.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/json_writer.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"
#include "net/instaweb/util/public/re2.h"

namespace net_instaweb {

const char SplitHtmlFilter::kRenderCohort[] = "render";
const char SplitHtmlFilter::kCriticalLineInfoPropertyName[] =
    "critical_line_info";

// At StartElement, if element is panel instance push a new json to capture
// contents of instance to the json stack.
// All the emitBytes are captured into the top json until a new panel
// instance is found or the current panel instance ends.
SplitHtmlFilter::SplitHtmlFilter(RewriteDriver* rewrite_driver)
    : HtmlWriterFilter(rewrite_driver),
      rewrite_driver_(rewrite_driver),
      options_(rewrite_driver->options()),
      script_written_(false) {
}

SplitHtmlFilter::~SplitHtmlFilter() {
  STLDeleteContainerPairSecondPointers(xpath_map_.begin(), xpath_map_.end());
}

void SplitHtmlFilter::StartDocument() {
  num_children_stack_.clear();
  url_ = rewrite_driver_->google_url().Spec();
  json_writer_.reset(new JsonWriter(rewrite_driver_->writer(),
                                    &element_json_stack_));
  set_writer(json_writer_.get());

  HtmlWriterFilter::StartDocument();

  // Push json object to capture critical html.
  StartPanelInstance(static_cast<HtmlElement*>(NULL));

  ReadCriticalLineConfig();

  script_written_ = false;
}

void SplitHtmlFilter::Cleanup() {
  // Delete the root object pushed in StartDocument;
  delete element_json_stack_[0].second;
  element_json_stack_.pop_back();
}

void SplitHtmlFilter::EndDocument() {
  json_writer_->UpdateDictionary();
  Flush();

  // Remove critical html since it should already have been sent out by now.
  element_json_stack_[0].second->removeMember(BlinkUtil::kInstanceHtml);

  Json::Value json = Json::arrayValue;
  json.append(*(element_json_stack_[0].second));

  ServeNonCriticalPanelContents(json);
  WriteString("\n</body></html>\n");
  Cleanup();
}

void SplitHtmlFilter::WriteString(const StringPiece& str) {
  rewrite_driver_->writer()->Write(str, rewrite_driver_->message_handler());
}

void SplitHtmlFilter::Flush() {
  if (element_json_stack_.size() == 0) {
    return;
  }

  GoogleString instance_html =
      (*(element_json_stack_[0].second))[BlinkUtil::kInstanceHtml].asCString();
  if (instance_html.empty()) {
    return;
  }

  WriteString(instance_html);
  HtmlWriterFilter::Flush();
  (*(element_json_stack_[0].second))[BlinkUtil::kInstanceHtml] = "";
}

void SplitHtmlFilter::ServeNonCriticalPanelContents(const Json::Value& json) {
  GoogleString non_critical_json = fast_writer_.write(json);
  BlinkUtil::StripTrailingNewline(&non_critical_json);
  WriteString("<script>pagespeed.panelLoader.bufferNonCriticalData(");
  BlinkUtil::EscapeString(&non_critical_json);
  WriteString(non_critical_json);
  WriteString(");</script>");
  HtmlWriterFilter::Flush();
}

void SplitHtmlFilter::ReadCriticalLineConfig() {
  const GoogleString& critical_line_config_from_options =
       options_->critical_line_config();
  if (!critical_line_config_from_options.empty()) {
    StringPieceVector xpaths;
    SplitStringPieceToVector(critical_line_config_from_options, ",",
                             &xpaths, true);
    for (int i = 0, n = xpaths.size(); i < n; i++) {
      StringPieceVector xpath_pair;
      SplitStringPieceToVector(xpaths[i], ":", &xpath_pair, true);
      Panel* panel = critical_line_info_.add_panels();
      panel->set_start_xpath(xpath_pair[0].data(), xpath_pair[0].length());
      if (xpath_pair.size() == 2) {
        panel->set_end_marker_xpath(
            xpath_pair[1].data(), xpath_pair[1].length());
      }
    }
  } else {
    const PropertyCache::Cohort* cohort_ =
        rewrite_driver_->server_context()->page_property_cache()->
        GetCohort(kRenderCohort);
    PropertyValue* property_value = rewrite_driver_->property_page()->
        GetProperty(cohort_, kCriticalLineInfoPropertyName);
    if (property_value != NULL) {
      ArrayInputStream input(property_value->value().data(),
                             property_value->value().size());
      critical_line_info_.ParseFromZeroCopyStream(&input);
    }
  }

  ComputePanels(critical_line_info_, &panel_id_to_spec_);
  PopulateXpathMap();
}

void SplitHtmlFilter::ComputePanels(
    const CriticalLineInfo& critical_line_info,
    PanelIdToSpecMap* panel_id_to_spec) {
  for (int i = 0; i < critical_line_info.panels_size(); ++i) {
    const Panel& panel = critical_line_info.panels(i);
    const GoogleString panel_id =
        StrCat(BlinkUtil::kPanelId, ".", IntegerToString(i));
    (*panel_id_to_spec)[panel_id] = &panel;
  }
}

void SplitHtmlFilter::PopulateXpathMap() {
  for (int i = 0; i < critical_line_info_.panels_size(); ++i) {
    const Panel& panel = critical_line_info_.panels(i);
    PopulateXpathMap(panel.start_xpath());
    if (panel.has_end_marker_xpath()) {
      PopulateXpathMap(panel.end_marker_xpath());
    }
  }
}

void SplitHtmlFilter::PopulateXpathMap(const GoogleString& xpath) {
  if (xpath_map_.find(xpath) == xpath_map_.end()) {
    XpathUnits* xpath_units = new XpathUnits();
    ParseXpath(xpath, xpath_units);
    xpath_map_[xpath] = xpath_units;
  }
}

bool SplitHtmlFilter::IsElementSiblingOfCurrentPanel(HtmlElement* element) {
  HtmlElement* current_panel_element = element_json_stack_.back().first;
  DCHECK_NE(current_panel_element, element);
  return current_panel_element != NULL &&
      current_panel_element->parent() == element->parent();
}

bool SplitHtmlFilter::IsElementParentOfCurrentPanel(HtmlElement* element) {
  HtmlElement* current_panel_element = element_json_stack_.back().first;
  return current_panel_element != NULL &&
      current_panel_element->parent() == element;
}

void SplitHtmlFilter::EndPanelInstance() {
  json_writer_->UpdateDictionary();

  ElementJsonPair element_json_pair = element_json_stack_.back();
  scoped_ptr<Json::Value> dictionary(element_json_pair.second);
  element_json_stack_.pop_back();
  Json::Value* parent_dictionary = element_json_stack_.back().second;
  GoogleString panel_id = GetPanelIdForInstance(element_json_pair.first);
  AppendJsonData(&((*parent_dictionary)[panel_id]), *dictionary);
}

void SplitHtmlFilter::StartPanelInstance(HtmlElement* element) {
  if (element_json_stack_.size() != 0) {
    json_writer_->UpdateDictionary();
  }

  Json::Value* new_json = new Json::Value(Json::objectValue);
  // Push new Json
  element_json_stack_.push_back(std::make_pair(element, new_json));
}

void SplitHtmlFilter::InsertPanelStub(HtmlElement* element,
                                      const GoogleString& panel_id) {
  HtmlCommentNode* comment = rewrite_driver_->NewCommentNode(
      element->parent(),
      StrCat(RewriteOptions::kPanelCommentPrefix, " begin ", panel_id));
  rewrite_driver_->InsertElementBeforeCurrent(comment);
  Comment(comment);
  // Append end stub to json.
  comment = rewrite_driver_->NewCommentNode(
      element->parent(),
      StrCat(RewriteOptions::kPanelCommentPrefix, " end ", panel_id));
  rewrite_driver_->InsertElementBeforeCurrent(comment);
  Comment(comment);
}

void SplitHtmlFilter::InsertBlinkJavascript(HtmlElement* element) {
  bool include_head = (element->keyword() != HtmlName::kHead);
  GoogleString defer_js_with_blink = "";
  if (include_head) {
    StrAppend(&defer_js_with_blink, "<head>");
  }

  StaticJavascriptManager* js_manager =
      rewrite_driver_->server_context()->static_javascript_manager();
  StrAppend(&defer_js_with_blink, "<script src=\"",
            js_manager->GetBlinkJsUrl(options_), "\"></script>");

  StrAppend(&defer_js_with_blink, "<script>pagespeed.deferInit();</script>");

  if (include_head) {
    StrAppend(&defer_js_with_blink, "</head>");
  }

  HtmlCharactersNode* blink_script_node = rewrite_driver_->NewCharactersNode(
      element, defer_js_with_blink);
  Characters(blink_script_node);
  script_written_ = true;
}

void SplitHtmlFilter::StartElement(HtmlElement* element) {
  if (!num_children_stack_.empty()) {
    num_children_stack_.back()++;;
    num_children_stack_.push_back(0);
  } else if (element->keyword() == HtmlName::kBody) {
    // Start the stack only once body is encountered.
    num_children_stack_.push_back(0);
  }

  if (element->keyword() == HtmlName::kBody && !script_written_) {
    InsertBlinkJavascript(element);
  }

  if (IsEndMarkerForCurrentPanel(element)) {
    EndPanelInstance();
  }

  GoogleString panel_id = MatchPanelIdForElement(element);
  // if panel_id is empty, then element didn't match with any start xpath of
  // panel specs
  if (!panel_id.empty()) {
    InsertPanelStub(element, panel_id);
    StartPanelInstance(element);
    MarkElementWithPanelId(element, panel_id);
  } else if (IsElementSiblingOfCurrentPanel(element)) {
    panel_id = GetPanelIdForInstance(element_json_stack_.back().first);
    MarkElementWithPanelId(element, panel_id);
  }
  HtmlWriterFilter::StartElement(element);
}

void SplitHtmlFilter::EndElement(HtmlElement* element) {
  if (!num_children_stack_.empty()) {
    num_children_stack_.pop_back();
  }
  if (IsElementParentOfCurrentPanel(element) ||
      (element->parent() == NULL &&
       element_json_stack_.back().first == element)) {
    EndPanelInstance();
  }

  if (element->keyword() == HtmlName::kHead && !script_written_) {
    InsertBlinkJavascript(element);
  }

  HtmlWriterFilter::EndElement(element);
}

void SplitHtmlFilter::AppendJsonData(Json::Value* dictionary,
                                 const Json::Value& dict) {
  if (!dictionary->isArray()) {
    *dictionary = Json::arrayValue;
  }
  dictionary->append(dict);
}

GoogleString SplitHtmlFilter::MatchPanelIdForElement(HtmlElement* element) {
  for (int i = 0; i < critical_line_info_.panels_size(); i++) {
    const Panel& panel = critical_line_info_.panels(i);
    if (ElementMatchesXpath(element, *(xpath_map_[panel.start_xpath()]))) {
      return StrCat(BlinkUtil::kPanelId, ".", IntegerToString(i));
    }
  }
  return "";
}

bool SplitHtmlFilter::IsEndMarkerForCurrentPanel(HtmlElement* element) {
  HtmlElement* current_panel_element = element_json_stack_.back().first;
  if (current_panel_element == NULL)
    return false;
  const GoogleString& panel_id = GetPanelIdForInstance(current_panel_element);

  if (panel_id_to_spec_.find(panel_id) == panel_id_to_spec_.end()) {
    LOG(DFATAL) << "Invalid Panelid: " << panel_id << " for url " << url_;
    return false;
  }
  const Panel& panel = *(panel_id_to_spec_[panel_id]);
  return panel.has_end_marker_xpath() ?
      ElementMatchesXpath(element, *(xpath_map_[panel.end_marker_xpath()])) :
      false;
}

void SplitHtmlFilter::MarkElementWithPanelId(HtmlElement* element,
                                         const GoogleString& panel_id) {
  element->AddAttribute(rewrite_driver_->MakeName(BlinkUtil::kPanelId),
                        panel_id, HtmlElement::DOUBLE_QUOTE);
}

GoogleString SplitHtmlFilter::GetPanelIdForInstance(HtmlElement* element) {
  GoogleString panel_id_value;
  StringPiece panel_id_attr_name = BlinkUtil::kPanelId;
  const HtmlElement::AttributeList& attrs = element->attributes();
  for (HtmlElement::AttributeConstIterator i(attrs.begin());
         i != attrs.end(); ++i) {
      const HtmlElement::Attribute& attribute = *i;
    if ((panel_id_attr_name == attribute.name().c_str()) &&
        (attribute.DecodedValueOrNull() != NULL)) {
      panel_id_value = attribute.DecodedValueOrNull();
      break;
    }
  }
  return panel_id_value;
}

bool SplitHtmlFilter::ParseXpath(const GoogleString& xpath,
                                 std::vector<XpathUnit>* xpath_units) {
  static const char* kXpathWithChildNumber = "(\\w+)(\\[(\\d+)\\])";
  static const char* kXpathWithId = "(\\w+)(\\[@(\\w+)\\s*=\\s*\"(.*)\"\\])";
  GoogleString modified_xpath = xpath.substr(2);
  std::vector<GoogleString> list;
  SplitStringUsingSubstr(modified_xpath, "/", &list);
  for (int j = 0, n = list.size(); j < n; j++) {
    XpathUnit unit;
    GoogleString str;
    if (!RE2::FullMatch(list[j], kXpathWithChildNumber,
                       &unit.tag_name, &str, &unit.child_number)) {
      GoogleString str1;
      RE2::FullMatch(list[j], kXpathWithId, &unit.tag_name, &str,
                     &str1, &unit.attribute_value);
    }
    xpath_units->push_back(unit);
  }
  return true;
}

bool SplitHtmlFilter::ElementMatchesXpath(
    const HtmlElement* element, const std::vector<XpathUnit>& xpath_units) {
  int j = xpath_units.size() - 1, k = num_children_stack_.size() - 2;
  for (; j >= 0 && k >= 0; j--, k--, element = element->parent()) {
    if (element->name_str() !=  xpath_units[j].tag_name) {
      return false;
    }
    if (!xpath_units[j].attribute_value.empty()) {
      return (element->AttributeValue(HtmlName::kId) != NULL &&
          element->AttributeValue(HtmlName::kId) ==
              xpath_units[j].attribute_value);
    } else if (xpath_units[j].child_number == num_children_stack_[k]) {
      continue;
    } else {
      return false;
    }
  }

  if (j < 0 && k < 0) {
      return true;
  }
  return false;
}

}  // namespace net_instaweb
