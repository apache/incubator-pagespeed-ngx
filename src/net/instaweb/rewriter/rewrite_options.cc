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

#include "net/instaweb/rewriter/public/rewrite_options.h"

#include <algorithm>
#include <cstddef>
#include <set>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/rewriter/panel_config.pb.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/furious_util.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/wildcard_group.h"

namespace {

// This version index serves as global signature key.  Much of the
// data emitted in signatures is based on the option ordering, which
// can change as we add new options.  So every time there is a
// binary-incompatible change to the option ordering, we bump this
// version.
//
// Note: we now use a two-letter code for identifying enabled filters, so
// there is no need bump the option version when changing the filter enum.
//
// Updating this value will have the indirect effect of flushing the metadata
// cache.
//
// This version number should be incremented if any default-values are changed,
// either in the add_option() call or via options->set_default.
const int kOptionsVersion = 12;

}  // namespace

namespace net_instaweb {

// RewriteFilter prefixes
const char RewriteOptions::kAjaxRewriteId[] = "aj";
const char RewriteOptions::kCssCombinerId[] = "cc";
const char RewriteOptions::kCssFilterId[] = "cf";
const char RewriteOptions::kCssImportFlattenerId[] = "if";
const char RewriteOptions::kCssInlineId[] = "ci";
const char RewriteOptions::kCacheExtenderId[] = "ce";
const char RewriteOptions::kImageCombineId[] = "is";
const char RewriteOptions::kImageCompressionId[] = "ic";
const char RewriteOptions::kJavascriptCombinerId[] = "jc";
const char RewriteOptions::kJavascriptMinId[] = "jm";
const char RewriteOptions::kJavascriptInlineId[] = "ji";
const char RewriteOptions::kPanelCommentPrefix[] = "GooglePanel";

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
// merged, is in image_rewrite_filter you could apply the
// base64-bloat-factor before comparing against the threshold.  Then
// we could use one number if we like that idea.
//
// jmaessen: For the moment, there's a separate threshold for image inline.
const int64 RewriteOptions::kDefaultCssInlineMaxBytes = 2048;
// TODO(jmaessen): Adjust these thresholds in a subsequent CL
// (Will require re-golding tests.)
const int64 RewriteOptions::kDefaultImageInlineMaxBytes = 2048;
const int64 RewriteOptions::kDefaultCssImageInlineMaxBytes = 2048;
const int64 RewriteOptions::kDefaultJsInlineMaxBytes = 2048;
const int64 RewriteOptions::kDefaultCssOutlineMinBytes = 3000;
const int64 RewriteOptions::kDefaultJsOutlineMinBytes = 3000;
const int64 RewriteOptions::kDefaultProgressiveJpegMinBytes = 10240;

const int64 RewriteOptions::kDefaultMaxHtmlCacheTimeMs = 0;
const int64 RewriteOptions::kDefaultMinResourceCacheTimeToRewriteMs = 0;

const int64 RewriteOptions::kDefaultCacheInvalidationTimestamp = -1;
const int64 RewriteOptions::kDefaultIdleFlushTimeMs = 10;
const int64 RewriteOptions::kDefaultImplicitCacheTtlMs = 5 * Timer::kMinuteMs;

const int64 RewriteOptions::kDefaultPrioritizeVisibleContentCacheTimeMs =
    30 * Timer::kMinuteMs;  // 30 mins.

// Limit on concurrent ongoing image rewrites.
// TODO(jmaessen): Determine a sane default for this value.
const int RewriteOptions::kDefaultImageMaxRewritesAtOnce = 8;

// IE limits URL size overall to about 2k characters.  See
// http://support.microsoft.com/kb/208427/EN-US
const int RewriteOptions::kDefaultMaxUrlSize = 2083;

// Jpeg quality that needs to be used while recompressing. If set to -1, we
// use source image quality parameters, and is lossless.
const int RewriteOptions::kDefaultImageJpegRecompressQuality = -1;

// Number of scans to output for jpeg images when using progressive mode. If set
// to -1, we ignore this setting.
const int RewriteOptions::kDefaultImageJpegNumProgressiveScans = -1;

// Percentage savings in order to retain rewritten images; these default
// to 100% so that we always attempt to resize downsized images, and
// unconditionally retain images if they save any bytes at all.
const int RewriteOptions::kDefaultImageLimitOptimizedPercent = 100;
const int RewriteOptions::kDefaultImageLimitResizeAreaPercent = 100;

// WebP quality that needs to be used while recompressing. If set to -1, we
// use source image quality parameters.
const int RewriteOptions::kDefaultImageWebpRecompressQuality = -1;

// See http://code.google.com/p/modpagespeed/issues/detail?id=9.  By
// default, Apache evidently limits each URL path segment (between /)
// to about 256 characters.  This is not a fundamental URL limitation
// but is Apache specific.  Ben Noordhuis has provided a workaround
// of hooking map_to_storage to skip the directory-mapping phase in
// Apache.  See http://code.google.com/p/modpagespeed/issues/detail?id=176
const int RewriteOptions::kDefaultMaxUrlSegmentSize = 1024;

const GoogleString RewriteOptions::kDefaultBeaconUrl =
    "/mod_pagespeed_beacon?ets=";

const int RewriteOptions::kDefaultMaxInlinedPreviewImagesIndex = 5;
const int64 RewriteOptions::kDefaultMinImageSizeLowResolutionBytes = 1 * 1024;
const int64 RewriteOptions::kDefaultCriticalImagesCacheExpirationMs =
    Timer::kHourMs;
const int64 RewriteOptions::kDefaultMetadataCacheStalenessThresholdMs = 0;
const int RewriteOptions::kDefaultFuriousTrafficPercent = 50;

const char RewriteOptions::kClassName[] = "RewriteOptions";

const char RewriteOptions::kDefaultXModPagespeedHeaderValue[] = "__VERSION__";

const char* RewriteOptions::option_enum_to_name_array_[
    RewriteOptions::kEndOfOptions];

namespace {

const RewriteOptions::Filter kCoreFilterSet[] = {
  RewriteOptions::kAddHead,
  RewriteOptions::kCombineCss,
  RewriteOptions::kConvertMetaTags,
  RewriteOptions::kExtendCacheCss,
  RewriteOptions::kExtendCacheImages,
  RewriteOptions::kExtendCacheScripts,
  RewriteOptions::kHtmlWriterFilter,
  RewriteOptions::kInlineCss,
  RewriteOptions::kInlineImages,
  RewriteOptions::kInlineImportToLink,
  RewriteOptions::kInlineJavascript,
  RewriteOptions::kLeftTrimUrls,
  RewriteOptions::kRecompressImages,
  RewriteOptions::kResizeImages,
  RewriteOptions::kRewriteCss,
  RewriteOptions::kRewriteJavascript,
  RewriteOptions::kRewriteStyleAttributesWithUrl,
};

// Note: all Core filters are Test filters as well.  For maintainability,
// this is managed in the c++ switch statement.
const RewriteOptions::Filter kTestFilterSet[] = {
  RewriteOptions::kConvertJpegToWebp,
  RewriteOptions::kInsertGA,
  RewriteOptions::kInsertImageDimensions,
  RewriteOptions::kMakeGoogleAnalyticsAsync,
  RewriteOptions::kRewriteDomains,
  RewriteOptions::kSpriteImages,
};

// Note: These filters should not be included even if the level is "All".
const RewriteOptions::Filter kDangerousFilterSet[] = {
  RewriteOptions::kComputePanelJson,  // internal, enabled conditionally
  RewriteOptions::kDeferJavascript,
  RewriteOptions::kDisableJavascript,
  RewriteOptions::kDivStructure,
  RewriteOptions::kExplicitCloseTags,
  RewriteOptions::kLazyloadImages,
  RewriteOptions::kServeNonCacheableNonCritical,  // internal,
                                                  // enabled conditionally
  RewriteOptions::kStripNonCacheable,  // internal, enabled conditionally
  RewriteOptions::kStripScripts,
};

#ifndef NDEBUG
void CheckFilterSetOrdering(const RewriteOptions::Filter* filters, int num) {
  for (int i = 1; i < num; ++i) {
    DCHECK_GT(filters[i], filters[i - 1]);
  }
}
#endif

bool IsInSet(const RewriteOptions::Filter* filters, int num,
             RewriteOptions::Filter filter) {
  const RewriteOptions::Filter* end = filters + num;
  return std::binary_search(filters, end, filter);
}

}  // namespace

const char* RewriteOptions::FilterName(Filter filter) {
  switch (filter) {
    case kAddHead:                         return "Add Head";
    case kAddInstrumentation:              return "Add Instrumentation";
    case kCollapseWhitespace:              return "Collapse Whitespace";
    case kCombineCss:                      return "Combine Css";
    case kCombineHeads:                    return "Combine Heads";
    case kCombineJavascript:               return "Combine Javascript";
    case kComputePanelJson:                return "Computes panel json";
    case kConvertJpegToProgressive:        return "Convert Jpeg to Progressive";
    case kConvertJpegToWebp:               return "Convert Jpeg To Webp";
    case kConvertMetaTags:                 return "Convert Meta Tags";
    case kConvertPngToJpeg:                return "Convert Png to Jpeg";
    case kDebug:                           return "Debug";
    case kDeferJavascript:                 return "Defer Javascript";
    case kDelayImages:                     return "Delay Images";
    case kDisableJavascript:
      return "Disables scripts by placing them inside noscript tags";
    case kDivStructure:                    return "Div Structure";
    case kElideAttributes:                 return "Elide Attributes";
    case kExplicitCloseTags:               return "Explicit Close Tags";
    case kExtendCacheCss:                  return "Cache Extend Css";
    case kExtendCacheImages:               return "Cache Extend Images";
    case kExtendCacheScripts:              return "Cache Extend Scripts";
    case kFlattenCssImports:               return "Flatten CSS Imports";
    case kHtmlWriterFilter:                return "Flushes html";
    case kInlineCss:                       return "Inline Css";
    case kInlineImages:                    return "Inline Images";
    case kInlineImportToLink:              return "Inline @import to Link";
    case kInlineJavascript:                return "Inline Javascript";
    case kInsertGA:                        return "Insert Google Analytics";
    case kInsertImageDimensions:           return "Insert Image Dimensions";
    case kLazyloadImages:                  return "Lazyload Images";
    case kLeftTrimUrls:                    return "Left Trim Urls";
    case kMakeGoogleAnalyticsAsync:        return "Make Google Analytics Async";
    case kMoveCssToHead:                   return "Move Css To Head";
    case kOutlineCss:                      return "Outline Css";
    case kOutlineJavascript:               return "Outline Javascript";
    case kPrioritizeVisibleContent:        return "Prioritize Visible Content";
    case kRecompressImages:                return "Recompress Images";
    case kRemoveComments:                  return "Remove Comments";
    case kRemoveQuotes:                    return "Remove Quotes";
    case kResizeImages:                    return "Resize Images";
    case kResizeMobileImages:              return "Resize Mobile Images";
    case kRewriteCss:                      return "Rewrite Css";
    case kRewriteDomains:                  return "Rewrite Domains";
    case kRewriteJavascript:               return "Rewrite Javascript";
    case kRewriteStyleAttributes:          return "Rewrite Style Attributes";
    case kRewriteStyleAttributesWithUrl:
      return "Rewrite Style Attributes With Url";
    case kServeNonCacheableNonCritical:
        return "Serve Non Cacheable and Non Critical Content";
    case kSpriteImages:                    return "Sprite Images";
    case kStripNonCacheable:               return "Strip Non Cacheable";
    case kStripScripts:                    return "Strip Scripts";
    case kEndOfFilters:                    return "End of Filters";
  }
  return "Unknown Filter";
}

const char* RewriteOptions::FilterId(Filter filter) {
  switch (filter) {
    case kAddHead:                         return "ah";
    case kAddInstrumentation:              return "ai";
    case kCollapseWhitespace:              return "cw";
    case kCombineCss:                      return kCssCombinerId;
    case kCombineHeads:                    return "ch";
    case kCombineJavascript:               return kJavascriptCombinerId;
    case kComputePanelJson:                return "bp";
    case kConvertJpegToProgressive:        return "jp";
    case kConvertJpegToWebp:               return "jw";
    case kConvertMetaTags:                 return "mc";
    case kConvertPngToJpeg:                return "pj";
    case kDebug:                           return "db";
    case kDeferJavascript:                 return "dj";
    case kDelayImages:                     return "di";
    case kDisableJavascript:               return "jd";
    case kDivStructure:                    return "ds";
    case kElideAttributes:                 return "ea";
    case kExplicitCloseTags:               return "xc";
    case kExtendCacheCss:                  return "ec";
    case kExtendCacheImages:               return "ei";
    case kExtendCacheScripts:              return "es";
    case kFlattenCssImports:               return kCssImportFlattenerId;
    case kHtmlWriterFilter:                return "hw";
    case kInlineCss:                       return kCssInlineId;
    case kInlineImages:                    return "ii";
    case kInlineImportToLink:              return "il";
    case kInlineJavascript:                return kJavascriptInlineId;
    case kInsertGA:                        return "ig";
    case kInsertImageDimensions:           return "id";
    case kLazyloadImages:                  return "ll";
    case kLeftTrimUrls:                    return "tu";
    case kMakeGoogleAnalyticsAsync:        return "ga";
    case kMoveCssToHead:                   return "cm";
    case kOutlineCss:                      return "co";
    case kOutlineJavascript:               return "jo";
    case kPrioritizeVisibleContent:        return "pv";
    case kRecompressImages:                return "ir";
    case kRemoveComments:                  return "rc";
    case kRemoveQuotes:                    return "rq";
    case kResizeImages:                    return "ri";
    case kResizeMobileImages:              return "rm";
    case kRewriteCss:                      return kCssFilterId;
    case kRewriteDomains:                  return "rd";
    case kRewriteJavascript:               return kJavascriptMinId;
    case kRewriteStyleAttributes:          return "cs";
    case kRewriteStyleAttributesWithUrl:   return "cu";
    case kServeNonCacheableNonCritical:    return "sn";
    case kStripNonCacheable:               return "nc";
    case kSpriteImages:                    return kImageCombineId;
    case kStripScripts:                    return "ss";
    case kEndOfFilters:
      LOG(DFATAL) << "EndOfFilters passed as code: " << filter;
      return "EF";
  }
  LOG(DFATAL) << "Unknown filter code: " << filter;
  return "UF";
}

bool RewriteOptions::ParseRewriteLevel(
    const StringPiece& in, RewriteLevel* out) {
  bool ret = false;
  if (in != NULL) {
    if (StringCaseEqual(in, "CoreFilters")) {
      *out = kCoreFilters;
      ret = true;
    } else if (StringCaseEqual(in, "PassThrough")) {
      *out = kPassThrough;
      ret = true;
    } else if (StringCaseEqual(in, "TestingCoreFilters")) {
      *out = kTestingCoreFilters;
      ret = true;
    } else if (StringCaseEqual(in, "AllFilters")) {
      *out = kAllFilters;
      ret = true;
    }
  }
  return ret;
}

RewriteOptions::RewriteOptions()
    : modified_(false),
      frozen_(false),
      options_uniqueness_checked_(false),
      furious_id_(furious::kFuriousNotSet) {
  // Sanity-checks -- will be active only when compiled for debug.
#ifndef NDEBUG
  CheckFilterSetOrdering(kCoreFilterSet, arraysize(kCoreFilterSet));
  CheckFilterSetOrdering(kTestFilterSet, arraysize(kTestFilterSet));
  CheckFilterSetOrdering(kDangerousFilterSet, arraysize(kDangerousFilterSet));

  // Ensure that all filters have unique IDs.
  StringSet id_set;
  for (int i = 0; i < static_cast<int>(kEndOfFilters); ++i) {
    Filter filter = static_cast<Filter>(i);
    const char* id = FilterId(filter);
    std::pair<StringSet::iterator, bool> insertion = id_set.insert(id);
    DCHECK(insertion.second) << "Duplicate RewriteOption filter id: " << id;
  }

  // We can't check options uniqueness until additional extra
  // options are added by subclasses.  We could do this in the
  // destructor I suppose, but we defer it till ComputeSignature.
#endif

  // TODO(jmarantz): consider adding these on demand so that the cost of
  // initializing an empty RewriteOptions object is closer to zero.
  add_option(kPassThrough, &level_, "l", kRewriteLevel);
  add_option(kDefaultCssInlineMaxBytes, &css_inline_max_bytes_, "ci",
             kCssInlineMaxBytes);
  add_option(kDefaultImageInlineMaxBytes, &image_inline_max_bytes_, "ii",
             kImageInlineMaxBytes);
  add_option(kDefaultCssImageInlineMaxBytes, &css_image_inline_max_bytes_,
             "cii", kCssImageInlineMaxBytes);
  add_option(kDefaultJsInlineMaxBytes, &js_inline_max_bytes_, "ji",
             kJsInlineMaxBytes);
  add_option(kDefaultCssOutlineMinBytes, &css_outline_min_bytes_, "co",
             kCssOutlineMinBytes);
  add_option(kDefaultJsOutlineMinBytes, &js_outline_min_bytes_, "jo",
             kJsOutlineMinBytes);
  add_option(kDefaultProgressiveJpegMinBytes, &progressive_jpeg_min_bytes_,
             "jp", kProgressiveJpegMinBytes);
  add_option(kDefaultMaxHtmlCacheTimeMs, &max_html_cache_time_ms_, "hc",
             kMaxHtmlCacheTimeMs);
  add_option(kDefaultMinResourceCacheTimeToRewriteMs,
             &min_resource_cache_time_to_rewrite_ms_, "rc",
             kMinResourceCacheTimeToRewriteMs);
  add_option(kDefaultCacheInvalidationTimestamp,
             &cache_invalidation_timestamp_, "it", kCacheInvalidationTimestamp);
  add_option(kDefaultIdleFlushTimeMs, &idle_flush_time_ms_, "if",
             kIdleFlushTimeMs);
  add_option(kDefaultImplicitCacheTtlMs, &implicit_cache_ttl_ms_, "ict",
             kImplicitCacheTtlMs);
  add_option(kDefaultImageMaxRewritesAtOnce, &image_max_rewrites_at_once_,
             "im", kImageMaxRewritesAtOnce);
  add_option(kDefaultMaxUrlSegmentSize, &max_url_segment_size_, "uss",
             kMaxUrlSegmentSize);
  add_option(kDefaultMaxUrlSize, &max_url_size_, "us", kMaxUrlSize);
  add_option(true, &enabled_, "e", kEnabled);
  add_option(false, &ajax_rewriting_enabled_, "ar", kAjaxRewritingEnabled);
  add_option(false, &botdetect_enabled_, "be", kBotdetectEnabled);
  add_option(true, &combine_across_paths_, "cp", kCombineAcrossPaths);
  add_option(false, &log_rewrite_timing_, "lr", kLogRewriteTiming);
  add_option(false, &lowercase_html_names_, "lh", kLowercaseHtmlNames);
  add_option(false, &always_rewrite_css_, "arc", kAlwaysRewriteCss);
  add_option(false, &respect_vary_, "rv", kRespectVary);
  add_option(false, &flush_html_, "fh", kFlushHtml);
  add_option(true, &serve_stale_if_fetch_error_, "ss", kServeStaleIfFetchError);
  add_option(false, &disable_override_doc_open_, "dodo",
             kDisableOverrideDocOpen);
  add_option(false, &enable_blink_critical_line_, "ebcl",
             kEnableBlinkCriticalLine);
  add_option(false, &serve_blink_non_critical_, "snc", kServeBlinkNonCritical);
  add_option(false, &default_cache_html_, "dch", kDefaultCacheHtml);
  add_option(true, &modify_caching_headers_, "mch", kModifyCachingHeaders);
  add_option(kDefaultBeaconUrl, &beacon_url_, "bu", kBeaconUrl);
  add_option(false, &lazyload_images_after_onload_, "llio",
             kLazyloadImagesAfterOnload);
  add_option(false, &domain_rewrite_all_tags_, "rat", kDomainRewriteAllTags);
  add_option(kDefaultImageJpegRecompressQuality,
             &image_jpeg_recompress_quality_, "iq",
             kImageJpegRecompressionQuality);
  add_option(kDefaultImageLimitOptimizedPercent,
             &image_limit_optimized_percent_, "ip",
             kImageLimitOptimizedPercent);
  add_option(kDefaultImageLimitResizeAreaPercent,
             &image_limit_resize_area_percent_, "ia",
             kImageLimitResizeAreaPercent);
  add_option(kDefaultImageWebpRecompressQuality,
             &image_webp_recompress_quality_, "iw",
             kImageWebpRecompressQuality);
  add_option(kDefaultMaxInlinedPreviewImagesIndex,
             &max_inlined_preview_images_index_, "mdii",
             kMaxInlinedPreviewImagesIndex);
  add_option(kDefaultMinImageSizeLowResolutionBytes,
             &min_image_size_low_resolution_bytes_, "islr",
             kMinImageSizeLowResolutionBytes);
  add_option(kDefaultCriticalImagesCacheExpirationMs,
             &critical_images_cache_expiration_time_ms_, "cice",
             kCriticalImagesCacheExpirationTimeMs);
  add_option(kDefaultImageJpegNumProgressiveScans,
             &image_jpeg_num_progressive_scans_, "ijps",
             kImageJpegNumProgressiveScans);
  add_option(false, &image_retain_color_profile_, "ircp",
             kImageRetainColorProfile);
  add_option(false, &image_retain_color_sampling_, "ircs",
             kImageRetainColorSampling);
  add_option(false, &image_retain_exif_data_, "ired", kImageRetainExifData);
  add_option("", &prioritize_visible_content_non_cacheable_elements_, "nce",
             kPrioritizeVisibleContentNonCacheableElements);
  add_option(kDefaultPrioritizeVisibleContentCacheTimeMs,
             &prioritize_visible_content_cache_time_ms_, "ctm",
             kPrioritizeVisibleContentCacheTime);
  add_option("", &ga_id_, "ig", kAnalyticsID);
  add_option(true, &increase_speed_tracking_, "st", kIncreaseSpeedTracking);
  add_option(false, &running_furious_, "fur", kRunningFurious);
  add_option(kDefaultFuriousTrafficPercent, &furious_percent_, "fp",
             kFuriousPercent);
  add_option(kDefaultXModPagespeedHeaderValue, &x_header_value_, "xhv",
             kXModPagespeedHeaderValue);
  // Sort all_options_ on enum.
  SortOptions();
  // Do not call add_option with OptionEnum fourth argument after this.
  add_option(kDefaultMetadataCacheStalenessThresholdMs,
             &metadata_cache_staleness_threshold_ms_, "mcst");

  // Enable HtmlWriterFilter by default.
  EnableFilter(kHtmlWriterFilter);
}

RewriteOptions::~RewriteOptions() {
  STLDeleteElements(&furious_specs_);
}

RewriteOptions::OptionBase::~OptionBase() {
}

void RewriteOptions::Initialize() {
  InitOptionEnumToNameArray();
}

void RewriteOptions::SortOptions() {
  std::sort(all_options_.begin(), all_options_.end(),
            RewriteOptions::OptionLessThanByEnum);
}

void RewriteOptions::set_panel_config(
    PublisherConfig* panel_config) {
  panel_config_.reset(panel_config);
}

const PublisherConfig* RewriteOptions::panel_config() const {
  return panel_config_.get();
}

void RewriteOptions::SetFuriousState(int id) {
  furious_id_ = id;
  SetupFuriousRewriters();
}

void RewriteOptions::DisallowTroublesomeResources() {
  // http://code.google.com/p/modpagespeed/issues/detail?id=38
  Disallow("*js_tinyMCE*");  // js_tinyMCE.js
  // Official tinyMCE URLs: tiny_mce.js, tiny_mce_src.js, tiny_mce_gzip.php, ...
  Disallow("*tiny_mce*");
  // I've also seen tinymce.js
  Disallow("*tinymce*");

  // http://code.google.com/p/modpagespeed/issues/detail?id=352
  Disallow("*scriptaculous.js*");

  // Breaks some sites.
  Disallow("*connect.facebook.net/*");

  // http://code.google.com/p/modpagespeed/issues/detail?id=186
  // ckeditor.js, ckeditor_basic.js, ckeditor_basic_source.js, ...
  Disallow("*ckeditor*");

  // http://code.google.com/p/modpagespeed/issues/detail?id=207
  // jquery-ui-1.8.2.custom.min.js, jquery-1.4.4.min.js, jquery.fancybox-...
  //
  // TODO(sligocki): Is jquery actually a problem? Perhaps specific
  // jquery libraries (like tiny MCE). Investigate before disabling.
  // Disallow("*jquery*");

  // http://code.google.com/p/modpagespeed/issues/detail?id=216
  // Appears to be an issue with old version of jsminify.
  // Disallow("*swfobject*");  // swfobject.js

  // TODO(sligocki): Add disallow for the JS broken in:
  // http://code.google.com/p/modpagespeed/issues/detail?id=142
  // Not clear which JS file is broken and proxying is not working correctly.

  if (Enabled(kComputePanelJson)) {
    RetainComment(StrCat(kPanelCommentPrefix, "*"));
  }
}

bool RewriteOptions::AdjustFiltersByCommaSeparatedList(
    const StringPiece& filters, MessageHandler* handler) {
  return AddCommaSeparatedListToPlusAndMinusFilterSets(
      filters, handler, &enabled_filters_, &disabled_filters_);
}

bool RewriteOptions::EnableFiltersByCommaSeparatedList(
    const StringPiece& filters, MessageHandler* handler) {
  return AddCommaSeparatedListToFilterSetState(
      filters, handler, &enabled_filters_);
}

bool RewriteOptions::DisableFiltersByCommaSeparatedList(
    const StringPiece& filters, MessageHandler* handler) {
  return AddCommaSeparatedListToFilterSetState(
      filters, handler, &disabled_filters_);
}

void RewriteOptions::DisableAllFiltersNotExplicitlyEnabled() {
  for (int f = kFirstFilter; f != kEndOfFilters; ++f) {
    Filter filter = static_cast<Filter>(f);
    if (enabled_filters_.find(filter) == enabled_filters_.end()) {
      DisableFilter(filter);
    }
  }
}

void RewriteOptions::EnableFilter(Filter filter) {
  DCHECK(!frozen_);
  std::pair<FilterSet::iterator, bool> inserted =
      enabled_filters_.insert(filter);
  modified_ |= inserted.second;
}

void RewriteOptions::ForceEnableFilter(Filter filter) {
  DCHECK(!frozen_);

  // insert into set of enabled filters.
  std::pair<FilterSet::iterator, bool> inserted =
      enabled_filters_.insert(filter);
  modified_ |= inserted.second;

  // remove from set of disabled filters.
  modified_ |= disabled_filters_.erase(filter);
}

void RewriteOptions::EnableExtendCacheFilters() {
  EnableFilter(kExtendCacheCss);
  EnableFilter(kExtendCacheImages);
  EnableFilter(kExtendCacheScripts);
}

void RewriteOptions::DisableFilter(Filter filter) {
  DCHECK(!frozen_);
  std::pair<FilterSet::iterator, bool> inserted =
      disabled_filters_.insert(filter);
  modified_ |= inserted.second;
}

void RewriteOptions::EnableFilters(
    const RewriteOptions::FilterSet& filter_set) {
  for (RewriteOptions::FilterSet::const_iterator iter = filter_set.begin();
       iter != filter_set.end(); ++iter) {
    EnableFilter(*iter);
  }
}

void RewriteOptions::DisableFilters(
    const RewriteOptions::FilterSet& filter_set) {
  for (RewriteOptions::FilterSet::const_iterator iter = filter_set.begin();
       iter != filter_set.end(); ++iter) {
    DisableFilter(*iter);
  }
}

void RewriteOptions::ClearFilters() {
  DCHECK(!frozen_);
  modified_ = true;
  enabled_filters_.clear();
  disabled_filters_.clear();

  // Re-enable HtmlWriterFilter by default.
  EnableFilter(kHtmlWriterFilter);
}

bool RewriteOptions::AddCommaSeparatedListToFilterSetState(
    const StringPiece& filters, MessageHandler* handler, FilterSet* set) {
  DCHECK(!frozen_);
  size_t prev_set_size = set->size();
  bool ret = AddCommaSeparatedListToFilterSet(filters, handler, set);
  modified_ |= (set->size() != prev_set_size);
  return ret;
}

bool RewriteOptions::AddCommaSeparatedListToFilterSet(
    const StringPiece& filters, MessageHandler* handler, FilterSet* set) {
  StringPieceVector names;
  SplitStringPieceToVector(filters, ",", &names, true);
  bool ret = true;
  for (int i = 0, n = names.size(); i < n; ++i) {
    ret = AddByNameToFilterSet(names[i], handler, set);
  }
  return ret;
}

bool RewriteOptions::AddCommaSeparatedListToPlusAndMinusFilterSets(
    const StringPiece& filters, MessageHandler* handler,
    FilterSet* plus_set, FilterSet* minus_set) {
  DCHECK(!frozen_);
  StringPieceVector names;
  SplitStringPieceToVector(filters, ",", &names, true);
  bool ret = true;
  size_t sets_size_sum_before = (plus_set->size() + minus_set->size());
  for (int i = 0, n = names.size(); i < n; ++i) {
    StringPiece& option = names[i];
    if (!option.empty()) {
      if (option[0] == '-') {
        option.remove_prefix(1);
        ret = AddByNameToFilterSet(names[i], handler, minus_set);
      } else if (option[0] == '+') {
        option.remove_prefix(1);
        ret = AddByNameToFilterSet(names[i], handler, plus_set);
      } else {
        // No prefix is treated the same as '+'. Arbitrary but reasonable.
        ret = AddByNameToFilterSet(names[i], handler, plus_set);
      }
    }
  }
  size_t sets_size_sum_after = (plus_set->size() + minus_set->size());
  modified_ |= (sets_size_sum_before != sets_size_sum_after);
  return ret;
}

bool RewriteOptions::AddByNameToFilterSet(
    const StringPiece& option, MessageHandler* handler, FilterSet* set) {
  bool ret = true;
  Filter filter = LookupFilter(option);
  if (filter == kEndOfFilters) {
    // Handle a compound filter name.  This is much less common, so we don't
    // have any special infrastructure for it; just code.
    if (option == "rewrite_images") {
      set->insert(kInlineImages);
      set->insert(kRecompressImages);
      set->insert(kResizeImages);
    } else if (option == "extend_cache") {
      set->insert(kExtendCacheCss);
      set->insert(kExtendCacheImages);
      set->insert(kExtendCacheScripts);
    } else if (option == "testing") {
      for (int i = 0, n = arraysize(kTestFilterSet); i < n; ++i) {
        set->insert(kTestFilterSet[i]);
      }
    } else if (option == "core") {
      for (int i = 0, n = arraysize(kCoreFilterSet); i < n; ++i) {
        set->insert(kCoreFilterSet[i]);
      }
    } else if (option == "dangerous") {
      for (int i = 0, n = arraysize(kDangerousFilterSet); i < n; ++i) {
        set->insert(kDangerousFilterSet[i]);
      }
    } else {
      handler->Message(kWarning, "Invalid filter name: %s",
                       option.as_string().c_str());
      ret = false;
    }
  } else {
    set->insert(filter);
    // kResizeMobileImages requires kDelayImages.
    if (filter == kResizeMobileImages) {
      set->insert(kDelayImages);
    }
  }
  return ret;
}

RewriteOptions::OptionSettingResult RewriteOptions::SetOptionFromName(
    const StringPiece& name, const GoogleString& value, GoogleString* msg) {
  OptionEnum name_enum = LookupOption(name);
  if (name_enum == kEndOfOptions) {
    // Not a mapped option.
    SStringPrintf(msg, "Option %s not mapped.", name.as_string().c_str());
    return kOptionNameUnknown;
  }
  OptionBaseVector::iterator it = std::lower_bound(
      all_options_.begin(), all_options_.end(), name_enum,
      RewriteOptions::LessThanArg);
  OptionBase* option = *it;
  if (option->option_enum() == name_enum) {
    if (!option->SetFromString(value)) {
      SStringPrintf(msg, "Cannot set %s for option %s.", value.c_str(),
                    name.as_string().c_str());
      return kOptionValueInvalid;
    } else {
      return kOptionOk;
    }
  } else {
    // No Option with name_enum in all_options_.
    SStringPrintf(msg, "Option %s not found.", name.as_string().c_str());
    return kOptionNameUnknown;
  }
}

bool RewriteOptions::SetOptionFromNameAndLog(const StringPiece& name,
                                             const GoogleString& value,
                                             MessageHandler* handler) {
  GoogleString msg;
  OptionSettingResult result = SetOptionFromName(name, value, &msg);
  if (result == kOptionOk) {
    return true;
  } else {
    handler->Message(kWarning, "%s", msg.c_str());
    return false;
  }
}

bool RewriteOptions::Enabled(Filter filter) const {
  if (disabled_filters_.find(filter) != disabled_filters_.end()) {
    return false;
  }
  switch (level_.value()) {
    case kTestingCoreFilters:
      if (IsInSet(kTestFilterSet, arraysize(kTestFilterSet), filter)) {
        return true;
      }
      // fall through
    case kCoreFilters:
      if (IsInSet(kCoreFilterSet, arraysize(kCoreFilterSet), filter)) {
        return true;
      }
      break;
    case kAllFilters:
      if (!IsInSet(kDangerousFilterSet, arraysize(kDangerousFilterSet),
                   filter)) {
        return true;
      }
      break;
    case kPassThrough:
      break;
  }
  return enabled_filters_.find(filter) != enabled_filters_.end();
}

int64 RewriteOptions::ImageInlineMaxBytes() const {
  if (Enabled(kInlineImages)) {
    return image_inline_max_bytes_.value();
  } else {
    return 0;
  }
}

void RewriteOptions::set_image_inline_max_bytes(int64 x) {
  set_option(x, &image_inline_max_bytes_);
  if (!css_image_inline_max_bytes_.was_set() &&
      x > css_image_inline_max_bytes_.value()) {
    // Make sure css_image_inline_max_bytes is at least image_inline_max_bytes
    // if it has not been explicitly configured.
    css_image_inline_max_bytes_.set(x);
  }
}

int64 RewriteOptions::CssImageInlineMaxBytes() const {
  if (Enabled(kInlineImages)) {
    return css_image_inline_max_bytes_.value();
  } else {
    return 0;
  }
}

int64 RewriteOptions::MaxImageInlineMaxBytes() const {
  return std::max(ImageInlineMaxBytes(),
                  CssImageInlineMaxBytes());
}

void RewriteOptions::AddToPrioritizeVisibleContentCacheableFamilies(
    const StringPiece& str) {
  // We do not call Modify here, since
  // prioritize_visible_content_cacheable_families does not affect signature.
  prioritize_visible_content_cacheable_families_.Allow(str);
}

void RewriteOptions::Merge(const RewriteOptions& src) {
  DCHECK(!frozen_);
  modified_ |= src.modified_;
  for (FilterSet::const_iterator p = src.enabled_filters_.begin(),
           e = src.enabled_filters_.end(); p != e; ++p) {
    Filter filter = *p;
    // Enabling in 'second' trumps Disabling in first.
    disabled_filters_.erase(filter);
    enabled_filters_.insert(filter);
  }

  for (FilterSet::const_iterator p = src.disabled_filters_.begin(),
           e = src.disabled_filters_.end(); p != e; ++p) {
    Filter filter = *p;
    // Disabling in 'second' trumps enabling in anything.
    disabled_filters_.insert(filter);
    enabled_filters_.erase(filter);
  }

  for (int i = 0, n = src.furious_specs_.size(); i < n; ++i) {
    FuriousSpec* spec = src.furious_specs_[i]->Clone();
    AddFuriousSpec(spec);
  }

  // Note that from the perspective of this class, we can be merging
  // RewriteOptions subclasses & superclasses, so don't read anything
  // that doesn't exist.  However this is almost certainly the wrong
  // thing to do -- we should ensure that within a system all the
  // RewriteOptions that are instantiated are the same sublcass, so
  // DCHECK that they have the same number of options.
  size_t options_to_read = std::max(all_options_.size(),
                                    src.all_options_.size());
  DCHECK_EQ(all_options_.size(), src.all_options_.size());
  size_t options_to_merge = std::min(options_to_read, all_options_.size());
  for (size_t i = 0; i < options_to_merge; ++i) {
    all_options_[i]->Merge(src.all_options_[i]);
  }

  domain_lawyer_.Merge(src.domain_lawyer_);
  file_load_policy_.Merge(src.file_load_policy_);
  allow_resources_.AppendFrom(src.allow_resources_);
  retain_comments_.AppendFrom(src.retain_comments_);

  // Merge logic for prioritize visible content cacheable families.
  prioritize_visible_content_cacheable_families_.AppendFrom(
      src.prioritize_visible_content_cacheable_families_);

  if (src.panel_config() != NULL) {
    set_panel_config(new PublisherConfig(*(src.panel_config())));
  }
}

RewriteOptions::OptionInt64MergeWithMax::~OptionInt64MergeWithMax() {
}

void RewriteOptions::OptionInt64MergeWithMax::Merge(
    const OptionBase* src_base) {
  const OptionInt64MergeWithMax* src =
      static_cast<const OptionInt64MergeWithMax*>(src_base);
  if (src->was_set() && (!was_set() || (src->value() > value()))) {
    set(src->value());
  }
}

RewriteOptions* RewriteOptions::Clone() const {
  RewriteOptions* options = new RewriteOptions;
  options->Merge(*this);
  options->frozen_ = false;
  options->modified_ = false;
  return options;
}

GoogleString RewriteOptions::OptionSignature(const GoogleString& x,
                                             const Hasher* hasher) {
  return hasher->Hash(x);
}

GoogleString RewriteOptions::OptionSignature(RewriteLevel level,
                                             const Hasher* hasher) {
  switch (level) {
    case kPassThrough: return "p";
    case kCoreFilters: return "c";
    case kTestingCoreFilters: return "t";
    case kAllFilters: return "a";
  }
  return "?";
}

void RewriteOptions::ComputeSignature(const Hasher* hasher) {
  if (frozen_) {
    return;
  }

#ifndef NDEBUG
  if (!options_uniqueness_checked_) {
    options_uniqueness_checked_ = true;
    StringSet id_set;
    for (int i = 0, n = all_options_.size(); i < n; ++i) {
      const char* id = all_options_[i]->id();
      std::pair<StringSet::iterator, bool> insertion = id_set.insert(id);
      DCHECK(insertion.second) << "Duplicate RewriteOption option id: " << id;
    }
  }
#endif

  signature_ = IntegerToString(kOptionsVersion);
  for (int i = kFirstFilter; i != kEndOfFilters; ++i) {
    Filter filter = static_cast<Filter>(i);
    if (Enabled(filter)) {
      StrAppend(&signature_, "_", FilterId(filter));
    }
  }
  signature_ += "O";
  for (int i = 0, n = all_options_.size(); i < n; ++i) {
    // Keep the signature relatively short by only including options
    // with values overridden from the default.
    OptionBase* option = all_options_[i];
    if (option->was_set()) {
      StrAppend(&signature_, option->id(), ":", option->Signature(hasher), "_");
    }
  }
  StrAppend(&signature_, domain_lawyer_.Signature(), "_");
  frozen_ = true;

  // TODO(jmarantz): Incorporate signatures from the domain_lawyer,
  // file_load_policy, allow_resources, and retain_comments.  However,
  // the changes made here make our system strictly more correct than
  // it was before, using an ad-hoc signature in css_filter.cc.
}

GoogleString RewriteOptions::ToString(RewriteLevel level) {
  switch (level) {
    case kPassThrough: return "Pass Through";
    case kCoreFilters: return "Core Filters";
    case kTestingCoreFilters: return "Testing Core Filters";
    case kAllFilters: return "All Filters";
  }
  return "?";
}

GoogleString RewriteOptions::ToString() const {
  GoogleString output;
  StrAppend(&output, "Version: ", IntegerToString(kOptionsVersion), "\n\n");
  output += "Filters\n";
  for (int i = kFirstFilter; i != kEndOfFilters; ++i) {
    Filter filter = static_cast<Filter>(i);
    if (Enabled(filter)) {
      StrAppend(&output, FilterId(filter), "\t", FilterName(filter), "\n");
    }
  }
  output += "\nOptions\n";
  for (int i = 0, n = all_options_.size(); i < n; ++i) {
    // Only including options with values overridden from the default.
    OptionBase* option = all_options_[i];
    if (option->was_set()) {
      StrAppend(&output, "  ", option->id(), "\t", option->ToString(), "\n");
    }
  }
  // TODO(mmohabey): Incorporate ToString() from the domain_lawyer,
  // file_load_policy, allow_resources, and retain_comments.
  return output;
}

// TODO(nforman): This string may need to be significantly more compact to
// display in Google Analytics properly.
GoogleString RewriteOptions::ToExperimentString() const {
  GoogleString output;
  // Only add the experiment id if we're running this experiment.
  if (furious_id_ == furious::kFuriousControl ||
      GetFuriousSpec(furious_id_) != NULL) {
    output = StringPrintf("Experiment: %d; ", furious_id_);
  }
  for (int f = kFirstFilter; f != kEndOfFilters; ++f) {
    Filter filter = static_cast<Filter>(f);
    if (Enabled(filter)) {
      output += FilterId(filter);
      output += ",";
    }
  }
  output += "css:";
  output += Integer64ToString(css_inline_max_bytes());
  output += ",im:";
  output += Integer64ToString(ImageInlineMaxBytes());
  output += ",js:";
  output += Integer64ToString(js_inline_max_bytes());
  output += ";";
  return output;
}

void RewriteOptions::Modify() {
  DCHECK(!frozen_);
  modified_ = true;
}

const char* RewriteOptions::class_name() const {
  return RewriteOptions::kClassName;
}

// We expect furious_specs_.size() to be small (not more than 2 or 3)
// so there is no need to optimize this
RewriteOptions::FuriousSpec* RewriteOptions::GetFuriousSpec(int id) const {
  for (int i = 0, n = furious_specs_.size(); i < n; ++i) {
    if (furious_specs_[i]->id() == id) {
      return furious_specs_[i];
    }
  }
  return NULL;
}

bool RewriteOptions::AvailableFuriousId(int id) {
  if (id < 0 || id == furious::kFuriousNotSet ||
      id == furious::kFuriousNoExperiment ||
      id == furious::kFuriousControl) {
    return false;
  }
  return (GetFuriousSpec(id) == NULL);
}

bool RewriteOptions::AddFuriousSpec(const StringPiece& spec,
                                    MessageHandler* handler) {
  FuriousSpec* f_spec = new FuriousSpec(spec, handler);
  return AddFuriousSpec(f_spec);
}

bool RewriteOptions::AddFuriousSpec(int furious_id) {
  FuriousSpec* f_spec = new FuriousSpec(furious_id);
  return AddFuriousSpec(f_spec);
}

bool RewriteOptions::AddFuriousSpec(FuriousSpec* spec) {
  if (!AvailableFuriousId(spec->id())) {
    delete spec;
    return false;
  }
  furious_specs_.push_back(spec);
  return true;
}

// Always enable add_head, insert_ga, add_instrumentation,
// and HtmlWriter.  This is considered a "no-filter" base for
// furious experiments.
void RewriteOptions::SetupFuriousRewriters() {
  // Don't change anything if we're not in an experiment or have some
  // unset id.
  if (furious_id_ == furious::kFuriousNotSet ||
      furious_id_ == furious::kFuriousNoExperiment) {
    return;
  }
  // Control: just make sure that the necessary stuff is on.
  // Do NOT try to set up things to look like the FuriousSpec
  // for this id: it doesn't match the rewrite options.
  if (furious_id_ == furious::kFuriousControl) {
    ForceEnableFilter(RewriteOptions::kAddHead);
    ForceEnableFilter(RewriteOptions::kAddInstrumentation);
    ForceEnableFilter(RewriteOptions::kInsertGA);
    ForceEnableFilter(RewriteOptions::kHtmlWriterFilter);
    return;
  }
  FuriousSpec* spec = GetFuriousSpec(furious_id_);
  if (spec == NULL) {
    return;
  }
  ClearFilters();
  SetRewriteLevel(spec->rewrite_level());
  EnableFilters(spec->enabled_filters());
  DisableFilters(spec->disabled_filters());
  // We need these for the experiment to work properly.
  ForceEnableFilter(RewriteOptions::kAddHead);
  ForceEnableFilter(RewriteOptions::kAddInstrumentation);
  ForceEnableFilter(RewriteOptions::kInsertGA);
  ForceEnableFilter(RewriteOptions::kHtmlWriterFilter);
  set_css_inline_max_bytes(spec->css_inline_max_bytes());
  set_js_inline_max_bytes(spec->js_inline_max_bytes());
  set_image_inline_max_bytes(spec->image_inline_max_bytes());
}

RewriteOptions::FuriousSpec::FuriousSpec(const StringPiece& spec,
                                         MessageHandler* handler)
    : id_(furious::kFuriousNotSet),
      rewrite_level_(kPassThrough),
      css_inline_max_bytes_(kDefaultCssInlineMaxBytes),
      js_inline_max_bytes_(kDefaultJsInlineMaxBytes),
      image_inline_max_bytes_(kDefaultImageInlineMaxBytes) {
  Initialize(spec, handler);
}

RewriteOptions::FuriousSpec::FuriousSpec(int id)
    : id_(id),
      rewrite_level_(kPassThrough),
      css_inline_max_bytes_(kDefaultCssInlineMaxBytes),
      js_inline_max_bytes_(kDefaultJsInlineMaxBytes),
      image_inline_max_bytes_(kDefaultImageInlineMaxBytes) {
}

RewriteOptions::FuriousSpec* RewriteOptions::FuriousSpec::Clone() {
  FuriousSpec* ret = new FuriousSpec(id_);
  for (FilterSet::const_iterator iter = enabled_filters_.begin();
       iter != enabled_filters_.end(); ++iter) {
    ret->enabled_filters_.insert(*iter);
  }
  for (FilterSet::const_iterator iter = disabled_filters_.begin();
       iter != disabled_filters_.end(); ++iter) {
    ret->disabled_filters_.insert(*iter);
  }
  ret->rewrite_level_ = rewrite_level_;
  ret->css_inline_max_bytes_ = css_inline_max_bytes_;
  ret->js_inline_max_bytes_ = js_inline_max_bytes_;
  ret->image_inline_max_bytes_ = image_inline_max_bytes_;
  return ret;
}

StringPiece RewriteOptions::FuriousSpec::PieceAfterEquals(
    const StringPiece& piece) {
  size_t index = piece.find("=");
  if (index != piece.npos) {
    ++index;
    StringPiece ret = piece;
    ret.remove_prefix(index);
    TrimWhitespace(&ret);
    return ret;
  }
  return StringPiece(piece.data(), 0);
}

// Options are written in the form:
// ModPagespeedExperimentSpec 'id= 2; RewriteLevel= CoreFilters;
// enable= resize_images; disable = is; inline_css = 25556'
void RewriteOptions::FuriousSpec::Initialize(const StringPiece& spec,
                                             MessageHandler* handler) {
  StringPieceVector spec_pieces;
  SplitStringPieceToVector(spec, ";", &spec_pieces, true);
  for (int i = 0, n = spec_pieces.size(); i < n; ++i) {
    StringPiece piece = spec_pieces[i];
    TrimWhitespace(&piece);
    if (StringCaseStartsWith(piece, "id")) {
      StringPiece id = PieceAfterEquals(piece);
      if (id.length() > 0 && !StringToInt(id.as_string(), &id_)) {
        // If we failed to turn this string into an int, then
        // set the id_ to kFuriousNotSet so we don't end up adding
        // in this spec.
        id_ = furious::kFuriousNotSet;
      }
    } else if (StringCaseStartsWith(piece, "level")) {
      StringPiece level = PieceAfterEquals(piece);
      if (level.length() > 0) {
        ParseRewriteLevel(level, &rewrite_level_);
      }
    } else if (StringCaseStartsWith(piece, "enable")) {
      StringPiece enabled = PieceAfterEquals(piece);
      if (enabled.length() > 0) {
        AddCommaSeparatedListToFilterSet(enabled, handler, &enabled_filters_);
      }
    } else if (StringCaseStartsWith(piece, "disable")) {
      StringPiece disabled = PieceAfterEquals(piece);
      if (disabled.length() > 0) {
        AddCommaSeparatedListToFilterSet(disabled, handler, &disabled_filters_);
      }
    } else if (StringCaseStartsWith(piece, "inline_css")) {
      StringPiece max_bytes = PieceAfterEquals(piece);
      if (max_bytes.length() > 0) {
        StringToInt64(max_bytes.as_string(), &css_inline_max_bytes_);
      }
    } else if (StringCaseStartsWith(piece, "inline_images")) {
      StringPiece max_bytes = PieceAfterEquals(piece);
      if (max_bytes.length() > 0) {
        StringToInt64(max_bytes.as_string(), &image_inline_max_bytes_);
      }
    } else if (StringCaseStartsWith(piece, "inline_js")) {
      StringPiece max_bytes = PieceAfterEquals(piece);
      if (max_bytes.length() > 0) {
        StringToInt64(max_bytes.as_string(), &js_inline_max_bytes_);
      }
    }
  }
}

}  // namespace net_instaweb
