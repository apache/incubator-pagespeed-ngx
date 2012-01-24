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
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/wildcard_group.h"

namespace net_instaweb {

class Hasher;
class MessageHandler;
class PublisherConfig;

class RewriteOptions {
 public:
  // If you add or remove anything from this list, you need to update the
  // version number in rewrite_options.cc and FilterName().
  enum Filter {
    kAboveTheFold,  // Update kFirstFilter if you add something before this.
    kAddHead,
    kAddInstrumentation,
    kCollapseWhitespace,
    kCombineCss,
    kCombineHeads,
    kCombineJavascript,
    kComputePanelJson,
    kConvertJpegToProgressive,
    kConvertJpegToWebp,
    kConvertMetaTags,
    kConvertPngToJpeg,
    kDebug,
    kDeferJavascript,
    kDelayImages,
    kDisableJavascript,
    kDivStructure,
    kElideAttributes,
    kExplicitCloseTags,
    kExtendCacheCss,
    kExtendCacheImages,
    kExtendCacheScripts,
    kFlattenCssImports,
    kHtmlWriterFilter,
    kInlineCss,
    kInlineImages,
    kInlineImportToLink,
    kInlineJavascript,
    kInsertGA,
    kInsertImageDimensions,
    kLazyloadImages,
    kLeftTrimUrls,
    kMakeGoogleAnalyticsAsync,
    kMoveCssToHead,
    kOutlineCss,
    kOutlineJavascript,
    kRecompressImages,
    kRemoveComments,
    kRemoveQuotes,
    kResizeImages,
    kResizeMobileImages,
    kRewriteCss,
    kRewriteDomains,
    kRewriteJavascript,
    kRewriteStyleAttributes,
    kRewriteStyleAttributesWithUrl,
    kSpriteImages,
    kStripScripts,
    kEndOfFilters
  };

  // Any new Option added, should have a corresponding enum here and this should
  // be passed in when add_option is called in the constructor.
  enum OptionEnum {
    kRewriteLevel,
    kCssInlineMaxBytes,
    kImageInlineMaxBytes,
    kCssImageInlineMaxBytes,
    kJsInlineMaxBytes,
    kCssOutlineMinBytes,
    kJsOutlineMinBytes,
    kProgressiveJpegMinBytes,
    kMaxHtmlCacheTimeMs,
    kMinResourceCacheTimeToRewriteMs,
    kCacheInvalidationTimestamp,
    kIdleFlushTimeMs,
    kImageMaxRewritesAtOnce,
    kMaxUrlSegmentSize,
    kMaxUrlSize,
    kEnabled,
    kAjaxRewritingEnabled,
    kBotdetectEnabled,
    kCombineAcrossPaths,
    kLogRewriteTiming,
    kLowercaseHtmlNames,
    kAlwaysRewriteCss,
    kRespectVary,
    kFlushHtml,
    kServeStaleIfFetchError,
    kEnableBlink,
    kServeBlinkNonCritical,
    kDefaultCacheHtml,
    kModifyCachingHeaders,
    kBeaconUrl,
    kImageJpegRecompressionQuality,
    kImageLimitOptimizedPercent,
    kImageLimitResizeAreaPercent,
    kImageWebpRecompressQuality,
    kMaxDelayedImagesIndex,
    kMinImageSizeLowResolutionBytes,
    kCriticalImagesCacheExpirationTimeMs,
    kImageJpegNumProgressiveScans,
    kImageRetainColorProfile,
    kImageRetainExifData,
    kAnalyticsID,
    kAboveTheFoldNonCacheableElements,
    kAboveTheFoldCacheTime,
    kAboveTheFoldCacheableFamilies,
    kEndOfOptions
  };

  static const char kAjaxRewriteId[];
  static const char kCssCombinerId[];
  static const char kCssFilterId[];
  static const char kCssImportFlattenerId[];
  static const char kCssInlineId[];
  static const char kCacheExtenderId[];
  static const char kImageCombineId[];
  static const char kImageCompressionId[];
  static const char kJavascriptCombinerId[];
  static const char kJavascriptInlineId[];
  static const char kJavascriptMinId[];

  static const char kPanelCommentPrefix[];

  // Return the appropriate human-readable filter name for the given filter,
  // e.g. "CombineCss".
  static const char* FilterName(Filter filter);

  // Returns a two-letter id code for this filter, used for for encoding
  // URLs.
  static const char* FilterId(Filter filter);

  // Used for enumerating over all entries in the Filter enum.
  static const Filter kFirstFilter = kAboveTheFold;

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
  static const int64 kDefaultCssImageInlineMaxBytes;
  static const int64 kDefaultJsInlineMaxBytes;
  static const int64 kDefaultCssOutlineMinBytes;
  static const int64 kDefaultJsOutlineMinBytes;
  static const int64 kDefaultProgressiveJpegMinBytes;
  static const int64 kDefaultMaxHtmlCacheTimeMs;
  static const int64 kDefaultMinResourceCacheTimeToRewriteMs;
  static const int64 kDefaultCacheInvalidationTimestamp;
  static const int64 kDefaultIdleFlushTimeMs;
  static const GoogleString kDefaultBeaconUrl;
  static const int kDefaultImageJpegRecompressQuality;
  static const int kDefaultImageLimitOptimizedPercent;
  static const int kDefaultImageLimitResizeAreaPercent;
  static const int kDefaultImageJpegNumProgressiveScans;
  static const int kDefaultImageWebpRecompressQuality;

  // IE limits URL size overall to about 2k characters.  See
  // http://support.microsoft.com/kb/208427/EN-US
  static const int kDefaultMaxUrlSize;

  static const int kDefaultImageMaxRewritesAtOnce;

  // See http://code.google.com/p/modpagespeed/issues/detail?id=9
  // Apache evidently limits each URL path segment (between /) to
  // about 256 characters.  This is not fundamental URL limitation
  // but is Apache specific.
  static const int kDefaultMaxUrlSegmentSize;

  // Default number of first N images for which low res image is generated by
  // DelayImagesFilter.
  static const int kDefaultMaxDelayedImagesIndex;
  // Default minimum image size above which low res image is generated by
  // DelayImagesFilter.
  static const int64 kDefaultMinImageSizeLowResolutionBytes;
  // Default cache expiration value for critical images in ajax metadata cache.
  static const int64 kDefaultCriticalImagesCacheExpirationMs;

  // Default time in milliseconds for which a metadata cache entry may be used
  // after expiry.
  static const int64 kDefaultMetadataCacheStalenessThresholdMs;

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

  // Enables filters specified without a prefix or with a prefix of '+' and
  // disables filters specified with a prefix of '-'. Returns false if any
  // of the filter names are invalid, but all the valid ones will be
  // added anyway.
  bool AdjustFiltersByCommaSeparatedList(const StringPiece& filters,
                                         MessageHandler* handler);

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

  // Adds the filter to the list of enabled filters. However, if the filter
  // is also present in the list of disabled filters, that takes precedence.
  void EnableFilter(Filter filter);
  // Guarantees that a filter would be enabled even if it is present in the list
  // of disabled filters by removing it from disabled filter list.
  void ForceEnableFilter(Filter filter);
  void DisableFilter(Filter filter);
  void EnableFilters(const FilterSet& filter_set);
  void DisableFilters(const FilterSet& filter_set);
  // Clear all explicitly enabled and disabled filters. Some filters may still
  // be enabled by the rewrite level and HtmlWriterFilter will be enabled.
  void ClearFilters();

  // Enables all three extend_cache filters.
  void EnableExtendCacheFilters();

  bool Enabled(Filter filter) const;

  // Set Option name with value.
  bool SetOptionFromName(const GoogleString& name, const GoogleString& value,
                         MessageHandler* handler);

  // TODO(jmarantz): consider setting flags in the set_ methods so that
  // first's explicit settings can override default values from second.

  int64 css_outline_min_bytes() const { return css_outline_min_bytes_.value(); }
  void set_css_outline_min_bytes(int64 x) {
    set_option(x, &css_outline_min_bytes_);
  }

  GoogleString ga_id() const { return ga_id_.value(); }
  void set_ga_id(GoogleString id) {
    set_option(id, &ga_id_);
  }

  int64 js_outline_min_bytes() const { return js_outline_min_bytes_.value(); }
  void set_js_outline_min_bytes(int64 x) {
    set_option(x, &js_outline_min_bytes_);
  }

  int64 progressive_jpeg_min_bytes() const {
    return progressive_jpeg_min_bytes_.value();
  }
  void set_progressive_jpeg_min_bytes(int64 x) {
    set_option(x, &progressive_jpeg_min_bytes_);
  }

  // Retrieve the image inlining threshold, but return 0 if it's disabled.
  int64 ImageInlineMaxBytes() const;
  void set_image_inline_max_bytes(int64 x);
  // Retrieve the css image inlining threshold, but return 0 if it's disabled.
  int64 CssImageInlineMaxBytes() const;
  void set_css_image_inline_max_bytes(int64 x) {
    set_option(x, &css_image_inline_max_bytes_);
  }
  // The larger of ImageInlineMaxBytes and CssImageInlineMaxBytes.
  int64 MaxImageInlineMaxBytes() const;
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

  void set_ajax_rewriting_enabled(bool x) {
    set_option(x, &ajax_rewriting_enabled_);
  }

  bool ajax_rewriting_enabled() const {
    return ajax_rewriting_enabled_.value();
  }

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

  void set_serve_stale_if_fetch_error(bool x) {
    set_option(x, &serve_stale_if_fetch_error_);
  }
  bool serve_stale_if_fetch_error() const {
    return serve_stale_if_fetch_error_.value();
  }

  void set_enable_blink(bool x) { set_option(x, &enable_blink_); }
  bool enable_blink() const { return enable_blink_.value(); }

  void set_serve_blink_non_critical(bool x) {
    set_option(x, &serve_blink_non_critical_);
  }
  bool serve_blink_non_critical() const {
    return serve_blink_non_critical_.value();
  }

  void set_default_cache_html(bool x) { set_option(x, &default_cache_html_); }
  bool default_cache_html() const { return default_cache_html_.value(); }

  void set_modify_caching_headers(bool x) {
    set_option(x, &modify_caching_headers_);
  }
  bool modify_caching_headers() const {
    return modify_caching_headers_.value();
  }

  void set_max_delayed_images_index(int x) {
    set_option(x, &max_delayed_images_index_);
  }
  int max_delayed_images_index() const {
    return max_delayed_images_index_.value();
  }

  void set_min_image_size_low_resolution_bytes(int64 x) {
    set_option(x, &min_image_size_low_resolution_bytes_);
  }
  int64 min_image_size_low_resolution_bytes() const {
    return min_image_size_low_resolution_bytes_.value();
  }

  void set_critical_images_cache_expiration_time_ms(int64 x) {
    set_option(x, &critical_images_cache_expiration_time_ms_);
  }
  int64 critical_images_cache_expiration_time_ms() const {
    return critical_images_cache_expiration_time_ms_.value();
  }

  bool image_retain_color_profile() const {
    return image_retain_color_profile_.value();
  }
  void set_image_retain_color_profile(bool x) {
    set_option(x, &image_retain_color_profile_);
  }

  bool image_retain_exif_data() const {
    return image_retain_exif_data_.value();
  }
  void set_image_retain_exif_data(bool x) {
    set_option(x, &image_retain_exif_data_);
  }

  void set_metadata_cache_staleness_threshold_ms(int64 x) {
    set_option(x, &metadata_cache_staleness_threshold_ms_);
  }
  int64 metadata_cache_staleness_threshold_ms() const {
    return metadata_cache_staleness_threshold_ms_.value();
  }

  const GoogleString& beacon_url() const { return beacon_url_.value(); }
  void set_beacon_url(const StringPiece& p) {
    set_option(GoogleString(p.data(), p.size()), &beacon_url_);
  }

  // Return false in a subclass if you want to disallow all URL trimming in CSS.
  virtual bool trim_urls_in_css() const { return true; }

  int image_jpeg_recompress_quality() const {
    return image_jpeg_recompress_quality_.value();
  }
  void set_image_jpeg_recompress_quality(int x) {
    set_option(x, &image_jpeg_recompress_quality_);
  }

  int image_limit_optimized_percent() const {
    return image_limit_optimized_percent_.value();
  }
  void set_image_limit_optimized_percent(int x) {
    set_option(x, &image_limit_optimized_percent_);
  }
  int image_limit_resize_area_percent() const {
    return image_limit_resize_area_percent_.value();
  }
  void set_image_limit_resize_area_percent(int x) {
    set_option(x, &image_limit_resize_area_percent_);
  }

  int image_jpeg_num_progressive_scans() const {
    return image_jpeg_num_progressive_scans_.value();
  }
  void set_image_jpeg_num_progressive_scans(int x) {
    set_option(x, &image_jpeg_num_progressive_scans_);
  }

  int image_webp_recompress_quality() const {
    return image_webp_recompress_quality_.value();
  }
  void set_image_webp_recompress_quality(int x) {
    set_option(x, &image_webp_recompress_quality_);
  }

  // Takes ownership of the config.
  void set_panel_config(PublisherConfig* panel_config);
  const PublisherConfig* panel_config() const;

  // Merge src into 'this'.  Generally, options that are explicitly
  // set in src will override those explicitly set in 'this', although
  // option Merge implementations can be redefined by specific Option
  // class implementations (e.g. OptionInt64MergeWithMax).  One
  // semantic subject to interpretation is when a core-filter is
  // disabled in the first set and not in the second.  My judgement is
  // that the 'disable' from 'this' should override the core-set
  // membership in the 'src', but not an 'enable' in the 'src'.
  //
  // You can make an exact duplicate of RewriteOptions object 'src' via
  // (new 'typeof src')->Merge(src), aka Clone().
  //
  // Merge expects that 'src' and 'this' are the same type.  If that's
  // not true, this function will DCHECK.
  virtual void Merge(const RewriteOptions& src);

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
  // TODO(sligocki): Rename to allow for more general initialization.
  virtual void DisallowTroublesomeResources();

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

  GoogleString ToString() const;

  // Name of the actual type of this instance as a poor man's RTTI.
  virtual const char* class_name() const;

  // Returns true if generation low res images is required.
  virtual bool NeedLowResImages() const {
    return Enabled(kDelayImages);
  }

 protected:
  class OptionBase {
   public:
    OptionBase() : id_(NULL), option_enum_(RewriteOptions::kEndOfOptions) {}
    virtual ~OptionBase();
    virtual bool SetFromString(const GoogleString& value_string) = 0;
    virtual void Merge(const OptionBase* src) = 0;
    virtual bool was_set() const = 0;
    virtual GoogleString Signature(const Hasher* hasher) const = 0;
    virtual GoogleString ToString() const = 0;
    void set_id(const char* id) { id_ = id; }
    const char* id() {
      DCHECK(id_);
      return id_;
    }
    void set_option_enum(OptionEnum option_enum) { option_enum_ = option_enum; }
    OptionEnum option_enum() const { return option_enum_; }
   private:
    const char* id_;
    OptionEnum option_enum_;  // To know where this is in all_options_.
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
  template<class T> class OptionTemplateBase : public OptionBase {
   public:
    OptionTemplateBase() : was_set_(false) {}

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

    // The signature of the Merge implementation must match the base-class.  The
    // caller is responsible for ensuring that only the same typed Options are
    // compared.  In RewriteOptions::Merge this is guaranteed because the
    // vector<OptionBase*> all_options_ is sorted on option_enum().  We DCHECK
    // that the option_enum of this and src are the same.
    virtual void Merge(const OptionBase* src) {
      DCHECK(option_enum() == src->option_enum());
      MergeHelper(static_cast<const OptionTemplateBase*>(src));
    }

    void MergeHelper(const OptionTemplateBase* src) {
      // Even if !src->was_set, the default value needs to be transferred
      // over in case it was changed with set_default or SetDefaultRewriteLevel.
      if (src->was_set_ || !was_set_) {
        value_ = src->value_;
        was_set_ = src->was_set_;
      }
    }

   private:
    T value_;
    bool was_set_;

    DISALLOW_COPY_AND_ASSIGN(OptionTemplateBase);
  };

  // Subclassing OptionTemplateBase so that the conversion functions that need
  // to invoke static overloaded functions are declared only here.  Enables
  // subclasses of RewriteOptions to override these in case they use Option
  // types not visible here.
  template<class T> class Option : public OptionTemplateBase<T> {
   public:
    Option() {}

    // Sets value_ from value_string.
    virtual bool SetFromString(const GoogleString& value_string) {
      T value;
      bool success = RewriteOptions::ParseFromString(value_string, &value);
      if (success) {
        this->set(value);
      }
      return success;
    }

    virtual GoogleString Signature(const Hasher* hasher) const {
      return RewriteOptions::OptionSignature(this->value(), hasher);
    }

    virtual GoogleString ToString() const {
      return RewriteOptions::ToString(this->value());
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(Option);
  };

  // Like Option<int64>, but merge by taking the Max of the two values.  Note
  // that this could be templatized on type in which case we'd need to inline
  // the implementation of Merge.
  class OptionInt64MergeWithMax : public Option<int64> {
   public:
    virtual ~OptionInt64MergeWithMax();
    virtual void Merge(const OptionBase* src_base);
  };

  // When adding an option, we take the default_value by value, not
  // const-reference.  This is because when calling add_option we may
  // want to use a compile-time constant (e.g. Timer::kHourMs) which
  // does not have a linkable address.
  // The option_enum_ field of Option is set from the option_enum argument here.
  // It has to be ensured that correct enum is passed in.  If two Option<>
  // objects have same enum, then SetOptionFromName will not work for those.  If
  // option_enum is not passed in, then kEndOfOptions will be used (default
  // value in OptionBase constructor) and this means this option cannot be set
  // using SetOptionFromName.
  template<class T, class U>  // U must be assignable to T.
  void add_option(U default_value, OptionTemplateBase<T>* option,
                  const char* id, OptionEnum option_enum) {
    add_option(default_value, option, id);
    option->set_option_enum(option_enum);
  }
  template<class T, class U>  // U must be assignable to T.
  void add_option(U default_value, OptionTemplateBase<T>* option,
                  const char* id) {
    option->set_default(default_value);
    option->set_id(id);
    all_options_.push_back(option);
  }

  // When setting an option, however, we generally are doing so
  // with a variable rather than a constant so it makes sense to pass
  // it by reference.
  template<class T, class U>  // U must be assignable to T.
  void set_option(const U& new_value, OptionTemplateBase<T>* option) {
    option->set(new_value);
    Modify();
  }

  // To be called after construction and before this object is used.
  // Currently this is called from constructor.  If a sub-class calls
  // add_option() with OptionEnum, then it has to call this again to ensure
  // sorted order.
  void SortOptions();

  // Marks the config as modified.
  void Modify();

 private:
  void SetUp();
  bool AddCommaSeparatedListToFilterSet(
      const StringPiece& filters, MessageHandler* handler, FilterSet* set);
  bool AddCommaSeparatedListToPlusAndMinusFilterSets(
      const StringPiece& filters, MessageHandler* handler,
      FilterSet* plus_set, FilterSet* minus_set);
  bool AddByNameToFilterSet(
      const StringPiece& option, MessageHandler* handler, FilterSet* set);
  static Filter LookupFilter(const StringPiece& filter_name);
  static OptionEnum LookupOption(const StringPiece& option_name);

  // These static methods are used by Option<T>::SetFromString to set
  // Option<T>::value_ from a string representation of it.
  static bool ParseFromString(const GoogleString& value_string, bool* value) {
    // How are bools passed in the string?  I am assuming "true" / "false".
    *value = (value_string == "true");
    return true;
  }
  static bool ParseFromString(const GoogleString& value_string, int* value) {
    return StringToInt(value_string, value);
  }
  static bool ParseFromString(const GoogleString& value_string, int64* value) {
    return StringToInt64(value_string, value);
  }
  static bool ParseFromString(const GoogleString& value_string,
                              GoogleString* value) {
    *value = value_string;
    return true;
  }
  static bool ParseFromString(const GoogleString& value_string,
                              RewriteLevel* value) {
    // We shouldn't be setting level_ via SetOptionFromName. So ignore
    // value_string.
    // Should we set the default?  *value = kPassThrough
    return false;
  }

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

  // These static methods enable us to generate strings for all
  // instantiated option-types from Option<T>::Signature().
  static GoogleString ToString(bool x) {
    return x ? "True" : "False";
  }
  static GoogleString ToString(int x) {
    return IntegerToString(x);
  }
  static GoogleString ToString(int64 x) {
    return Integer64ToString(x);
  }
  static GoogleString ToString(const GoogleString& x) {
    return x;
  }
  static GoogleString ToString(RewriteLevel x);

  // Returns true if option1's enum is less than option2's. Used to order
  // all_options_.
  static bool OptionLessThanByEnum(OptionBase* option1, OptionBase* option2) {
    return option1->option_enum() < option2->option_enum();
  }

  // Returns if option's enum is less than arg.
  static bool LessThanArg(OptionBase* option, OptionEnum arg) {
    return option->option_enum() < arg;
  }

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

  OptionInt64MergeWithMax cache_invalidation_timestamp_;

  Option<int64> css_inline_max_bytes_;
  Option<int64> image_inline_max_bytes_;
  Option<int64> css_image_inline_max_bytes_;
  Option<int64> js_inline_max_bytes_;
  Option<int64> css_outline_min_bytes_;
  Option<int64> js_outline_min_bytes_;
  Option<int64> progressive_jpeg_min_bytes_;
  // The max Cache-Control TTL for HTML.
  Option<int64> max_html_cache_time_ms_;
  // Resources with Cache-Control TTL less than this will not be rewritten.
  Option<int64> min_resource_cache_time_to_rewrite_ms_;
  Option<int64> idle_flush_time_ms_;

  // Options related to jpeg compression.
  Option<int> image_jpeg_recompress_quality_;
  Option<int> image_jpeg_num_progressive_scans_;
  Option<bool> image_retain_color_profile_;
  Option<bool> image_retain_exif_data_;

  // Options governing when to retain optimized images vs keep original
  Option<int> image_limit_optimized_percent_;
  Option<int> image_limit_resize_area_percent_;

  // Options related to webp compression.
  Option<int> image_webp_recompress_quality_;

  Option<int> image_max_rewrites_at_once_;
  Option<int> max_url_segment_size_;  // for http://a/b/c.d, use strlen("c.d")
  Option<int> max_url_size_;          // but this is strlen("http://a/b/c.d")

  Option<bool> enabled_;
  Option<bool> ajax_rewriting_enabled_;  // Should ajax rewriting be enabled?
  Option<bool> botdetect_enabled_;
  Option<bool> combine_across_paths_;
  Option<bool> log_rewrite_timing_;   // Should we time HtmlParser?
  Option<bool> lowercase_html_names_;
  Option<bool> always_rewrite_css_;  // For tests/debugging.
  Option<bool> respect_vary_;
  Option<bool> flush_html_;
  // Should we serve stale responses if the fetch results in a server side
  // error.
  Option<bool> serve_stale_if_fetch_error_;
  Option<bool> enable_blink_;
  // When non-cacheable panels are absent, non-critical content is already
  // served in blink flow. This flag indicates whether to serve non-critical
  // from panel_filter or not.
  Option<bool> serve_blink_non_critical_;
  // When default_cache_html_ is false (default) we do not cache
  // input HTML which lacks Cache-Control headers. But, when set true,
  // we will cache those inputs for the implicit lifetime just like we
  // do for resources.
  Option<bool> default_cache_html_;
  // In general, we rewrite Cache-Control headers for HTML. We do this
  // for several reasons, but at least one is that our rewrites are not
  // necessarily publicly cacheable.
  // Some people don't like this, so we allow them to disable it.
  Option<bool> modify_caching_headers_;

  // Number of first N images for which low res image is generated. Negative
  // values will bypass image index check.
  Option<int> max_delayed_images_index_;
  // Minimum image size above which low res image is generated.
  Option<int64> min_image_size_low_resolution_bytes_;

  // Critical images ajax metadata cache expiration time in msec.
  Option<int64> critical_images_cache_expiration_time_ms_;

  // The maximum time beyond expiry for which a metadata cache entry may be
  // used.
  Option<int64> metadata_cache_staleness_threshold_ms_;

  scoped_ptr<PublisherConfig> panel_config_;

  Option<GoogleString> beacon_url_;
  Option<GoogleString> ga_id_;

  // Be sure to update constructor if when new fields is added so that they
  // are added to all_options_, which is used for Merge, and eventually,
  // Compare.
  typedef std::vector<OptionBase*> OptionBaseVector;
  OptionBaseVector all_options_;

  // When compiled for debug, we lazily check whether the all the Option<>
  // member variables in all_options have unique IDs.
  //
  // Note that we include this member-variable in the structrue even under
  // optimization as otherwise it might be very bad news indeed if someone
  // mixed debug/opt object files in an executable.
  bool options_uniqueness_checked_;

  DomainLawyer domain_lawyer_;
  FileLoadPolicy file_load_policy_;

  WildcardGroup allow_resources_;
  WildcardGroup retain_comments_;

  GoogleString signature_;

  DISALLOW_COPY_AND_ASSIGN(RewriteOptions);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_OPTIONS_H_
