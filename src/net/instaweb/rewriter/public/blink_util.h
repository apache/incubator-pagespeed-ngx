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
class Panel;
class PanelSet;
class RequestHeaders;
class ServerContext;
class RewriteOptions;
class UserAgentMatcher;

typedef std::map<GoogleString, const Panel*> PanelIdToSpecMap;
typedef std::multimap<GoogleString, std::pair<GoogleString, const int>,
        StringCompareInsensitive> AttributesToNonCacheableValuesMap;

namespace BlinkUtil {

const char kContiguous[] = "contiguous";
const char kCritical[] = "critical";
const char kPanelId[] = "panel-id";
const char kImages[] = "images";
const char kInstanceHtml[] = "instance_html";
const char kStartBodyMarker[] = "<!--GooglePanel **** Start body ****-->";
const char kEndBodyTag[] = "</body>";
const char kLayoutMarker[] = "<!--GooglePanel **** Layout end ****-->";
const char kJsonCachePrefix[] = "json:";
const char kBlinkResponseCodePropertyName[] = "blink_last_response_code";
const char kXpath[] = "xpath";
// TODO(rahulbansal): Use these constants everywhere in the code from here.
const char kBlinkCohort[] = "blink";
const char kBlinkCriticalLineDataPropertyName[] = "blink_critical_line_data";

// Checks whether the request for 'url' is a valid blink request.
bool IsBlinkRequest(const GoogleUrl& url,
                    const RequestHeaders* request_headers,
                    const RewriteOptions* options,
                    const char* user_agent,
                    const UserAgentMatcher& user_agent_matcher_);

// Checks if blink critical line flow can be applied.
bool ShouldApplyBlinkFlowCriticalLine(
    const ServerContext* manager,
    const RewriteOptions* options);

// Returns true if json has only miscellaneous(like 'contiguous')
// atributes.
bool IsJsonEmpty(const Json::Value& json);

// Clears the json array if all objects are empty.
void ClearArrayIfAllEmpty(Json::Value* json);

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
    const RewriteOptions* rewrite_options,
    const GoogleUrl& url,
    AttributesToNonCacheableValuesMap* attribute_non_cacheable_values_map,
    std::vector<int>* panel_number_num_instances);

// Returns panel number for non cacheable element. If cacheable returns -1.
int GetPanelNumberForNonCacheableElement(
    const AttributesToNonCacheableValuesMap& attribute_non_cacheable_values_map,
    const HtmlElement* element);

// Gets panel id for the given panel instance.
GoogleString GetPanelId(int panel_number, int instance_number);
}  // namespace BlinkUtil

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_BLINK_UTIL_H_
