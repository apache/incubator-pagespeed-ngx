/**
 * Copyright 2010 Google Inc.
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

#include "net/instaweb/rewriter/public/rewrite_options.h"

#include <vector>
#include "net/instaweb/util/public/message_handler.h"

namespace net_instaweb {

// TODO(jmarantz): consider merging this threshold with the image-inlining
// threshold, which is currently defaulting at 2000, so we have a single
// byte-count threshold, above which inlined resources get outlined, and
// below which outlined resources get inlined.
//
// TODO(jmarantz): user-agent-specific selection of inline threshold so that
// mobile phones are more prone to inlining.
//
// Further notes; jmaessen says:
//
// I suspect we do not want these bounds to match, and inlining for
// images is a bit more complicated because base64 encoding inflates
// the byte count of data: urls.  This is a non-issue for other
// resources (there may be some weirdness with iframes I haven't
// thought about...).
//
// jmarantz says:
//
// One thing we could do, if we believe they should be conceptually
// merged, is in img_rewrite_filter you could apply the
// base64-bloat-factor before comparing against the threshold.  Then
// we could use one number if we like that idea.
//

// jmaessen: For the moment, there's a separate threshold for img inline.
const int64 RewriteOptions::kDefaultCssInlineMaxBytes = 2048;
const int64 RewriteOptions::kDefaultImgInlineMaxBytes = 2048;
const int64 RewriteOptions::kDefaultJsInlineMaxBytes = 2048;
const int64 RewriteOptions::kDefaultOutlineThreshold = 3000;
const std::string RewriteOptions::kDefaultBeaconUrl =
    "/mod_pagespeed_beacon?ets=";

RewriteOptions::RewriteOptions()
    : css_inline_max_bytes_(kDefaultCssInlineMaxBytes),
      img_inline_max_bytes_(kDefaultImgInlineMaxBytes),
      js_inline_max_bytes_(kDefaultJsInlineMaxBytes),
      outline_threshold_(kDefaultOutlineThreshold),
      num_shards_(0),
      beacon_url_(kDefaultBeaconUrl) {
  name_filter_map_["add_base_tag"] = kAddBaseTag;
  name_filter_map_["add_head"] = kAddHead;
  name_filter_map_["add_instrumentation"] = kAddInstrumentation;
  name_filter_map_["collapse_whitespace"] = kCollapseWhitespace;
  name_filter_map_["combine_css"] = kCombineCss;
  name_filter_map_["combine_heads"] = kCombineHeads;
  name_filter_map_["debug_log_img_tags"] = kDebugLogImgTags;
  name_filter_map_["elide_attributes"] = kElideAttributes;
  name_filter_map_["extend_cache"] = kExtendCache;
  name_filter_map_["inline_css"] = kInlineCss;
  name_filter_map_["inline_javascript"] = kInlineJavascript;
  name_filter_map_["insert_img_dimensions"] = kInsertImgDimensions;
  name_filter_map_["left_trim_urls"] = kLeftTrimUrls;
  name_filter_map_["move_css_to_head"] = kMoveCssToHead;
  name_filter_map_["outline_css"] = kOutlineCss;
  name_filter_map_["outline_javascript"] = kOutlineJavascript;
  name_filter_map_["remove_comments"] = kRemoveComments;
  name_filter_map_["remove_quotes"] = kRemoveQuotes;
  name_filter_map_["rewrite_css"] = kRewriteCss;
  name_filter_map_["rewrite_images"] = kRewriteImages;
  name_filter_map_["rewrite_javascript"] = kRewriteJavascript;
  name_filter_map_["strip_scripts"] = kStripScripts;
}

bool RewriteOptions::AddFiltersByCommaSeparatedList(
    const StringPiece& filters, MessageHandler* handler) {
  std::vector<StringPiece> names;
  SplitStringPieceToVector(filters, ",", &names, true);
  bool ret = true;
  for (int i = 0, n = names.size(); i < n; ++i) {
    std::string option(names[i].data(), names[i].size());
    NameFilterMap::iterator p = name_filter_map_.find(option);
    if (p == name_filter_map_.end()) {
      handler->Message(kWarning, "Invalid filter name: %s", option.c_str());
      ret = false;
    } else {
      AddFilter(p->second);
    }
  }
  return ret;
}

void RewriteOptions::AddFilter(Filter filter) {
  filters_.insert(filter);
}

bool RewriteOptions::Enabled(Filter filter) const {
  return (filters_.find(filter) != filters_.end());
}

}  // namespace net_instaweb
