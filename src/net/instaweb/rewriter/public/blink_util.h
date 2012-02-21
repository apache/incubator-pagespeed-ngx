// Copyright 2011 Google Inc. All Rights Reserved.
// Author: gagansingh@google.com (Gagan Singh)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_BLINK_UTIL_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_BLINK_UTIL_H_

#include <map>

#include "net/instaweb/util/public/string.h"
#include "third_party/jsoncpp/include/json/json.h"

namespace net_instaweb {

class GoogleUrl;
class Layout;
class Panel;
class PanelSet;
class PublisherConfig;
class RewriteOptions;

typedef std::map<GoogleString, const Panel*> PanelIdToSpecMap;

namespace BlinkUtil {

const char kContiguous[] = "contiguous";
const char kCritical[] = "critical";
const char kPanelId[] = "panel-id";
const char kImages[] = "images";
const char kInstanceHtml[] = "instance_html";
const char kLayoutMarker[] = "<!--GooglePanel **** Layout end ****-->";
const char kJsonCachePrefix[] = "json:";

// Checks whether the request for 'url' is a valid blink request. If yes,
// returns a pointer to the corresponding Layout, and NULL otherwise.
// TODO(sriharis): Split the check part and extracting the layout into separate
// functions.
const Layout* ExtractBlinkLayout(const GoogleUrl& url, RewriteOptions* options);

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
}  // namespace BlinkUtil

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_BLINK_UTIL_H_
