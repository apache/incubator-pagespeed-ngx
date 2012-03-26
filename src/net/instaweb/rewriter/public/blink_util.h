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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_BLINK_UTIL_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_BLINK_UTIL_H_

#include <map>
#include <utility>
#include <vector>

#include "net/instaweb/util/public/json.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class GoogleUrl;
class HtmlElement;
class Layout;
class Panel;
class PanelSet;
class ProxyFetchPropertyCallbackCollector;
class PublisherConfig;
class RequestHeaders;
class ResourceManager;
class RewriteOptions;
class UserAgentMatcher;

typedef std::map<GoogleString, const Panel*> PanelIdToSpecMap;
typedef std::multimap<GoogleString, std::pair<GoogleString, const int> >
    AttributesToNonCacheableValuesMap;

namespace BlinkUtil {

const char kContiguous[] = "contiguous";
const char kCritical[] = "critical";
const char kPanelId[] = "panel-id";
const char kImages[] = "images";
const char kInstanceHtml[] = "instance_html";
const char kLayoutMarker[] = "<!--GooglePanel **** Layout end ****-->";
const char kJsonCachePrefix[] = "json:";

// Checks whether the request for 'url' is a valid blink request.
bool IsBlinkRequest(const GoogleUrl& url,
                    const RequestHeaders* request_headers,
                    const RewriteOptions* options,
                    const char* user_agent,
                    const UserAgentMatcher& user_agent_matcher_);

// Checks if blink critical line flow can be applied.
bool ShouldApplyBlinkFlowCriticalLine(
    const ResourceManager* manager,
    const ProxyFetchPropertyCallbackCollector* property_callback,
    const RewriteOptions* options);

// Returns a pointer to the corresponding Layout, and NULL otherwise.
const Layout* ExtractBlinkLayout(const GoogleUrl& url, RewriteOptions* options,
                                 const StringPiece& user_agent);

// Finds the layout for the given request_url.
const Layout* FindLayout(const PublisherConfig& config,
                         const GoogleUrl& request_url);

// Splits complete json into critical and non-critical and stores
// them in corresponding member strings. must be called with mutex_ held.
void SplitCritical(const Json::Value& complete_json,
                   const PanelIdToSpecMap& panel_id_to_spec,
                   GoogleString* critical_json_str,
                   GoogleString* non_critical_json_str,
                   GoogleString* pushed_images_str);

// Splits complete json array into critical, non-cacheable and non-cacheable
// arrays.
void SplitCriticalArray(const Json::Value& complete_json,
                        const PanelIdToSpecMap& panel_id_to_spec,
                        Json::Value* critical_json,
                        Json::Value* non_cacheable_json,
                        Json::Value* non_critical_json,
                        bool panel_valid,
                        int num_critical_instances,
                        Json::Value* pushed_images);
// Splits complete json object into critical, non-cacheable and non-cacheable
// objects.
void SplitCriticalObj(const Json::Value& json_obj,
                      const PanelIdToSpecMap& panel_id_to_spec,
                      Json::Value* critical_obj,
                      Json::Value* non_cacheable_obj,
                      Json::Value* non_critical_obj,
                      bool panel_cacheable,
                      Json::Value* pushed_images);
// Returns true if json has only miscellaneous(like 'contiguous')
// atributes.
bool IsJsonEmpty(const Json::Value& json);

// Clears the json array if all objects are empty.
void ClearArrayIfAllEmpty(Json::Value* json);

// Deletes images from given json.
void DeleteImagesFromJson(Json::Value* json);

// Computes panel id to specification map and returns if any non cacheable
// panels are present.
bool ComputePanels(const PanelSet* panel_set_,
                   PanelIdToSpecMap* panel_id_to_spec);

// Escapes < and > with __psa_lt; and __psa_gt; respectively.
void EscapeString(GoogleString* str);

// TODO(rahulbansal): Move this function to net/instaweb/util/string_util
bool StripTrailingNewline(GoogleString* s);

// Populates the attributes to non cacheable values map.
void PopulateAttributeToNonCacheableValuesMap(
    const GoogleString& atf_non_cacheable_elements,
    AttributesToNonCacheableValuesMap* attribute_non_cacheable_values_map,
    std::vector<int>* panel_number_num_instances);

// Returns panel number for non cacheable element. If cacheable returns -1.
int GetPanelNumberForNonCacheableElement(
    const AttributesToNonCacheableValuesMap&
        attribute_non_cacheable_values_map,
    const HtmlElement* element);

// Gets panel id for the given panel instance.
GoogleString GetPanelId(int panel_number, int instance_number);
}  // namespace BlinkUtil

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_BLINK_UTIL_H_
