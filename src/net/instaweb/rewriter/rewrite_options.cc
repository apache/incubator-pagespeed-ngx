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

namespace net_instaweb {

const char RewriteOptions::kAddBaseTag[] = "add_base_tag";
const char RewriteOptions::kAddHead[] = "add_head";
const char RewriteOptions::kAddInstrumentation[] = "add_instrumentation";
const char RewriteOptions::kCollapseWhitespace[] = "collapse_whitespace";
const char RewriteOptions::kCombineCss[] = "combine_css";
const char RewriteOptions::kDebugLogImgTags[] = "debug_log_img_tags";
const char RewriteOptions::kElideAttributes[] = "elide_attributes";
const char RewriteOptions::kExtendCache[] = "extend_cache";
const char RewriteOptions::kInlineCss[] = "inline_css";
const char RewriteOptions::kInlineJavascript[] = "inline_javascript";
const char RewriteOptions::kInsertImgDimensions[] = "insert_img_dimensions";
const char RewriteOptions::kLeftTrimUrls[] = "left_trim_urls";
const char RewriteOptions::kMoveCssToHead[] = "move_css_to_head";
const char RewriteOptions::kOutlineCss[] = "outline_css";
const char RewriteOptions::kOutlineJavascript[] = "outline_javascript";
const char RewriteOptions::kRemoveComments[] = "remove_comments";
const char RewriteOptions::kRemoveQuotes[] = "remove_quotes";
const char RewriteOptions::kRewriteCss[] = "rewrite_css";
const char RewriteOptions::kRewriteImages[] = "rewrite_images";
const char RewriteOptions::kRewriteJavascript[] = "rewrite_javascript";
const char RewriteOptions::kStripScripts[] = "strip_scripts";

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

RewriteOptions::RewriteOptions()
    : css_inline_max_bytes_(kDefaultCssInlineMaxBytes),
      img_inline_max_bytes_(kDefaultImgInlineMaxBytes),
      js_inline_max_bytes_(kDefaultJsInlineMaxBytes),
      outline_threshold_(kDefaultOutlineThreshold),
      num_shards_(0) {
  all_filters_.insert(kAddBaseTag);
  all_filters_.insert(kAddHead);
  all_filters_.insert(kAddInstrumentation);
  all_filters_.insert(kCollapseWhitespace);
  all_filters_.insert(kCombineCss);
  all_filters_.insert(kDebugLogImgTags);
  all_filters_.insert(kElideAttributes);
  all_filters_.insert(kExtendCache);
  all_filters_.insert(kInlineCss);
  all_filters_.insert(kInlineJavascript);
  all_filters_.insert(kInsertImgDimensions);
  all_filters_.insert(kLeftTrimUrls);
  all_filters_.insert(kMoveCssToHead);
  all_filters_.insert(kOutlineCss);
  all_filters_.insert(kOutlineJavascript);
  all_filters_.insert(kRemoveComments);
  all_filters_.insert(kRemoveQuotes);
  all_filters_.insert(kRewriteCss);
  all_filters_.insert(kRewriteImages);
  all_filters_.insert(kRewriteJavascript);
  all_filters_.insert(kStripScripts);
}

void RewriteOptions::AddFiltersByCommaSeparatedList(
    const StringPiece& filters) {
  std::vector<StringPiece> names;
  SplitStringPieceToVector(filters, ",", &names, true);
  for (int i = 0, n = names.size(); i < n; ++i) {
    AddFilter(names[i]);
  }
}

void RewriteOptions::AddFilter(const StringPiece& filter) {
  std::string option(filter.data(), filter.size());

  // TODO(jmarantz): consider returning a bool and taking a message
  // handler to make this a more user-friendly error message.
  CHECK(all_filters_.find(option) != all_filters_.end());
  filters_.insert(option);
}

bool RewriteOptions::Enabled(const StringPiece& filter) const {
  return (filters_.find(filter.as_string()) != filters_.end());
}

}  // namespace net_instaweb
