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

// Author: gagansingh@google.com (Gagan Singh)

#include "net/instaweb/rewriter/public/blink_util.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/rewriter/panel_config.pb.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/re2.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/wildcard.h"

namespace net_instaweb {
namespace BlinkUtil {

namespace {

bool IsBlacklistedBrowser(const StringPiece& user_agent,
                          const PublisherConfig& config) {
  for (int i = 0; i < config.browser_blacklist_patterns_size(); ++i) {
    Wildcard wildcard(config.browser_blacklist_patterns(i));
    if (wildcard.Match(user_agent)) {
      return true;
    }
  }
  return false;
}

}  // namespace

// TODO(rahulbansal): Add tests for this.
bool IsBlinkRequest(const GoogleUrl& url,
                    const RequestHeaders* request_headers,
                    const RewriteOptions* options,
                    const char* user_agent,
                    const UserAgentMatcher& user_agent_matcher_) {
  if (options != NULL &&
      // Is rewriting enabled?
      options->enabled() &&
      // Is Get Request?
      request_headers->method() == RequestHeaders::kGet &&
      // Is prioritize visible content filter enabled?
      options->Enabled(RewriteOptions::kPrioritizeVisibleContent) &&
      // Is url allowed? (i.e., it is not in black-list.)
      // TODO(sriharis): We also make this check in regular proxy flow
      // (ProxyFetch).  Should we combine these?
      options->IsAllowed(url.Spec()) &&
      // Does url match a cacheable family pattern specified in config?
      options->IsInBlinkCacheableFamily(url.PathAndLeaf()) &&
      // user agent supports Blink.
      user_agent_matcher_.GetBlinkUserAgentType(
          user_agent, options->enable_blink_for_mobile_devices()) !=
          UserAgentMatcher::kDoesNotSupportBlink) {
    return true;
  }
  return false;
}

bool ShouldApplyBlinkFlowCriticalLine(
    const ResourceManager* manager,
    const RewriteOptions* options) {
  return options != NULL &&
      // Blink flow critical line is enabled in rewrite options.
      options->enable_blink_critical_line() &&
      manager->blink_critical_line_data_finder() != NULL;
}

const Layout* ExtractBlinkLayout(const GoogleUrl& url,
                                 RewriteOptions* options,
                                 const StringPiece& user_agent) {
  if (options != NULL) {
    const PublisherConfig* config = options->panel_config();
    if (config != NULL && !IsBlacklistedBrowser(user_agent, *config)) {
      return FindLayout(*config, url);
    }
  }
  return NULL;
}

// Finds the layout for the given request_url.
const Layout* FindLayout(const PublisherConfig& config,
                         const GoogleUrl& request_url) {
  for (int i = 0; i < config.layout_size(); ++i) {  // Typically 3-4 layouts.
    const Layout& layout = config.layout(i);
    if (layout.reference_page_url_path() == request_url.PathAndLeaf()) {
      return &layout;
    }
    for (int j = 0; j < layout.relative_url_patterns_size(); ++j) {
      VLOG(2) << "regex = |" << layout.relative_url_patterns(j)
              << "|\t str = |" << request_url.PathAndLeaf() << "|";
      if (RE2::FullMatch(request_url.PathAndLeaf().data(),
                         layout.relative_url_patterns(j).data())) {
        return &layout;
      }
    }
  }
  return NULL;
}

void SplitCritical(const Json::Value& complete_json,
                   const PanelIdToSpecMap& panel_id_to_spec,
                   GoogleString* critical_json_str,
                   GoogleString* non_critical_json_str,
                   GoogleString* pushed_images_str) {
  Json::Value critical_json(Json::arrayValue);
  Json::Value non_cacheable_critical_json(Json::arrayValue);
  Json::Value non_critical_json(Json::arrayValue);
  Json::Value pushed_images(Json::objectValue);

  Json::Value panel_json(complete_json);
  panel_json[0].removeMember(kInstanceHtml);

  SplitCriticalArray(panel_json, panel_id_to_spec, &critical_json,
                     &non_cacheable_critical_json, &non_critical_json,
                     true, 1, &pushed_images);
  critical_json = critical_json.empty() ? Json::objectValue : critical_json[0];

  Json::FastWriter fast_writer;
  *critical_json_str = fast_writer.write(critical_json);
  BlinkUtil::StripTrailingNewline(critical_json_str);

  DeleteImagesFromJson(&non_critical_json);
  non_critical_json =
      non_critical_json.empty() ? Json::objectValue : non_critical_json[0];
  *non_critical_json_str = fast_writer.write(non_critical_json);
  BlinkUtil::StripTrailingNewline(non_critical_json_str);

  *pushed_images_str = fast_writer.write(pushed_images);
  BlinkUtil::StripTrailingNewline(pushed_images_str);
}

// complete_json = [panel1, panel2 ... ]
// panel = {
//   "instanceHtml": "html of panel",
//   "images": {"img1:<lowres>", "img2:<lowres>"} (images inside instanceHtml)
//   "panel-id.0": <complete_json>,
//   "panel-id.1": <complete_json>,
// }
//
// CRITICAL = [panel1]
// NON-CACHEABLE = [Empty panel, panel2]
// NON-CRITICAL = [Empty panel, Empty panel, panel3]
//
// TODO(ksimbili): Support images inling for non_cacheable too.
void SplitCriticalArray(const Json::Value& complete_json,
                        const PanelIdToSpecMap& panel_id_to_spec,
                        Json::Value* critical_json,
                        Json::Value* critical_non_cacheable_json,
                        Json::Value* non_critical_json,
                        bool panel_cacheable,
                        int num_critical_instances,
                        Json::Value* pushed_images) {
  DCHECK(pushed_images);
  num_critical_instances = std::min(num_critical_instances,
                                    static_cast<int>(complete_json.size()));

  for (int i = 0; i < num_critical_instances; ++i) {
    Json::Value instance_critical(Json::objectValue);
    Json::Value instance_non_cacheable_critical(Json::objectValue);
    Json::Value instance_non_critical(Json::objectValue);

    SplitCriticalObj(complete_json[i], panel_id_to_spec, &instance_critical,
                     &instance_non_cacheable_critical,
                     &instance_non_critical,
                     panel_cacheable,
                     pushed_images);
    critical_json->append(instance_critical);
    critical_non_cacheable_json->append(instance_non_cacheable_critical);
    non_critical_json->append(instance_non_critical);
  }

  for (Json::ArrayIndex i = num_critical_instances; i < complete_json.size();
      ++i) {
    non_critical_json->append(complete_json[i]);
  }

  ClearArrayIfAllEmpty(critical_json);
  ClearArrayIfAllEmpty(critical_non_cacheable_json);
  ClearArrayIfAllEmpty(non_critical_json);
}

void SplitCriticalObj(const Json::Value& json_obj,
                      const PanelIdToSpecMap& panel_id_to_spec,
                      Json::Value* critical_obj,
                      Json::Value* non_cacheable_obj,
                      Json::Value* non_critical_obj,
                      bool panel_cacheable,
                      Json::Value* pushed_images) {
  const std::vector<std::string>& keys = json_obj.getMemberNames();
  for (Json::ArrayIndex j = 0; j < keys.size(); ++j) {
    const std::string& key = keys[j];

    if (key == kContiguous) {
      (*critical_obj)[kContiguous] = json_obj[key];
      (*non_cacheable_obj)[kContiguous] = json_obj[key];
      (*non_critical_obj)[kContiguous] = json_obj[key];
      continue;
    }

    if (key == kInstanceHtml) {
      if (panel_cacheable) {
        (*critical_obj)[kInstanceHtml] = json_obj[key];
      } else {
        (*non_cacheable_obj)[kInstanceHtml] = json_obj[key];
      }
      continue;
    }

    if (key == kImages) {
      if (panel_cacheable) {
        const Json::Value& image_obj = json_obj[key];
        const std::vector<std::string>& image_keys = image_obj.getMemberNames();
        for (Json::ArrayIndex k = 0; k < image_keys.size(); ++k) {
          const std::string& image_url = image_keys[k];
          (*pushed_images)[image_url] = image_obj[image_url];
        }
      }
      continue;
    }

    if (panel_id_to_spec.find(key) == panel_id_to_spec.end()) {
      LOG(DFATAL) << "SplitCritical called with invalid Panelid: " << key;
      continue;
    }
    const Panel& child_panel = *((panel_id_to_spec.find(key))->second);

    Json::Value child_critical(Json::arrayValue);
    Json::Value child_non_cacheable_critical(Json::arrayValue);
    Json::Value child_non_critical(Json::arrayValue);
    bool child_panel_cacheable = panel_cacheable &&
        (child_panel.cacheability_in_minutes() != 0);
    SplitCriticalArray(json_obj[key],
                       panel_id_to_spec,
                       &child_critical,
                       &child_non_cacheable_critical,
                       &child_non_critical,
                       child_panel_cacheable,
                       child_panel.num_critical_instances(),
                       pushed_images);

    if (!child_critical.empty()) {
      (*critical_obj)[key] = child_critical;
    }
    if (!child_non_cacheable_critical.empty()) {
      (*non_cacheable_obj)[key] = child_non_cacheable_critical;
    }
    if (!child_non_critical.empty()) {
      (*non_critical_obj)[key] = child_non_critical;
    }
  }
}

bool IsJsonEmpty(const Json::Value& json) {
  const std::vector<std::string>& keys = json.getMemberNames();
  for (Json::ArrayIndex k = 0; k < keys.size(); ++k) {
    const std::string& key = keys[k];
    if (key != kContiguous) {
      return false;
    }
  }
  return true;
}

void ClearArrayIfAllEmpty(Json::Value* json) {
  for (Json::ArrayIndex i = 0; i < json->size(); ++i) {
    if (!IsJsonEmpty((*json)[i])) {
      return;
    }
  }
  json->clear();
}

void DeleteImagesFromJson(Json::Value* complete_json) {
  for (Json::ArrayIndex i = 0; i < complete_json->size(); ++i) {
    const std::vector<std::string>& keys = (*complete_json)[i].getMemberNames();
    for (Json::ArrayIndex j = 0; j < keys.size(); ++j) {
      const std::string& key = keys[j];
      if (key == kImages) {
        (*complete_json)[i].removeMember(key);
      } else if (key != kInstanceHtml) {
        DeleteImagesFromJson(&(*complete_json)[i][key]);
      }
    }
  }
}

bool ComputePanels(const PanelSet* panel_set_,
                   PanelIdToSpecMap* panel_id_to_spec) {
  bool non_cacheable_present = false;
  for (int i = 0; i < panel_set_->panels_size(); ++i) {
    const Panel& panel = panel_set_->panels(i);
    const GoogleString panel_id = StrCat(kPanelId, ".", IntegerToString(i));
    non_cacheable_present |= (panel.cacheability_in_minutes() == 0);
    (*panel_id_to_spec)[panel_id] = &panel;
  }
  return non_cacheable_present;
}

void EscapeString(GoogleString* str) {
  GlobalReplaceSubstring("<", "__psa_lt;", str);
  GlobalReplaceSubstring(">", "__psa_gt;", str);
}

bool StripTrailingNewline(GoogleString* s) {
  if (!s->empty() && (*s)[s->size() - 1] == '\n') {
    if (s->size() > 1 && (*s)[s->size() - 2] == '\r')
      s->resize(s->size() - 2);
    else
      s->resize(s->size() - 1);
    return true;
  }
  return false;
}

StringPiece GetNonCacheableElements(
    const GoogleString& atf_non_cacheable_elements, const GoogleUrl& url) {
  StringPieceVector url_family_non_cacheable_elements;
  SplitStringPieceToVector(atf_non_cacheable_elements,
                           ";", &url_family_non_cacheable_elements, true);
  for (size_t i = 0; i < url_family_non_cacheable_elements.size(); ++i) {
    StringPieceVector url_family_non_cacheable_elements_pair;
    SplitStringPieceToVector(url_family_non_cacheable_elements[i], ":",
                             &url_family_non_cacheable_elements_pair, true);
    if (url_family_non_cacheable_elements_pair.size() != 2) {
      LOG(ERROR) << "Incorrect non cacheable element value "
                 << url_family_non_cacheable_elements[i];
      return "";
    }
    Wildcard wildcard(url_family_non_cacheable_elements_pair[0]);
    if (wildcard.Match(url.PathAndLeaf())) {
      return url_family_non_cacheable_elements_pair[1];
    }
  }
  return "";
}

void PopulateAttributeToNonCacheableValuesMap(
    const RewriteOptions* rewrite_options, const GoogleUrl& url,
    AttributesToNonCacheableValuesMap* attribute_non_cacheable_values_map,
    std::vector<int>* panel_number_num_instances) {
  GoogleString non_cacheable_elements_str =
      rewrite_options->GetBlinkNonCacheableElementsFor(url.PathAndLeaf());
  StringPiece non_cacheable_elements(non_cacheable_elements_str);
  if (non_cacheable_elements.empty()) {
    non_cacheable_elements = GetNonCacheableElements(
        rewrite_options->prioritize_visible_content_non_cacheable_elements(),
        url);
  }
  // TODO(rahulbansal): Add more error checking.
  StringPieceVector non_cacheable_values;
  SplitStringPieceToVector(non_cacheable_elements,
                           ",", &non_cacheable_values, true);
  for (size_t i = 0; i < non_cacheable_values.size(); ++i) {
    StringPieceVector non_cacheable_values_pair;
    SplitStringPieceToVector(non_cacheable_values[i], "=",
                             &non_cacheable_values_pair, true);
    if (non_cacheable_values_pair.size() != 2) {
      LOG(ERROR) << "Incorrect non cacheable element value " <<
          non_cacheable_values[i];
      return;
    }
    attribute_non_cacheable_values_map->insert(make_pair(
        non_cacheable_values_pair[0].as_string(),
        make_pair(non_cacheable_values_pair[1].as_string(), i)));
    panel_number_num_instances->push_back(0);
  }
}

int GetPanelNumberForNonCacheableElement(
    const AttributesToNonCacheableValuesMap&
        attribute_non_cacheable_values_map,
    const HtmlElement* element) {
  for (int i = 0; i < element->attribute_size(); ++i) {
    const HtmlElement::Attribute& attribute = element->attribute(i);
    StringPiece value = attribute.DecodedValueOrNull();
    if (value.empty()) {
      continue;
    }
    std::pair<AttributesToNonCacheableValuesMap::const_iterator,
        AttributesToNonCacheableValuesMap::const_iterator> ret =
            attribute_non_cacheable_values_map.equal_range(
                attribute.name().c_str());
    AttributesToNonCacheableValuesMap::const_iterator it;
    for (it = ret.first; it != ret.second; ++it) {
      if ((it->first == attribute.name().c_str()) &&
          (value == it->second.first)) {
        return it->second.second;
      }
    }
  }
  return -1;
}

GoogleString GetPanelId(int panel_number, int instance_number) {
  return StrCat(BlinkUtil::kPanelId, "-", IntegerToString(panel_number),
                ".", IntegerToString(instance_number));
}

}  // namespace BlinkUtil
}  // namespace net_instaweb
