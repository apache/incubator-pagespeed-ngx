/*
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

#include <set>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/wildcard_group.h"

namespace net_instaweb {

class Hasher;
class MessageHandler;

class RewriteOptions {
 public:
  // If you add or remove anything from this list, you need to update the
  // version number in rewrite_options.cc.
  enum Filter {
    kAddHead,  // Update kFirstFilter if you add something before this.
    kAddInstrumentation,
    kCollapseWhitespace,
    kCombineCss,
    kCombineHeads,
    kCombineJavascript,
    kConvertJpegToWebp,
    kConvertMetaTags,
    kDivStructure,
    kElideAttributes,
    kExtendCache,
    kInlineCss,
    kInlineImages,
    kInlineJavascript,
    kInsertImageDimensions,
    kLeftTrimUrls,
    kMakeGoogleAnalyticsAsync,
    kMoveCssToHead,
    kOutlineCss,
    kOutlineJavascript,
    kRecompressImages,
    kRemoveComments,
    kRemoveQuotes,
    kResizeImages,
    kRewriteCss,
    kRewriteDomains,
    kRewriteJavascript,
    kRewriteStyleAttributes,
    kRewriteStyleAttributesWithUrl,
    kSpriteImages,
    kStripScripts,
    kEndOfFilters
  };

  // Used for enumerating over all entries in the Filter enum.
  static const Filter kFirstFilter = kAddHead;

  // Convenience name for a set of rewrite filters.
  typedef std::set<Filter> FilterSet;

  enum RewriteLevel {
    // Enable no filters. Parse HTML but do not perform any
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

    // Enable all filters intended for core, but some of which might
    // need more testing. Good for if users are willing to test out
    // the results of the rewrite more closely.
    kTestingCoreFilters,

    // Enable all filters. This includes filters you should never turn
    // on for a real page, like StripScripts!
    kAllFilters,
  };

  static const int64 kDefaultCssInlineMaxBytes;
  static const int64 kDefaultImageInlineMaxBytes;
  static const int64 kDefaultJsInlineMaxBytes;
  static const int64 kDefaultCssOutlineMinBytes;
  static const int64 kDefaultJsOutlineMinBytes;
  static const int64 kDefaultMaxHtmlCacheTimeMs;
  static const int64 kDefaultMinResourceCacheTimeToRewriteMs;
  static const int64 kDefaultCacheInvalidationTimestamp;
  static const int64 kDefaultIdleFlushTimeMs;
  static const GoogleString kDefaultBeaconUrl;

  // IE limits URL size overall to about 2k characters.  See
  // http://support.microsoft.com/kb/208427/EN-US
  static const int kMaxUrlSize;

  static const int kDefaultImageMaxRewritesAtOnce;

  // See http://code.google.com/p/modpagespeed/issues/detail?id=9
  // Apache evidently limits each URL path segment (between /) to
  // about 256 characters.  This is not fundamental URL limitation
  // but is Apache specific.
  static const int kDefaultMaxUrlSegmentSize;

  static const char kClassName[];

  static bool ParseRewriteLevel(const StringPiece& in, RewriteLevel* out);

  RewriteOptions();
  virtual ~RewriteOptions();

  bool modified() const { return modified_; }

  void SetDefaultRewriteLevel(RewriteLevel level) {
    // Do not set the modified bit -- we are only changing the default.
    level_.set_default(level);
  }
  void SetRewriteLevel(RewriteLevel level) {
    set_option(level, &level_);
  }
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

  // Explicitly disable all filters which are not *currently* explicitly enabled
  //
  // Note: Do not call EnableFilter(...) for this options object after calling
  // DisableAllFilters..., because the Disable list will not be auto-updated.
  //
  // Used to deal with query param ?ModPagespeedFilter=foo
  // Which implies that all filters not listed should be disabled.
  void DisableAllFiltersNotExplicitlyEnabled();

  void EnableFilter(Filter filter);
  void DisableFilter(Filter filter);
  void EnableFilters(const FilterSet& filter_set);
  void DisableFilters(const FilterSet& filter_set);

  bool Enabled(Filter filter) const;

  // TODO(jmarantz): consider setting flags in the set_ methods so that
  // first's explicit settings can override default values from second.

  int64 css_outline_min_bytes() const { return css_outline_min_bytes_.value(); }
  void set_css_outline_min_bytes(int64 x) {
    set_option(x, &css_outline_min_bytes_);
  }
  int64 js_outline_min_bytes() const { return js_outline_min_bytes_.value(); }
  void set_js_outline_min_bytes(int64 x) {
    set_option(x, &js_outline_min_bytes_);
  }
  int64 image_inline_max_bytes() const {
    return image_inline_max_bytes_.value();
  }
  void set_image_inline_max_bytes(int64 x) {
    set_option(x, &image_inline_max_bytes_);
  }
  int64 css_inline_max_bytes() const { return css_inline_max_bytes_.value(); }
  void set_css_inline_max_bytes(int64 x) {
    set_option(x, &css_inline_max_bytes_);
  }
  int64 js_inline_max_bytes() const { return js_inline_max_bytes_.value(); }
  void set_js_inline_max_bytes(int64 x) {
    set_option(x, &js_inline_max_bytes_);
  }
  int64 max_html_cache_time_ms() const {
    return max_html_cache_time_ms_.value();
  }
  void set_max_html_cache_time_ms(int64 x) {
    set_option(x, &max_html_cache_time_ms_);
  }
  int64 min_resource_cache_time_to_rewrite_ms() const {
    return min_resource_cache_time_to_rewrite_ms_.value();
  }
  void set_min_resource_cache_time_to_rewrite_ms(int64 x) {
    set_option(x, &min_resource_cache_time_to_rewrite_ms_);
  }

  // Cache invalidation timestamp is in milliseconds since 1970.
  void set_cache_invalidation_timestamp(int64 x) {
    set_option(x, &cache_invalidation_timestamp_);
  }
  int64 cache_invalidation_timestamp() const {
    return cache_invalidation_timestamp_.value();
  }

  // How much inactivity of HTML input will result in PSA introducing a flush.
  // Values <= 0 disable the feature.
  int64 idle_flush_time_ms() const {
    return idle_flush_time_ms_.value();
  }
  void set_idle_flush_time_ms(int64 x) {
    set_option(x, &idle_flush_time_ms_);
  }

  // The maximum length of a URL segment.
  // for http://a/b/c.d, this is == strlen("c.d")
  int max_url_segment_size() const { return max_url_segment_size_.value(); }
  void set_max_url_segment_size(int x) {
    set_option(x, &max_url_segment_size_);
  }

  int image_max_rewrites_at_once() const {
    return image_max_rewrites_at_once_.value();
  }
  void set_image_max_rewrites_at_once(int x) {
    set_option(x, &image_max_rewrites_at_once_);
  }

  // The maximum size of the entire URL.  If '0', this is left unlimited.
  int max_url_size() const { return max_url_size_.value(); }
  void set_max_url_size(int x) {
    set_option(x, &max_url_size_);
  }

  void set_enabled(bool x) {
    set_option(x, &enabled_);
  }
  bool enabled() const { return enabled_.value(); }

  void set_botdetect_enabled(bool x) {
    set_option(x, &botdetect_enabled_);
  }
  bool botdetect_enabled() const { return botdetect_enabled_.value(); }

  void set_combine_across_paths(bool x) {
    set_option(x, &combine_across_paths_);
  }
  bool combine_across_paths() const { return combine_across_paths_.value(); }

  void set_log_rewrite_timing(bool x) {
    set_option(x, &log_rewrite_timing_);
  }
  bool log_rewrite_timing() const { return log_rewrite_timing_.value(); }

  void set_lowercase_html_names(bool x) {
    set_option(x, &lowercase_html_names_);
  }
  bool lowercase_html_names() const { return lowercase_html_names_.value(); }

  void set_always_rewrite_css(bool x) {
    set_option(x, &always_rewrite_css_);
  }
  bool always_rewrite_css() const { return always_rewrite_css_.value(); }

  void set_respect_vary(bool x) {
    set_option(x, &respect_vary_);
  }
  bool respect_vary() const { return respect_vary_.value(); }

  void set_flush_html(bool x) { set_option(x, &flush_html_); }
  bool flush_html() const { return flush_html_.value(); }

  const GoogleString& beacon_url() const { return beacon_url_.value(); }
  void set_beacon_url(const StringPiece& p) {
    set_option(GoogleString(p.data(), p.size()), &beacon_url_);
  }

  // Return false in a subclass if you want to disallow all URL trimming in CSS.
  virtual bool trim_urls_in_css() const { return true; }

  // Merge together two source RewriteOptions to populate this.  The order
  // is significant: the second will override the first.  One semantic
  // subject to interpretation is when a core-filter is disabled in the
  // first set and not in the second.  In this case, my judgement is that
  // the 'disable' from the first should override the core-set membership
  // in the second, but not an 'enable' in the second.
  void Merge(const RewriteOptions& first, const RewriteOptions& second);

  // Registers a wildcard pattern for to be allowed, potentially overriding
  // previous Disallow wildcards.
  void Allow(const StringPiece& wildcard_pattern) {
    Modify();
    allow_resources_.Allow(wildcard_pattern);
  }

  // Registers a wildcard pattern for to be disallowed, potentially overriding
  // previous Allow wildcards.
  void Disallow(const StringPiece& wildcard_pattern) {
    Modify();
    allow_resources_.Disallow(wildcard_pattern);
  }

  // Blacklist of javascript files that don't like their names changed.
  // This should be called for root options to set defaults.
  void DisallowTroublesomeResources();

  DomainLawyer* domain_lawyer() { return &domain_lawyer_; }
  const DomainLawyer* domain_lawyer() const { return &domain_lawyer_; }

  FileLoadPolicy* file_load_policy() { return &file_load_policy_; }
  const FileLoadPolicy* file_load_policy() const { return &file_load_policy_; }

  // Determines, based on the sequence of Allow/Disallow calls above, whether
  // a url is allowed.
  bool IsAllowed(const StringPiece& url) const {
    return allow_resources_.Match(url, true);
  }

  // Adds a new comment wildcard pattern to be retained.
  void RetainComment(const StringPiece& comment) {
    Modify();
    retain_comments_.Allow(comment);
  }

  // If enabled, the 'remove_comments' filter will remove all HTML comments.
  // As discussed in Issue 237, some comments have semantic value and must
  // be retained.
  bool IsRetainedComment(const StringPiece& comment) const {
    return retain_comments_.Match(comment, false);
  }

  void CopyFrom(const RewriteOptions& src) {
    frozen_ = false;
    modified_ = false;
    Merge(src, src);  // We lack a better implementation of Copy.
  }

  // Make an identical copy of these options and return it.  This does
  // *not* copy the signature, and the returned options are not in
  // a frozen state.
  virtual RewriteOptions* Clone() const;

  // Computes a signature for the RewriteOptions object, including all
  // contained classes (DomainLawyer, FileLoadPolicy, WildCardGroups).
  //
  // Computing a signature "freezes" the class instance.  Attempting
  // to modify a RewriteOptions after freezing will DCHECK.
  void ComputeSignature(const Hasher* hasher);

  // Clears a computed signature, unfreezing the options object.  This
  // is intended for testing.
  void ClearSignatureForTesting() {
    frozen_ = false;
    signature_.clear();
  }

  // Returns the computed signature.
  const GoogleString& signature() const {
    DCHECK(frozen_);
    return signature_;
  }

  // Name of the actual type of this instance as a poor man's RTTI.
  virtual const char* class_name() const;

 protected:
  class OptionBase {
   public:
    virtual ~OptionBase();
    virtual void Merge(const OptionBase* one, const OptionBase* two) = 0;
    virtual bool was_set() const = 0;
    virtual GoogleString Signature(const Hasher* hasher) const = 0;
  };

  // Helper class to represent an Option, whose value is held in some class T.
  // An option is explicitly initialized with its default value, although the
  // default value can be altered later.  It keeps track of whether a
  // value has been explicitly set (independent of whether that happens to
  // coincide with the default value).
  //
  // It can use this knowledge to intelligently merge a 'base' option value
  // into a 'new' option value, allowing explicitly set values from 'base'
  // to override default values from 'new'.
  template<class T> class Option : public OptionBase {
   public:
    Option() : was_set_(false) {}

    virtual bool was_set() const { return was_set_; }

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

    // The signature of the Merge implementation must match the base-class.
    // The caller is responsible for ensuring that only the same typed Options
    // are compared.  In RewriteOptions::Merge this is guaranteed because we
    // are always comparing options at the same index in a vector<OptionBase*>.
    virtual void Merge(const OptionBase* a, const OptionBase* b) {
      MergeHelper(static_cast<const Option*>(a), static_cast<const Option*>(b));
    }

    void MergeHelper(const Option* one, const Option* two) {
      if (two->was_set_ || !one->was_set_) {
        value_ = two->value_;
        was_set_ = two->was_set_;
      } else {
        value_ = one->value_;
        was_set_ = true;  // this stmt is reached only if one.was_set_==true
      }
    }

    virtual GoogleString Signature(const Hasher* hasher) const {
      return RewriteOptions::OptionSignature(value_, hasher);
    }

   private:
    T value_;
    bool was_set_;

    DISALLOW_COPY_AND_ASSIGN(Option);
  };

  // When adding an option, we take the default_value by value, not
  // const-reference.  This is because when calling add_option we may
  // want to use a compile-time constant (e.g. Timer::kHourMs) which
  // does not have a linkable address.
  template<class T, class U>  // U must be assignable to T.
  void add_option(U default_value, Option<T>* option) {
    option->set_default(default_value);
    all_options_.push_back(option);
  }

  // When setting an option, however, we generally are doing so
  // with a variable rather than a constant so it makes sense to pass
  // it by reference.
  template<class T, class U>  // U must be assignable to T.
  void set_option(const U& new_value, Option<T>* option) {
    option->set(new_value);
    Modify();
  }

  // Marks the config as modified.
  void Modify();

 private:
  void SetUp();
  bool AddCommaSeparatedListToFilterSet(
      const StringPiece& filters, MessageHandler* handler, FilterSet* set);
  static Filter Lookup(const StringPiece& filter_name);

  // These static methods enable us to generate signatures for all
  // instantiated option-types from Option<T>::Signature().
  static GoogleString OptionSignature(bool x, const Hasher* hasher) {
    return x ? "T" : "F";
  }
  static GoogleString OptionSignature(int x, const Hasher* hasher) {
    return IntegerToString(x);
  }
  static GoogleString OptionSignature(int64 x, const Hasher* hasher) {
    return Integer64ToString(x);
  }
  static GoogleString OptionSignature(const GoogleString& x,
                                      const Hasher* hasher);
  static GoogleString OptionSignature(RewriteLevel x,
                                      const Hasher* hasher);

  bool modified_;
  bool frozen_;
  FilterSet enabled_filters_;
  FilterSet disabled_filters_;

  // Note: using the template class Option here saves a lot of repeated
  // and error-prone merging code.  However, it is not space efficient as
  // we are alternating int64s and bools in the structure.  If we cared
  // about that, then we would keep the bools in a bitmask.  But since
  // we don't really care we'll try to keep the code structured better.
  Option<RewriteLevel> level_;

  Option<int64> css_inline_max_bytes_;
  Option<int64> image_inline_max_bytes_;
  Option<int64> js_inline_max_bytes_;
  Option<int64> css_outline_min_bytes_;
  Option<int64> js_outline_min_bytes_;
  // The max Cache-Control TTL for HTML.
  Option<int64> max_html_cache_time_ms_;
  // Resources with Cache-Control TTL less than this will not be rewritten.
  Option<int64> min_resource_cache_time_to_rewrite_ms_;
  Option<int64> cache_invalidation_timestamp_;
  Option<int64> idle_flush_time_ms_;

  Option<int> image_max_rewrites_at_once_;
  Option<int> max_url_segment_size_;  // for http://a/b/c.d, use strlen("c.d")
  Option<int> max_url_size_;          // but this is strlen("http://a/b/c.d")

  Option<bool> enabled_;
  Option<bool> botdetect_enabled_;
  Option<bool> combine_across_paths_;
  Option<bool> log_rewrite_timing_;   // Should we time HtmlParser?
  Option<bool> lowercase_html_names_;
  Option<bool> always_rewrite_css_;  // For tests/debugging.
  Option<bool> respect_vary_;
  Option<bool> flush_html_;

  Option<GoogleString> beacon_url_;
  // Be sure to update constructor if when new fields is added so that they
  // are added to all_options_, which is used for Merge, and eventually,
  // Compare.

  std::vector<OptionBase*> all_options_;

  DomainLawyer domain_lawyer_;
  FileLoadPolicy file_load_policy_;

  WildcardGroup allow_resources_;
  WildcardGroup retain_comments_;

  GoogleString signature_;

  DISALLOW_COPY_AND_ASSIGN(RewriteOptions);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_OPTIONS_H_
