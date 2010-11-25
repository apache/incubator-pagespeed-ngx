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

#include <map>
#include <set>
#include "base/basictypes.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;

class RewriteOptions {
 public:
  enum Filter {
    kAddBaseTag,
    kAddHead,
    kAddInstrumentation,
    kCollapseWhitespace,
    kCombineCss,
    kCombineHeads,
    kDebugLogImgTags,
    kElideAttributes,
    kExtendCache,
    kInlineCss,
    kInlineJavascript,
    kInsertImgDimensions,
    kLeftTrimUrls,
    kMoveCssToHead,
    kOutlineCss,
    kOutlineJavascript,
    kRemoveComments,
    kRemoveQuotes,
    kRewriteCss,
    kRewriteImages,
    kRewriteJavascript,
    kStripScripts,
  };

  enum RewriteLevel {
    // Run in pass-through mode. Parse HTML but do not perform any
    // transformations. This is the default value. Most users should
    // explcitly enable the kCoreFilters level by calling
    // SetRewriteLevel(kCoreFilters).
    kPassThrough,

    // Enable the core set of filters. These filters are considered
    // generally safe for most sites, though even safe filters can
    // break some sites. Most users should specify this option, and
    // then optionally add or remove specific filters based on
    // specific needs.
    kCoreFilters,
  };

  // Used for enumerating over all entries in the Filter enum.
  static const Filter kFirstFilter = kAddBaseTag;
  static const Filter kLastFilter = kStripScripts;

  static const int64 kDefaultCssInlineMaxBytes;
  static const int64 kDefaultImgInlineMaxBytes;
  static const int64 kDefaultJsInlineMaxBytes;
  static const int64 kDefaultCssOutlineMinBytes;
  static const int64 kDefaultJsOutlineMinBytes;
  static const std::string kDefaultBeaconUrl;

  // IE limits URL size overall to about 2k characters.  See
  // http://support.microsoft.com/kb/208427/EN-US
  static const int kMaxUrlSize;

  static const int kDefaultImgMaxRewritesAtOnce;

  // See http://code.google.com/p/modpagespeed/issues/detail?id=9
  // Apache evidently limits each URL path segment (between /) to
  // about 256 characters.  This is not fundamental URL limitation
  // but is Apache specific.
  static const int64 kMaxUrlSegmentSize;

  static bool ParseRewriteLevel(const StringPiece& in, RewriteLevel* out);

  RewriteOptions();

  void SetDefaultRewriteLevel(RewriteLevel level) { level_.set_default(level); }
  void SetRewriteLevel(RewriteLevel level) { level_.set(level); }
  RewriteLevel level() const { return level_.value();}

  // Adds a set of filters to the enabled set.  Returns false if any
  // of the filter names are invalid, but all the valid ones will be
  // added anyway.
  bool EnableFiltersByCommaSeparatedList(const StringPiece& filters,
                                         MessageHandler* handler);

  // Adds a set of filters to the disabled set.  Returns false if any
  // of the filter names are invalid, but all the valid ones will be
  // added anyway.
  bool DisableFiltersByCommaSeparatedList(const StringPiece& filters,
                                          MessageHandler* handler);
  void EnableFilter(Filter filter) { enabled_filters_.insert(filter); }
  void DisableFilter(Filter filter) { disabled_filters_.insert(filter); }

  bool Enabled(Filter filter) const;

  // TODO(jmarantz): consider setting flags in the set_ methods so that
  // first's explicit settings can override default values from second.

  int64 css_outline_min_bytes() const { return css_outline_min_bytes_.value(); }
  void set_css_outline_min_bytes(int64 x) { css_outline_min_bytes_.set(x); }
  int64 js_outline_min_bytes() const { return js_outline_min_bytes_.value(); }
  void set_js_outline_min_bytes(int64 x) { js_outline_min_bytes_.set(x); }
  int64 img_inline_max_bytes() const { return img_inline_max_bytes_.value(); }
  void set_img_inline_max_bytes(int64 x) { img_inline_max_bytes_.set(x); }
  int64 css_inline_max_bytes() const { return css_inline_max_bytes_.value(); }
  void set_css_inline_max_bytes(int64 x) { css_inline_max_bytes_.set(x); }
  int64 js_inline_max_bytes() const { return js_inline_max_bytes_.value(); }
  void set_js_inline_max_bytes(int64 x) { js_inline_max_bytes_.set(x); }
  int num_shards() const { return num_shards_.value(); }
  void set_num_shards(int x) { num_shards_.set(x); }
  const std::string& beacon_url() const { return beacon_url_.value(); }
  void set_beacon_url(const StringPiece& p) {
    beacon_url_.set(std::string(p.data(), p.size()));
  }

  // The maximum length of a URL segment.
  // for http://a/b/c.d, this is == strlen("c.d")
  int max_url_segment_size() const { return max_url_segment_size_.value(); }
  void set_max_url_segment_size(int x) { max_url_segment_size_.set(x); }

  int img_max_rewrites_at_once() const {
    return img_max_rewrites_at_once_.value();
  }
  void set_img_max_rewrites_at_once(int x) { img_max_rewrites_at_once_.set(x); }

  // The maximum size of the entire URL.  If '0', this is left unlimited.
  int max_url_size() const { return max_url_size_.value(); }
  void set_max_url_size(int x) { max_url_size_.set(x); }

  void set_enabled(bool x) { enabled_.set(x); }
  bool enabled() const { return enabled_.value(); }

  // Merge together two source RewriteOptions to populate this.  The order
  // is significant: the second will override the first.  One semantic
  // subject to interpretation is when a core-filter is disabled in the
  // first set and not in the second.  In this case, my judgement is that
  // the 'disable' from the first should override the core-set membership
  // in the second, but not an 'enable' in the second.
  void Merge(const RewriteOptions& first, const RewriteOptions& second);

 private:
  // Helper class to represent an Option, whose value is held in some class T.
  // An option is explicitly initialized with its default value, although the
  // default value can be altered later.  It keeps track of whether the a
  // value has been explicitly set (independent of whether that happens to
  // coincide with the default value).
  //
  // It can use this knowledge to intelligently merge a 'base' option value
  // into a 'new' option value, allowing explicitly set values from 'base'
  // override default values from 'new'.
  template<class T> class Option {
   public:
    explicit Option(const T& default_value)
        : value_(default_value),
          was_set_(false) {
    }

    void set(const T& val) {
      was_set_ = true;
      value_ = val;
    }

    void set_default(const T& val) {
      if (!was_set_) {
        value_ = val;
      }
    }

    const T& value() const { return value_; }

    void Merge(const Option& one, const Option& two) {
      if (two.was_set_ || !one.was_set_) {
        value_ = two.value_;
        was_set_ = two.was_set_;
      } else {
        value_ = one.value_;
        was_set_ = true;  // this stmt is reached only if one.was_set_==true
      }
    }

   private:
    T value_;
    bool was_set_;

    DISALLOW_COPY_AND_ASSIGN(Option);
  };

  typedef std::set<Filter> FilterSet;
  typedef std::map<std::string, Filter> NameToFilterMap;
  typedef std::map<RewriteLevel, FilterSet> RewriteLevelToFilterSetMap;

  void SetUp();
  bool AddCommaSeparatedListToFilterSet(
      const StringPiece& filters, MessageHandler* handler, FilterSet* set);

  NameToFilterMap name_filter_map_;
  RewriteLevelToFilterSetMap level_filter_set_map_;
  FilterSet enabled_filters_;
  FilterSet disabled_filters_;

  // Note: using the template class Option here saves a lot of repeated
  // and error-prone merging code.  However, if is not space efficient as
  // we are alternating int64s and bools in the structure.  If we cared
  // about that, then we would keep aa bools in a bitmask.  But since
  // we don't really care we'll try to keep the code structured better.
  Option<RewriteLevel> level_;
  Option<int64> css_inline_max_bytes_;
  Option<int64> img_inline_max_bytes_;
  Option<int64> img_max_rewrites_at_once_;
  Option<int64> js_inline_max_bytes_;
  Option<int64> css_outline_min_bytes_;
  Option<int64> js_outline_min_bytes_;
  Option<int> num_shards_;
  Option<std::string> beacon_url_;
  Option<int> max_url_segment_size_;  // for http://a/b/c.d, use strlen("c.d")
  Option<int> max_url_size_;          // but this is strlen("http://a/b/c.d")
  Option<bool> enabled_;
  // Be sure to update Merge() if a new field is added.

  DISALLOW_COPY_AND_ASSIGN(RewriteOptions);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_OPTIONS_H_
