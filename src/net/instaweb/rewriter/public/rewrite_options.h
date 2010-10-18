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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_OPTIONS_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_OPTIONS_H_

#include "base/basictypes.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class RewriteOptions {
 public:
  static const char kAddBaseTag[];
  static const char kAddHead[];
  static const char kAddInstrumentation[];
  static const char kCollapseWhitespace[];
  static const char kCombineCss[];
  static const char kDebugLogImgTags[];
  static const char kElideAttributes[];
  static const char kExtendCache[];
  static const char kInlineCss[];
  static const char kInlineJavascript[];
  static const char kInsertImgDimensions[];
  static const char kLeftTrimUrls[];
  static const char kMoveCssToHead[];
  static const char kOutlineCss[];
  static const char kOutlineJavascript[];
  static const char kRemoveComments[];
  static const char kRemoveQuotes[];
  static const char kRewriteCss[];
  static const char kRewriteImages[];
  static const char kRewriteJavascript[];
  static const char kStripScripts[];

  static const int64 kDefaultCssInlineMaxBytes;
  static const int64 kDefaultImgInlineMaxBytes;
  static const int64 kDefaultJsInlineMaxBytes;
  static const int64 kDefaultOutlineThreshold;

  RewriteOptions();

  void AddFiltersByCommaSeparatedList(const StringPiece& filters);
  void AddFilters(const StringSet& filters) {
    filters_.insert(filters.begin(), filters.end());
  }
  void ClearFilters() { filters_.clear(); }
  void AddFilter(const StringPiece& filter);

  bool Enabled(const StringPiece& filter_name) const;

  int64 outline_threshold() const { return outline_threshold_; }
  void set_outline_threshold(int64 x) { outline_threshold_ = x; }
  int64 img_inline_max_bytes() const { return img_inline_max_bytes_; }
  void set_img_inline_max_bytes(int64 x) { img_inline_max_bytes_ = x; }
  int64 css_inline_max_bytes() const { return css_inline_max_bytes_; }
  void set_css_inline_max_bytes(int64 x) { css_inline_max_bytes_ = x; }
  int64 js_inline_max_bytes() const { return js_inline_max_bytes_; }
  void set_js_inline_max_bytes(int64 x) { js_inline_max_bytes_ = x; }
  int num_shards() const { return num_shards_; }
  void set_num_shards(int x) { num_shards_ = x; }

 public:
  StringSet all_filters_;  // used for checking against misspelled options
  StringSet filters_;
  int64 css_inline_max_bytes_;
  int64 img_inline_max_bytes_;
  int64 js_inline_max_bytes_;
  int64 outline_threshold_;
  int num_shards_;

  DISALLOW_COPY_AND_ASSIGN(RewriteOptions);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_OPTIONS_H_
