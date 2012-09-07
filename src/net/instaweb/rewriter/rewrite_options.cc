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
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/furious_util.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/null_rw_lock.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/timer.h"

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
const char RewriteOptions::kLocalStorageCacheId[] = "ls";
const char RewriteOptions::kPanelCommentPrefix[] = "GooglePanel";

// Sets limit for buffering html in blink secondary fetch to 10MB default.
const int64 RewriteOptions::kDefaultBlinkMaxHtmlSizeRewritable =
    10 * 1024 * 1024;

// If positive, the overridden default cache-time for cacheable resources in
// blink.
const int64 RewriteOptions::kDefaultOverrideBlinkCacheTimeMs = -1;

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
const int64 RewriteOptions::kDefaultCssFlattenMaxBytes = 2048;
const int64 RewriteOptions::kDefaultCssImageInlineMaxBytes = 2048;
const int64 RewriteOptions::kDefaultCssOutlineMinBytes = 3000;
const int64 RewriteOptions::kDefaultImageInlineMaxBytes = 2048;
const int64 RewriteOptions::kDefaultJsInlineMaxBytes = 2048;
const int64 RewriteOptions::kDefaultJsOutlineMinBytes = 3000;
const int64 RewriteOptions::kDefaultProgressiveJpegMinBytes = 10240;

const int64 RewriteOptions::kDefaultMaxHtmlCacheTimeMs = 0;
const int64 RewriteOptions::kDefaultMaxImageBytesForWebpInCss = kint64max;

const int64 RewriteOptions::kDefaultMinResourceCacheTimeToRewriteMs = 0;

const int64 RewriteOptions::kDefaultCacheInvalidationTimestamp = -1;
const int64 RewriteOptions::kDefaultFlushBufferLimitBytes = 100 * 1024;
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

// Sets limit for image optimization to 32MB.
const int64 RewriteOptions::kDefaultImageResolutionLimitBytes = 32*1024*1024;

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

const char RewriteOptions::kDefaultBeaconUrl[] = "/mod_pagespeed_beacon?ets=";

const int RewriteOptions::kDefaultMaxInlinedPreviewImagesIndex = 5;
const int64 RewriteOptions::kDefaultMinImageSizeLowResolutionBytes = 1 * 1024;
const int64 RewriteOptions::kDefaultMaxImageSizeLowResolutionBytes =
    1 * 1024 * 1024;  // 1 MB.

// Setting the limit on combined js resource to -1 will bypass the size check.
const int64 RewriteOptions::kDefaultMaxCombinedJsBytes = -1;
const int64 RewriteOptions::kDefaultCriticalImagesCacheExpirationMs =
    Timer::kHourMs;
const int64 RewriteOptions::kDefaultMetadataCacheStalenessThresholdMs = 0;
const int RewriteOptions::kDefaultFuriousTrafficPercent = 50;
const int RewriteOptions::kDefaultFuriousSlot = 1;

const char RewriteOptions::kClassName[] = "RewriteOptions";

const char RewriteOptions::kDefaultBlinkDesktopUserAgentValue[] =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/536.5 "
    "(KHTML, like Gecko) Chrome/19.0.1084.46 Safari/536.5";

// An empty default key indicates that the blocking rewrite feature is disabled.
const char RewriteOptions::kDefaultBlockingRewriteKey[] = "";

// Allow all the declared shards.
const int RewriteOptions::kDefaultDomainShardCount = 0;

const int64 RewriteOptions::kDefaultBlinkHtmlChangeDetectionTimeMs =
    Timer::kMinuteMs;

const char* RewriteOptions::option_enum_to_name_array_[
    RewriteOptions::kEndOfOptions];

RewriteOptions::Properties* RewriteOptions::properties_ = NULL;
RewriteOptions::Properties* RewriteOptions::all_properties_ = NULL;

namespace {

const RewriteOptions::Filter kCoreFilterSet[] = {
  RewriteOptions::kAddHead,
  RewriteOptions::kCombineCss,
  RewriteOptions::kConvertGifToPng,
  RewriteOptions::kConvertMetaTags,
  RewriteOptions::kConvertPngToJpeg,
  RewriteOptions::kExtendCacheCss,
  RewriteOptions::kExtendCacheImages,
  RewriteOptions::kExtendCacheScripts,
  RewriteOptions::kInlineCss,
  RewriteOptions::kInlineImages,
  RewriteOptions::kInlineImportToLink,
  RewriteOptions::kInlineJavascript,
  RewriteOptions::kJpegSubsampling,
  RewriteOptions::kLeftTrimUrls,
  RewriteOptions::kRecompressJpeg,
  RewriteOptions::kRecompressPng,
  RewriteOptions::kRecompressWebp,
  RewriteOptions::kResizeImages,
  RewriteOptions::kRewriteCss,
  RewriteOptions::kRewriteJavascript,
  RewriteOptions::kRewriteStyleAttributesWithUrl,
  RewriteOptions::kStripImageColorProfile,
  RewriteOptions::kStripImageMetaData,
};

// Note: all Core filters are Test filters as well.  For maintainability,
// this is managed in the c++ switch statement.
const RewriteOptions::Filter kTestFilterSet[] = {
  RewriteOptions::kConvertJpegToWebp,
  // TODO(sligocki): Move kFallbackRewriteCssUrls to kCoreFilterSet when ready.
  RewriteOptions::kFallbackRewriteCssUrls,
  RewriteOptions::kInsertGA,
  RewriteOptions::kInsertImageDimensions,
  RewriteOptions::kLeftTrimUrls,
  RewriteOptions::kMakeGoogleAnalyticsAsync,
  RewriteOptions::kRewriteDomains,
  RewriteOptions::kSpriteImages,
};

// Note: These filters should not be included even if the level is "All".
const RewriteOptions::Filter kDangerousFilterSet[] = {
  RewriteOptions::kComputePanelJson,  // internal, enabled conditionally
  RewriteOptions::kComputeVisibleText,
  RewriteOptions::kDeferIframe,
  RewriteOptions::kDeferJavascript,
  RewriteOptions::kDetectReflowWithDeferJavascript,
  RewriteOptions::kDeterministicJs,   // used for measurement
  RewriteOptions::kDisableJavascript,
  RewriteOptions::kDivStructure,
  RewriteOptions::kExperimentSpdy,
  RewriteOptions::kExplicitCloseTags,
  RewriteOptions::kLazyloadImages,
  RewriteOptions::kProcessBlinkInBackground,  // internal,
                                              // enabled conditionally
  RewriteOptions::kServeNonCacheableNonCritical,  // internal,
                                                  // enabled conditionally
  RewriteOptions::kSplitHtml,  // internal, enabled conditionally
  RewriteOptions::kStripNonCacheable,  // internal, enabled conditionally
  RewriteOptions::kStripScripts,
};

// List of filters whose correct behaviour requires script execuction.
const RewriteOptions::Filter kRequiresScriptExecutionFilterSet[] = {
  RewriteOptions::kDeferIframe,
  RewriteOptions::kDeferJavascript,
  RewriteOptions::kDelayImages,
  RewriteOptions::kDetectReflowWithDeferJavascript,
  RewriteOptions::kLazyloadImages,
  RewriteOptions::kLocalStorageCache,
  // We do not include kPrioritizeVisibleContent since we do not want to attach
  // SupportNoscriptFilter in the case of blink pcache miss pass-through, since
  // this response will not have any custom script inserted.
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
    case kAddBaseTag:                      return "Add Base Tag";
    case kAddHead:                         return "Add Head";
    case kAddInstrumentation:              return "Add Instrumentation";
    case kCollapseWhitespace:              return "Collapse Whitespace";
    case kCombineCss:                      return "Combine Css";
    case kCombineHeads:                    return "Combine Heads";
    case kCombineJavascript:               return "Combine Javascript";
    case kComputePanelJson:                return "Computes panel json";
    case kComputeVisibleText:              return "Computes visible text";
    case kConvertGifToPng:                 return "Convert Gif to Png";
    case kConvertJpegToProgressive:        return "Convert Jpeg to Progressive";
    case kConvertJpegToWebp:               return "Convert Jpeg To Webp";
    case kConvertMetaTags:                 return "Convert Meta Tags";
    case kConvertPngToJpeg:                return "Convert Png to Jpeg";
    case kDebug:                           return "Debug";
    case kDeferIframe:                     return "Defer Iframe";
    case kDeferJavascript:                 return "Defer Javascript";
    case kDelayImages:                     return "Delay Images";
    case kDetectReflowWithDeferJavascript:
      return "Detect Reflow With Defer Javascript";
    case kDeterministicJs:                 return "Deterministic Js";
    case kDisableJavascript:
      return "Disables scripts by placing them inside noscript tags";
    case kDivStructure:                    return "Div Structure";
    case kElideAttributes:                 return "Elide Attributes";
    case kExperimentSpdy:                  return "SPDY Resources Experiment";
    case kExplicitCloseTags:               return "Explicit Close Tags";
    case kExtendCacheCss:                  return "Cache Extend Css";
    case kExtendCacheImages:               return "Cache Extend Images";
    case kExtendCachePdfs:                 return "Cache Extend PDFs";
    case kExtendCacheScripts:              return "Cache Extend Scripts";
    case kFallbackRewriteCssUrls:          return "Fallback Rewrite Css Urls";
    case kFlattenCssImports:               return "Flatten CSS Imports";
    case kFlushSubresources:               return "Flush Subresources";
    case kHandleNoscriptRedirect:          return "Handles Noscript Redirects";
    case kHtmlWriterFilter:                return "Flushes html";
    case kInlineCss:                       return "Inline Css";
    case kInlineImages:                    return "Inline Images";
    case kInlineImportToLink:              return "Inline @import to Link";
    case kInlineJavascript:                return "Inline Javascript";
    case kInsertDnsPrefetch:               return "Insert DNS Prefetch";
    case kInsertGA:                        return "Insert Google Analytics";
    case kInsertImageDimensions:           return "Insert Image Dimensions";
    case kJpegSubsampling:                 return "Jpeg Subsampling";
    case kLazyloadImages:                  return "Lazyload Images";
    case kLeftTrimUrls:                    return "Left Trim Urls";
    case kLocalStorageCache:               return "Local Storage Cache";
    case kMakeGoogleAnalyticsAsync:        return "Make Google Analytics Async";
    case kMoveCssAboveScripts:             return "Move Css Above Scripts";
    case kMoveCssToHead:                   return "Move Css To Head";
    case kOutlineCss:                      return "Outline Css";
    case kOutlineJavascript:               return "Outline Javascript";
    case kPrioritizeVisibleContent:        return "Prioritize Visible Content";
    case kProcessBlinkInBackground:        return "Blink Background Processing";
    case kRecompressJpeg:                  return "Recompress Jpeg";
    case kRecompressPng:                   return "Recompress Png";
    case kRecompressWebp:                  return "Recompress Webp";
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
    case kSplitHtml:                       return "Split Html";
    case kStripNonCacheable:               return "Strip Non Cacheable";
    case kStripImageColorProfile:          return "Strip Image Color Profiles";
    case kStripImageMetaData:              return "Strip Image Meta Data";
    case kStripScripts:                    return "Strip Scripts";
    case kEndOfFilters:                    return "End of Filters";
  }
  return "Unknown Filter";
}

const char* RewriteOptions::FilterId(Filter filter) {
  switch (filter) {
    case kAddBaseTag:                      return "ab";
    case kAddHead:                         return "ah";
    case kAddInstrumentation:              return "ai";
    case kCollapseWhitespace:              return "cw";
    case kCombineCss:                      return kCssCombinerId;
    case kCombineHeads:                    return "ch";
    case kCombineJavascript:               return kJavascriptCombinerId;
    case kComputePanelJson:                return "cv";
    case kComputeVisibleText:              return "bp";
    case kConvertGifToPng:                 return "gp";
    case kConvertJpegToProgressive:        return "jp";
    case kConvertJpegToWebp:               return "jw";
    case kConvertMetaTags:                 return "mc";
    case kConvertPngToJpeg:                return "pj";
    case kDebug:                           return "db";
    case kDeferIframe:                     return "df";
    case kDeferJavascript:                 return "dj";
    case kDelayImages:                     return "di";
    case kDetectReflowWithDeferJavascript: return "dr";
    case kDeterministicJs:                 return "mj";
    case kDisableJavascript:               return "jd";
    case kDivStructure:                    return "ds";
    case kElideAttributes:                 return "ea";
    case kExperimentSpdy:                  return "xs";
    case kExplicitCloseTags:               return "xc";
    case kExtendCacheCss:                  return "ec";
    case kExtendCacheImages:               return "ei";
    case kExtendCachePdfs:                 return "ep";
    case kExtendCacheScripts:              return "es";
    case kFallbackRewriteCssUrls:          return "fc";
    case kFlattenCssImports:               return kCssImportFlattenerId;
    case kFlushSubresources:               return "fs";
    case kHandleNoscriptRedirect:          return "hn";
    case kHtmlWriterFilter:                return "hw";
    case kInlineCss:                       return kCssInlineId;
    case kInlineImages:                    return "ii";
    case kInlineImportToLink:              return "il";
    case kInlineJavascript:                return kJavascriptInlineId;
    case kInsertDnsPrefetch:               return "idp";
    case kInsertGA:                        return "ig";
    case kInsertImageDimensions:           return "id";
    case kJpegSubsampling:                 return "js";
    case kLazyloadImages:                  return "ll";
    case kLeftTrimUrls:                    return "tu";
    case kLocalStorageCache:               return kLocalStorageCacheId;
    case kMakeGoogleAnalyticsAsync:        return "ga";
    case kMoveCssAboveScripts:             return "cj";
    case kMoveCssToHead:                   return "cm";
    case kOutlineCss:                      return "co";
    case kOutlineJavascript:               return "jo";
    case kPrioritizeVisibleContent:        return "pv";
    case kProcessBlinkInBackground:        return "bb";
    case kRecompressJpeg:                  return "rj";
    case kRecompressPng:                   return "rp";
    case kRecompressWebp:                  return "rw";
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
    case kSplitHtml:                       return "sh";
    case kStripNonCacheable:               return "nc";
    case kSpriteImages:                    return kImageCombineId;
    case kStripImageColorProfile:          return "cp";
    case kStripImageMetaData:              return "md";
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

bool RewriteOptions::ParseBeaconUrl(const StringPiece& in, BeaconUrl* out) {
  StringPieceVector urls;
  SplitStringPieceToVector(in, " ", &urls, true);

  if (urls.size() > 2 || urls.size() < 1) {
    return false;
  }
  urls[0].CopyToString(&out->http);
  if (urls.size() == 2) {
    urls[1].CopyToString(&out->https);
    return true;
  }
  if (urls[0].starts_with("http:")) {
    out->https.clear();
    StrAppend(&out->https, "https:", urls[0].substr(STATIC_STRLEN("http:")));
  } else {
    urls[0].CopyToString(&out->https);
  }
  return true;
}

bool RewriteOptions::ImageOptimizationEnabled() const {
  return (this->Enabled(RewriteOptions::kRecompressJpeg) ||
          this->Enabled(RewriteOptions::kRecompressPng) ||
          this->Enabled(RewriteOptions::kRecompressWebp) ||
          this->Enabled(RewriteOptions::kConvertGifToPng) ||
          this->Enabled(RewriteOptions::kConvertPngToJpeg) ||
          this->Enabled(RewriteOptions::kConvertJpegToWebp));
}

RewriteOptions::RewriteOptions()
    : modified_(false),
      frozen_(false),
      initialized_options_(0),
      options_uniqueness_checked_(false),
      need_to_store_experiment_data_(false),
      furious_id_(furious::kFuriousNotSet),
      furious_percent_(0),
      url_valued_attributes_(NULL) {
  DCHECK(properties_ != NULL)
      << "Call RewriteOptions::Initialize() before construction";

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

  InitializeOptions(properties_);

  // Enable HtmlWriterFilter by default.
  EnableFilter(kHtmlWriterFilter);
}

// static
void RewriteOptions::AddProperties() {
  add_option(kPassThrough, &RewriteOptions::level_, "l", kRewriteLevel);
  add_option(kDefaultBlinkMaxHtmlSizeRewritable,
             &RewriteOptions::blink_max_html_size_rewritable_,
             "bmhsr", kBlinkMaxHtmlSizeRewritable);
  add_option(kDefaultCssFlattenMaxBytes,
             &RewriteOptions::css_flatten_max_bytes_, "cf",
             kCssFlattenMaxBytes);
  add_option(kDefaultCssImageInlineMaxBytes,
             &RewriteOptions::css_image_inline_max_bytes_,
             "cii", kCssImageInlineMaxBytes);
  add_option(kDefaultCssInlineMaxBytes,
             &RewriteOptions::css_inline_max_bytes_, "ci",
             kCssInlineMaxBytes);
  add_option(kDefaultCssOutlineMinBytes,
             &RewriteOptions::css_outline_min_bytes_, "co",
             kCssOutlineMinBytes);
  add_option(kDefaultImageInlineMaxBytes,
             &RewriteOptions::image_inline_max_bytes_, "ii",
             kImageInlineMaxBytes);
  add_option(kDefaultJsInlineMaxBytes,
             &RewriteOptions::js_inline_max_bytes_, "ji",
             kJsInlineMaxBytes);
  add_option(kDefaultJsOutlineMinBytes,
             &RewriteOptions::js_outline_min_bytes_, "jo",
             kJsOutlineMinBytes);
  add_option(kDefaultProgressiveJpegMinBytes,
             &RewriteOptions::progressive_jpeg_min_bytes_,
             "jp", kProgressiveJpegMinBytes);
  add_option(kDefaultMaxHtmlCacheTimeMs,
             &RewriteOptions::max_html_cache_time_ms_, "hc",
             kMaxHtmlCacheTimeMs);
  add_option(kDefaultMaxImageBytesForWebpInCss,
             &RewriteOptions::max_image_bytes_for_webp_in_css_, "miwc",
             kMaxImageBytesForWebpInCss);
  add_option(kDefaultMinResourceCacheTimeToRewriteMs,
             &RewriteOptions::min_resource_cache_time_to_rewrite_ms_, "rc",
             kMinResourceCacheTimeToRewriteMs);
  add_option(kDefaultCacheInvalidationTimestamp,
             &RewriteOptions::cache_invalidation_timestamp_, "it",
             kCacheInvalidationTimestamp);
  add_option(kDefaultIdleFlushTimeMs,
             &RewriteOptions::idle_flush_time_ms_, "if",
             kIdleFlushTimeMs);
  add_option(kDefaultFlushBufferLimitBytes,
             &RewriteOptions::flush_buffer_limit_bytes_, "fbl",
             kFlushBufferLimitBytes);
  add_option(kDefaultImplicitCacheTtlMs,
             &RewriteOptions::implicit_cache_ttl_ms_, "ict",
             kImplicitCacheTtlMs);
  add_option(kDefaultImageMaxRewritesAtOnce,
             &RewriteOptions::image_max_rewrites_at_once_,
             "im", kImageMaxRewritesAtOnce);
  add_option(kDefaultMaxUrlSegmentSize, &RewriteOptions::max_url_segment_size_,
             "uss", kMaxUrlSegmentSize);
  add_option(kDefaultMaxUrlSize, &RewriteOptions::max_url_size_, "us",
             kMaxUrlSize);
  add_option(true, &RewriteOptions::enabled_, "e", kEnabled);
  add_option(false, &RewriteOptions::ajax_rewriting_enabled_, "ar",
             kAjaxRewritingEnabled);
  add_option(true, &RewriteOptions::combine_across_paths_, "cp",
             kCombineAcrossPaths);
  add_option(false, &RewriteOptions::log_rewrite_timing_, "lr",
             kLogRewriteTiming);
  add_option(false, &RewriteOptions::lowercase_html_names_, "lh",
             kLowercaseHtmlNames);
  add_option(false, &RewriteOptions::always_rewrite_css_, "arc",
             kAlwaysRewriteCss);
  add_option(false, &RewriteOptions::respect_vary_, "rv", kRespectVary);
  add_option(false, &RewriteOptions::flush_html_, "fh", kFlushHtml);
  add_option(true, &RewriteOptions::serve_stale_if_fetch_error_, "ss",
             kServeStaleIfFetchError);
  add_option(false, &RewriteOptions::enable_defer_js_experimental_, "edje",
             kEnableDeferJsExperimental);
  add_option(true, &RewriteOptions::enable_flush_subresources_experimental_,
             "efse", kEnableFlushSubresourcesExperimental);
  add_option(false, &RewriteOptions::enable_inline_preview_images_experimental_,
             "eipie", kEnableInlinePreviewImagesExperimental);
  add_option(false, &RewriteOptions::enable_blink_critical_line_, "ebcl",
             kEnableBlinkCriticalLine);
  add_option(false, &RewriteOptions::default_cache_html_, "dch",
             kDefaultCacheHtml);
  add_option(kDefaultDomainShardCount, &RewriteOptions::domain_shard_count_,
             "dsc", kDomainShardCount);
  add_option(true, &RewriteOptions::modify_caching_headers_, "mch",
             kModifyCachingHeaders);
  // This is not Plain Old Data, so we initialize it here.
  const RewriteOptions::BeaconUrl kDefaultBeaconUrls =
      { kDefaultBeaconUrl, kDefaultBeaconUrl };
  add_option(kDefaultBeaconUrls, &RewriteOptions::beacon_url_, "bu",
             kBeaconUrl);
  add_option(false, &RewriteOptions::lazyload_images_after_onload_, "llio",
             kLazyloadImagesAfterOnload);
  add_option(true, &RewriteOptions::inline_only_critical_images_, "ioci",
             kInlineOnlyCriticalImages);
  add_option(false, &RewriteOptions::domain_rewrite_hyperlinks_, "drh",
             kDomainRewriteHyperlinks);
  add_option(false, &RewriteOptions::client_domain_rewrite_, "cdr",
             kClientDomainRewrite);
  add_option(kDefaultImageJpegRecompressQuality,
             &RewriteOptions::image_jpeg_recompress_quality_, "iq",
             kImageJpegRecompressionQuality);
  add_option(kDefaultImageLimitOptimizedPercent,
             &RewriteOptions::image_limit_optimized_percent_, "ip",
             kImageLimitOptimizedPercent);
  add_option(kDefaultImageLimitResizeAreaPercent,
             &RewriteOptions::image_limit_resize_area_percent_, "ia",
             kImageLimitResizeAreaPercent);
  add_option(kDefaultImageWebpRecompressQuality,
             &RewriteOptions::image_webp_recompress_quality_, "iw",
             kImageWebpRecompressQuality);
  add_option(kDefaultMaxInlinedPreviewImagesIndex,
             &RewriteOptions::max_inlined_preview_images_index_, "mdii",
             kMaxInlinedPreviewImagesIndex);
  add_option(kDefaultMinImageSizeLowResolutionBytes,
             &RewriteOptions::min_image_size_low_resolution_bytes_, "nislr",
             kMinImageSizeLowResolutionBytes);
  add_option(kDefaultMaxImageSizeLowResolutionBytes,
             &RewriteOptions::max_image_size_low_resolution_bytes_, "xislr",
             kMaxImageSizeLowResolutionBytes);
  add_option(kDefaultCriticalImagesCacheExpirationMs,
             &RewriteOptions::critical_images_cache_expiration_time_ms_, "cice",
             kCriticalImagesCacheExpirationTimeMs);
  add_option(kDefaultImageJpegNumProgressiveScans,
             &RewriteOptions::image_jpeg_num_progressive_scans_, "ijps",
             kImageJpegNumProgressiveScans);
  add_option(false, &RewriteOptions::cache_small_images_unrewritten_, "csiu",
             kCacheSmallImagesUnrewritten);
  add_option(kDefaultImageResolutionLimitBytes,
             &RewriteOptions::image_resolution_limit_bytes_,
             "irlb", kImageResolutionLimitBytes);
  add_option(false, &RewriteOptions::image_retain_color_profile_, "ircp",
             kImageRetainColorProfile);
  add_option(false, &RewriteOptions::image_retain_color_sampling_, "ircs",
             kImageRetainColorSampling);
  add_option(false, &RewriteOptions::image_retain_exif_data_, "ired",
             kImageRetainExifData);
  add_option("", &RewriteOptions::ga_id_, "ig", kAnalyticsID);
  add_option(true, &RewriteOptions::increase_speed_tracking_, "st",
             kIncreaseSpeedTracking);
  add_option(false, &RewriteOptions::running_furious_, "fur", kRunningFurious);
  add_option(kDefaultFuriousSlot, &RewriteOptions::furious_ga_slot_, "fga",
             kFuriousSlot);
  add_option(false, &RewriteOptions::report_unload_time_, "rut",
             kReportUnloadTime);
  add_option("", &RewriteOptions::x_header_value_, "xhv",
             kXModPagespeedHeaderValue);
  add_option(false, &RewriteOptions::avoid_renaming_introspective_javascript_,
             "aris", kAvoidRenamingIntrospectiveJavascript);
  add_option(false, &RewriteOptions::enable_blink_for_mobile_devices_, "ebmd",
             kEnableBlinkForMobileDevices);
  add_option(false,
             &RewriteOptions::use_fixed_user_agent_for_blink_cache_misses_,
             "ufua", kUseFixedUserAgentForBlinkCacheMisses);
  add_option(kDefaultBlinkDesktopUserAgentValue,
             &RewriteOptions::blink_desktop_user_agent_, "bdua",
             kBlinkDesktopUserAgent);
  add_option(false,
             &RewriteOptions::passthrough_blink_for_last_invalid_response_code_,
             "ptbi", kPassthroughBlinkForInvalidResponseCode);
  add_option(false, &RewriteOptions::use_full_url_in_blink_families_, "bffu",
             kUseFullUrlInBlinkFamilies);
  add_option(false, &RewriteOptions::reject_blacklisted_, "rbl",
             kRejectBlacklisted);
  add_option(HttpStatus::kForbidden,
             &RewriteOptions::reject_blacklisted_status_code_, "rbls",
             kRejectBlacklistedStatusCode);
  add_option(kDefaultBlockingRewriteKey, &RewriteOptions::blocking_rewrite_key_,
             "blrw", kXPsaBlockingRewrite);
  add_option(true, &RewriteOptions::support_noscript_enabled_, "snse",
             kSupportNoScriptEnabled);
  add_option(kDefaultMaxCombinedJsBytes,
             &RewriteOptions::max_combined_js_bytes_, "xcj",
             kMaxCombinedJsBytes);
  add_option(false, &RewriteOptions::enable_blink_html_change_detection_,
             "ebhcd", kEnableBlinkHtmlChangeDetection);
  add_option(false,
             &RewriteOptions::enable_blink_html_change_detection_logging_,
             "ebhcdl", kEnableBlinkHtmlChangeDetectionLogging);
  add_option("", &RewriteOptions::critical_line_config_, "clc",
             kCriticalLineConfig);
  add_option(-1, &RewriteOptions::override_caching_ttl_ms_, "octm",
             kOverrideCachingTtlMs);
  add_option(5 * Timer::kSecondMs, &RewriteOptions::blocking_fetch_timeout_ms_,
             "bfto", RewriteOptions::kFetcherTimeOutMs);
  add_option(false, &RewriteOptions::enable_lazyload_in_blink_, "elib");
  add_option(kDefaultMetadataCacheStalenessThresholdMs,
             &RewriteOptions::metadata_cache_staleness_threshold_ms_, "mcst");
  add_option(false,
             &RewriteOptions::RewriteOptions::apply_blink_if_no_families_,
             "abnf");
  add_option(true, &RewriteOptions::enable_blink_debug_dashboard_, "ebdd");
  add_option(kDefaultOverrideBlinkCacheTimeMs,
             &RewriteOptions::override_blink_cache_time_ms_, "obctm");
  add_option("", &RewriteOptions::lazyload_images_blank_url_, "llbu");
  add_option(kDefaultBlinkHtmlChangeDetectionTimeMs,
             &RewriteOptions::blink_html_change_detection_time_ms_, "bhcdt");
  add_option(false, &RewriteOptions::override_ie_document_mode_, "oidm");

  //
  // Recently sriharis@ excluded a variety of options from
  // signature-computation which makes sense from the perspective
  // of metadata cache, however it makes Signature() useless for
  // determining equivalence of RewriteOptions.  This equivalence
  // is needed in ResourceManager::NewRewriteDriver to determine
  // whether the drivers in the freelist are still applicable, or
  // whether options have changed.
  //
  // So we need to either compute two signatures: one for equivalence
  // and one for metadata cache key, or just use the more comprehensive
  // one for metadata_cache.  We should determine whether we are getting
  // spurious cache fragmentation before investing in computing two
  // signatures.
  //
  // Commenting these out for now.
  //
  // In particular, ProxyInterfaceTest.AjaxRewritingForCss will fail
  // if we don't let ajax_rewriting_enabled_ affect the signature.
  //
  // TODO(jmarantz): consider whether there's any measurable benefit
  // from excluding these options from the signature.  If there is,
  // make 2 signatures: one for equivalence & one for metadata cache
  // keys.  If not, just remove the DoNotUseForSignatureComputation
  // infrastructure.
  //
  // ajax_rewriting_enabled_.DoNotUseForSignatureComputation();
  // log_rewrite_timing_.DoNotUseForSignatureComputation();
  // serve_stale_if_fetch_error_.DoNotUseForSignatureComputation();
  // enable_defer_js_experimental_.DoNotUseForSignatureComputation();
  // enable_blink_critical_line_.DoNotUseForSignatureComputation();
  // serve_blink_non_critical_.DoNotUseForSignatureComputation();
  // default_cache_html_.DoNotUseForSignatureComputation();
  // lazyload_images_after_onload_.DoNotUseForSignatureComputation();
  // ga_id_.DoNotUseForSignatureComputation();
  // increase_speed_tracking_.DoNotUseForSignatureComputation();
  // running_furious_.DoNotUseForSignatureComputation();
  // x_header_value_.DoNotUseForSignatureComputation();
  // blocking_fetch_timeout_ms_.DoNotUseForSignatureComputation();
}

RewriteOptions::~RewriteOptions() {
  STLDeleteElements(&custom_fetch_headers_);
  STLDeleteElements(&furious_specs_);
  STLDeleteElements(&prioritize_visible_content_families_);
  STLDeleteElements(&url_cache_invalidation_entries_);
}

void RewriteOptions::InitializeOptions(const Properties* properties) {
  all_options_.resize(all_properties_->size());

  // Note that we reserve space in all_options_ for all RewriteOptions
  // and subclass properties, but we initialize only the options
  // corresponding to the ones passed into this method, whether from
  // RewriteOptions or a subclass.
  //
  // This is because the member variables for the subclass properties
  // have not been constructed yet, so copying default values into
  // them would crash (at least the strings).  So we rely on subclass
  // constructors to initialize their own options by calling
  // InitializeOptions on their own property sets as well.
  for (int i = 0, n = properties->size(); i < n; ++i) {
    const PropertyBase* property = properties->property(i);
    property->InitializeOption(this);
  }
  initialized_options_ += properties->size();
}

RewriteOptions::OptionBase::~OptionBase() {
}

RewriteOptions::Properties::Properties()
    : initialization_count_(1),
      owns_properties_(true) {
}

RewriteOptions::Properties::~Properties() {
  if (owns_properties_) {
    STLDeleteElements(&property_vector_);
  }
}

RewriteOptions::PropertyBase::~PropertyBase() {
}

bool RewriteOptions::Properties::Initialize(Properties** properties_handle) {
  Properties* properties = *properties_handle;
  if (properties == NULL) {
    *properties_handle = new Properties;
    return true;
  }
  ++(properties->initialization_count_);
  return false;
}

void RewriteOptions::Properties::Merge(Properties* properties) {
  // We merge all subclass properties up into RewriteOptions::all_properties_.
  //   RewriteOptions::properties_.owns_properties_ is true.
  //   RewriteOptions::all_properties_.owns_properties_ is false.
  DCHECK(properties->owns_properties_);
  owns_properties_ = false;
  property_vector_.reserve(size() + properties->size());
  property_vector_.insert(property_vector_.end(),
                          properties->property_vector_.begin(),
                          properties->property_vector_.end());
  std::sort(property_vector_.begin(), property_vector_.end(),
            RewriteOptions::PropertyLessThanByEnum);
  for (int i = 0, n = property_vector_.size(); i < n; ++i) {
    property_vector_[i]->set_index(i);
  }
}

bool RewriteOptions::Properties::Terminate(Properties** properties_handle) {
  Properties* properties = *properties_handle;
  DCHECK_GT(properties->initialization_count_, 0);
  if (--(properties->initialization_count_) == 0) {
    delete properties;
    *properties_handle = NULL;
    return true;
  }
  return false;
}

bool RewriteOptions::Initialize() {
  if (Properties::Initialize(&properties_)) {
    Properties::Initialize(&all_properties_);
    AddProperties();
    InitOptionEnumToNameArray();
    all_properties_->Merge(properties_);
    return true;
  }
  return false;
}

bool RewriteOptions::Terminate() {
  if (Properties::Terminate(&properties_)) {
    Properties::Terminate(&all_properties_);
    return true;
  }
  return false;
}

void RewriteOptions::MergeSubclassProperties(Properties* properties) {
  all_properties_->Merge(properties);
}

bool RewriteOptions::SetFuriousState(int id) {
  furious_id_ = id;
  return SetupFuriousRewriters();
}

void RewriteOptions::SetFuriousStateStr(const StringPiece& experiment_index) {
  if (experiment_index.length() == 1) {
    int index = experiment_index[0] - 'a';
    int n_furious_specs = furious_specs_.size();
    if (0 <= index && index < n_furious_specs) {
      SetFuriousState(furious_specs_[index]->id());
    }
  }
  // Ignore any calls with an invalid index-string.  When experiments are ended
  // a previously valid index string may become invalid.  For example, if a
  // webmaster were running an a/b/c test and now is running an a/b test, a
  // visitor refreshing an old image opened in a separate tab on the 'c' branch
  // of the experiment needs to get some version of that image and not an error.
  // Perhaps more commonly, a webmaster might manually copy a url from pagespeed
  // output to somewhere else on their site at a time an experiment was active,
  // and it would be bad to break that resource link when the experiment ended.
}

GoogleString RewriteOptions::GetFuriousStateStr() const {
  // Don't look at more than 26 furious_specs because we use lowercase a-z.
  // While this is an arbitrary limit, it's much higher than webmasters are
  // likely to run into in practice.  Most of the time people will be running
  // a/b or a/b/c tests, and an a/b/c/d/.../y/z test would be unwieldy and
  // difficult to interpret.  If this does turn out to be needed we can switch
  // to base64 to get 64-way tests, and more than one character experiment index
  // strings would also be possible.
  for (int i = 0, n = furious_specs_.size(); i < n && i < 26; ++i) {
    if (furious_specs_[i]->id() == furious_id_) {
      return GoogleString(1, static_cast<char>('a' + i));
    }
  }
  return "";
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

  // Disable resources that are already being shared across multiple sites and
  // have strong CDN support (ie they are already cheap to fetch and are also
  // very likely to reside in the browser cache from visits to another site).
  // We keep these patterns as specific as possible while avoiding internal
  // wildcards.  Note that all of these urls have query parameters in long-tail
  // requests.
  // TODO(jmaessen): Consider setting up the blacklist by domain name and using
  // regexps only after a match has been found.  Alternatively, since we're
  // setting up a binary choice here, consider using RE2 to make the yes/no
  // decision.
  Disallow("*//ajax.googleapis.com/ajax/libs/*");
  Disallow("*//pagead2.googlesyndication.com/pagead/show_ads.js*");
  Disallow("*//partner.googleadservices.com/gampad/google_service.js*");
  Disallow("*//platform.twitter.com/widgets.js*");
  Disallow("*//s7.addthis.com/js/250/addthis_widget.js*");
  Disallow("*//www.google.com/coop/cse/brand*");
  Disallow("*//www.google-analytics.com/urchin.js*");
  Disallow("*//www.googleadservices.com/pagead/conversion.js*");
  // The following url pattern shows up often, but under too many different
  // unique urls:
  // Disallow("*//stats.wordpress.com/e-*");

  if (Enabled(kComputePanelJson)) {
    RetainComment(StrCat(kPanelCommentPrefix, "*"));
  }
}

bool RewriteOptions::EnableFiltersByCommaSeparatedList(
    const StringPiece& filters, MessageHandler* handler) {
  return AddCommaSeparatedListToFilterSetState(
      filters, &enabled_filters_, handler);
}

bool RewriteOptions::DisableFiltersByCommaSeparatedList(
    const StringPiece& filters, MessageHandler* handler) {
  return AddCommaSeparatedListToFilterSetState(
      filters, &disabled_filters_, handler);
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
  // Doesn't enable kExtendCachePdfs.
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
    const StringPiece& filters, FilterSet* set, MessageHandler* handler) {
  DCHECK(!frozen_);
  size_t prev_set_size = set->size();
  bool ret = AddCommaSeparatedListToFilterSet(filters, set, handler);
  modified_ |= (set->size() != prev_set_size);
  return ret;
}

bool RewriteOptions::AddCommaSeparatedListToFilterSet(
    const StringPiece& filters, FilterSet* set, MessageHandler* handler) {
  StringPieceVector names;
  SplitStringPieceToVector(filters, ",", &names, true);
  bool ret = true;
  for (int i = 0, n = names.size(); i < n; ++i) {
    ret = AddByNameToFilterSet(names[i], set, handler);
  }
  return ret;
}

bool RewriteOptions::AdjustFiltersByCommaSeparatedList(
    const StringPiece& filters, MessageHandler* handler) {
  DCHECK(!frozen_);
  StringPieceVector names;
  SplitStringPieceToVector(filters, ",", &names, true);
  bool ret = true;
  size_t sets_size_sum_before =
      (enabled_filters_.size() + disabled_filters_.size());

  // Default to false unless no filters are specified.
  // "ModPagespeedFilters=" -> disable all filters.
  bool non_incremental = names.empty();
  for (int i = 0, n = names.size(); i < n; ++i) {
    StringPiece& option = names[i];
    if (!option.empty()) {
      if (option[0] == '-') {
        option.remove_prefix(1);
        ret = AddByNameToFilterSet(names[i], &disabled_filters_, handler);
      } else if (option[0] == '+') {
        option.remove_prefix(1);
        ret = AddByNameToFilterSet(names[i], &enabled_filters_, handler);
      } else {
        // No prefix means: reset to pass-through mode prior to
        // applying any of the filters.  +a,-b,+c" will just add
        // a and c and remove b to current default config, but
        // "+a,-b,+c,d" will just run with filters a, c and d.
        ret = AddByNameToFilterSet(names[i], &enabled_filters_, handler);
        non_incremental = true;
      }
    }
  }

  if (non_incremental) {
    SetRewriteLevel(RewriteOptions::kPassThrough);
    DisableAllFiltersNotExplicitlyEnabled();
    modified_ = true;
  } else {
    // TODO(jmarantz): this modified_ computation for query-params doesn't
    // work as we'd like in RewriteQueryTest.NoChangesShouldNotModify.  See
    // a more detailed TODO there.
    size_t sets_size_sum_after =
        (enabled_filters_.size() + disabled_filters_.size());
    modified_ |= (sets_size_sum_before != sets_size_sum_after);
  }
  return ret;
}

bool RewriteOptions::AddByNameToFilterSet(
    const StringPiece& option, FilterSet* set, MessageHandler* handler) {
  bool ret = true;
  Filter filter = LookupFilter(option);
  if (filter == kEndOfFilters) {
    // Handle a compound filter name.  This is much less common, so we don't
    // have any special infrastructure for it; just code.
    // WARNING: Be careful if you add things here; the filters you add
    // here will be invokable by outside people, so they better not crash
    // if that happens!
    if (option == "rewrite_images") {
      set->insert(kConvertGifToPng);
      set->insert(kInlineImages);
      set->insert(kJpegSubsampling);
      set->insert(kRecompressJpeg);
      set->insert(kRecompressPng);
      set->insert(kRecompressWebp);
      set->insert(kResizeImages);
      set->insert(kStripImageMetaData);
      set->insert(kStripImageColorProfile);
    } else if (option == "recompress_images") {
      set->insert(kConvertGifToPng);
      set->insert(kJpegSubsampling);
      set->insert(kRecompressJpeg);
      set->insert(kRecompressPng);
      set->insert(kRecompressWebp);
      set->insert(kStripImageMetaData);
      set->insert(kStripImageColorProfile);
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
    } else {
      if (handler != NULL) {
        handler->Message(kWarning, "Invalid filter name: %s",
                         option.as_string().c_str());
      }
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

bool RewriteOptions::AddCommaSeparatedListToOptionSet(
    const StringPiece& options, OptionSet* set, MessageHandler* handler) {
  StringPieceVector option_vector;
  bool ret = true;
  SplitStringPieceToVector(options, ",", &option_vector, true);
  for (int i = 0, n = option_vector.size(); i < n; ++i) {
    StringPieceVector single_option_and_value;
    SplitStringPieceToVector(option_vector[i], "=", &single_option_and_value,
                             true);
    if (single_option_and_value.size() == 2) {
      set->insert(OptionStringPair(single_option_and_value[0],
                                   single_option_and_value[1]));
    } else {
      ret = false;
    }
  }
  return ret;
}

bool RewriteOptions::SetOptionsFromName(const OptionSet& option_set) {
  bool ret = true;
  for (RewriteOptions::OptionSet::const_iterator iter = option_set.begin();
       iter != option_set.end(); ++iter) {
    GoogleString msg;
    OptionSettingResult result = SetOptionFromName(
        iter->first, iter->second.as_string(), &msg);
    if (result != kOptionOk) {
      ret = false;
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

bool RewriteOptions::IsInBlinkCacheableFamily(const GoogleUrl& gurl) const {
  // If there are no families added and apply_blink_if_no_families is
  // true, then the default behaviour is to allow all urls.
  if (apply_blink_if_no_families() &&
      prioritize_visible_content_families_.empty()) {
    return true;
  }
  StringPiece url_to_match = (use_full_url_in_blink_families() ?
                              gurl.Spec() : gurl.PathAndLeaf());
  return FindPrioritizeVisibleContentFamily(url_to_match) != NULL;
}

int64 RewriteOptions::GetBlinkCacheTimeFor(const GoogleUrl& gurl) const {
  if (override_blink_cache_time_ms_.value() > 0) {
    return override_blink_cache_time_ms_.value();
  }
  StringPiece url_to_match = (use_full_url_in_blink_families() ?
                              gurl.Spec() : gurl.PathAndLeaf());
  const PrioritizeVisibleContentFamily* family =
      FindPrioritizeVisibleContentFamily(url_to_match);
  if (family != NULL) {
    return family->cache_time_ms;
  }
  return kDefaultPrioritizeVisibleContentCacheTimeMs;
}

GoogleString RewriteOptions::GetBlinkNonCacheableElementsFor(
    const GoogleUrl& gurl) const {
  StringPiece url_to_match = (use_full_url_in_blink_families() ?
                              gurl.Spec() : gurl.PathAndLeaf());
  const PrioritizeVisibleContentFamily* family =
      FindPrioritizeVisibleContentFamily(url_to_match);
  if (family != NULL) {
    return family->non_cacheable_elements;
  }
  return "";
}

const RewriteOptions::PrioritizeVisibleContentFamily*
RewriteOptions::FindPrioritizeVisibleContentFamily(
    const StringPiece url) const {
  for (int i = 0, n = prioritize_visible_content_families_.size(); i < n; ++i) {
    const PrioritizeVisibleContentFamily* family =
        prioritize_visible_content_families_[i];
    if (family->url_pattern.Match(url)) {
      return family;
    }
  }
  return NULL;
}

void RewriteOptions::AddBlinkCacheableFamily(
    const StringPiece url_pattern, int64 cache_time_ms,
    const StringPiece non_cacheable_elements) {
  Modify();
  prioritize_visible_content_families_.push_back(
      new PrioritizeVisibleContentFamily(
          url_pattern, cache_time_ms, non_cacheable_elements));
}

bool RewriteOptions::IsAnyFilterRequiringScriptExecutionEnabled() const {
  // TODO(sriharis):  Instead of just checking whether the filters are enabled,
  // tighten the check to also include the UserAgentSupports* conditions so that
  // we can avoid some unnecessary noscript redirects.
  for (int i = 0, n = arraysize(kRequiresScriptExecutionFilterSet); i < n;
       ++i) {
    if (Enabled(kRequiresScriptExecutionFilterSet[i])) {
      return true;
    }
  }
  return false;
}

void RewriteOptions::DisableFiltersRequiringScriptExecution() {
  for (int i = 0, n = arraysize(kRequiresScriptExecutionFilterSet); i < n;
       ++i) {
    DisableFilter(kRequiresScriptExecutionFilterSet[i]);
  }
}

void RewriteOptions::Merge(const RewriteOptions& src) {
  DCHECK(!frozen_);
  DCHECK_EQ(all_options_.size(), src.all_options_.size());
  DCHECK_EQ(initialized_options_, src.initialized_options_);
  DCHECK_EQ(initialized_options_, all_options_.size());
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
    InsertFuriousSpecInVector(spec);
  }

  for (int i = 0, n = src.custom_fetch_headers_.size(); i < n; ++i) {
    NameValue* nv = src.custom_fetch_headers_[i];
    AddCustomFetchHeader(nv->name, nv->value);
  }

  furious_id_ = src.furious_id_;
  for (int i = 0, n = src.num_url_valued_attributes(); i < n; ++i) {
    StringPiece element;
    StringPiece attribute;
    semantic_type::Category category;
    src.UrlValuedAttribute(i, &element, &attribute, &category);
    AddUrlValuedAttribute(element, attribute, category);
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
  override_caching_wildcard_.AppendFrom(src.override_caching_wildcard_);

  // Merge url_cache_invalidation_entries_ so that increasing order of timestamp
  // is preserved (assuming this.url_cache_invalidation_entries_ and
  // src.url_cache_invalidation_entries_ are both ordered).
  int original_size = url_cache_invalidation_entries_.size();
  // Append copies of src's url cache invalidation entries to this.
  for (int i = 0, n = src.url_cache_invalidation_entries_.size(); i < n; ++i) {
    url_cache_invalidation_entries_.push_back(
        src.url_cache_invalidation_entries_[i]->Clone());
  }
  // Now url_cache_invalidation_entries_ consists of two ordered ranges: [begin,
  // begin+original_size) and [begin+original_size, end).  Hence we can use
  // inplace_merge.
  std::inplace_merge(url_cache_invalidation_entries_.begin(),
                     url_cache_invalidation_entries_.begin() + original_size,
                     url_cache_invalidation_entries_.end(),
                     RewriteOptions::CompareUrlCacheInvalidationEntry);

  // If src's prioritize_visible_content_families_ is non-empty we simply
  // replace this' prioritize_visible_content_families_ with src's.  Naturally,
  // this means that families in this are lost.
  // TODO(sriharis):  Revisit the Merge logic to be used for
  // prioritize_visible_content_families_.
  if (!src.prioritize_visible_content_families_.empty()) {
    if (!prioritize_visible_content_families_.empty()) {
      STLDeleteElements(&prioritize_visible_content_families_);
      prioritize_visible_content_families_.clear();
    }
    for (int i = 0, n = src.prioritize_visible_content_families_.size(); i < n;
         ++i) {
      prioritize_visible_content_families_.push_back(
          src.prioritize_visible_content_families_[i]->Clone());
    }
  }
}

RewriteOptions::MutexedOptionInt64MergeWithMax::MutexedOptionInt64MergeWithMax()
    : mutex_(new NullRWLock) {
}

RewriteOptions::MutexedOptionInt64MergeWithMax::
~MutexedOptionInt64MergeWithMax() {
}

void RewriteOptions::MutexedOptionInt64MergeWithMax::Merge(
    const OptionBase* src_base) {
  // This option must be a MutexedOptionInt64 everywhere, so this cast is safe.
  const MutexedOptionInt64MergeWithMax* src =
      static_cast<const MutexedOptionInt64MergeWithMax*>(src_base);
  bool src_was_set;
  int64 src_value;
  {
    ThreadSystem::ScopedReader read_lock(src->mutex());
    src_was_set = src->was_set();
    src_value = src->value();
  }
  // We don't grab a writer lock because at merge time this is
  // only accessible to the current thread.
  if (src_was_set && (!was_set() || src_value > value())) {
    set(src_value);
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

GoogleString RewriteOptions::OptionSignature(const BeaconUrl& beacon_url,
                                             const Hasher* hasher) {
  return hasher->Hash(ToString(beacon_url));
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
    if (option->is_used_for_signature_computation() && option->was_set()) {
      StrAppend(&signature_, option->id(), ":", option->Signature(hasher), "_");
    }
  }
  StrAppend(&signature_, domain_lawyer_.Signature(), "_");
  StrAppend(&signature_, "AR:", allow_resources_.Signature(), "_");
  StrAppend(&signature_, "RC:", retain_comments_.Signature(), "_");
  StrAppend(&signature_, "UCI:");
  for (int i = 0, n = url_cache_invalidation_entries_.size(); i < n; ++i) {
    const UrlCacheInvalidationEntry& entry =
        *url_cache_invalidation_entries_[i];
    if (!entry.is_strict) {
      StrAppend(&signature_, entry.ComputeSignature(), "|");
    }
  }
  StrAppend(&signature_, "OC:", override_caching_wildcard_.Signature(), "_");
  StrAppend(&signature_, "PVC:");
  for (int i = 0, n = prioritize_visible_content_families_.size(); i < n; ++i) {
    StrAppend(&signature_,
              prioritize_visible_content_families_[i]->ComputeSignature(), "|");
  }
  frozen_ = true;

  // TODO(jmarantz): Incorporate signature from file_load_policy.  However, the
  // changes made here make our system strictly more correct than it was before,
  // using an ad-hoc signature in css_filter.cc.
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

GoogleString RewriteOptions::ToString(const BeaconUrl& beacon_url) {
  GoogleString result = beacon_url.http;
  if (beacon_url.http != beacon_url.https) {
    StrAppend(&result, " ", beacon_url.https);
  }
  return result;
}

GoogleString RewriteOptions::OptionsToString() const {
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
  output += "\nDomain Lawyer\n";
  StrAppend(&output, domain_lawyer_.ToString("  "));
  // TODO(mmohabey): Incorporate ToString() from the file_load_policy,
  // allow_resources, and retain_comments.
  if (!url_cache_invalidation_entries_.empty()) {
    StrAppend(&output, "\nURL cache invalidation entries\n");
    for (int i = 0, n = url_cache_invalidation_entries_.size(); i < n; ++i) {
      StrAppend(&output, "  ", url_cache_invalidation_entries_[i]->ToString(),
                "\n");
    }
  }
  if (!prioritize_visible_content_families_.empty()) {
    StrAppend(&output, "\nPrioritize visible content cacheable families\n");
    for (int i = 0, n = prioritize_visible_content_families_.size(); i < n;
         ++i) {
      StrAppend(&output, "  ",
                prioritize_visible_content_families_[i]->ToString(), "\n");
    }
  }
  GoogleString override_caching_wildcard_string(
      override_caching_wildcard_.Signature());
  if (!override_caching_wildcard_string.empty()) {
    StrAppend(&output, "\nOverride caching wildcards\n",
              override_caching_wildcard_string);
  }
  return output;
}

GoogleString RewriteOptions::ToExperimentString() const {
  // Only add the experiment id if we're running this experiment.
  if (GetFuriousSpec(furious_id_) != NULL) {
    return StringPrintf("Experiment: %d", furious_id_);
  }
  return GoogleString();
}

GoogleString RewriteOptions::ToExperimentDebugString() const {
  GoogleString output = ToExperimentString();
  if (!output.empty()) {
    output += "; ";
  }
  if (!running_furious()) {
    output += "off; ";
  } else if (furious_id_ == furious::kFuriousNotSet) {
    output += "not set; ";
  } else if (furious_id_ == furious::kFuriousNoExperiment) {
    output += "no experiment; ";
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

void RewriteOptions::AddCustomFetchHeader(const StringPiece& name,
                                          const StringPiece& value) {
  custom_fetch_headers_.push_back(new NameValue(name, value));
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
      id == furious::kFuriousNoExperiment) {
    return false;
  }
  return (GetFuriousSpec(id) == NULL);
}

bool RewriteOptions::AddFuriousSpec(const StringPiece& spec,
                                    MessageHandler* handler) {
  FuriousSpec* f_spec = new FuriousSpec(spec, this, handler);
  return InsertFuriousSpecInVector(f_spec);
}

bool RewriteOptions::InsertFuriousSpecInVector(FuriousSpec* spec) {
  // See RewriteOptions::GetFuriousStateStr for why we can't have more than 26.
  if (!AvailableFuriousId(spec->id()) || spec->percent() <= 0 ||
      furious_percent_ + spec->percent() > 100 ||
      furious_specs_.size() + 1 > 26) {
    delete spec;
    return false;
  }
  furious_specs_.push_back(spec);
  furious_percent_ += spec->percent();
  return true;
}

// Always enable add_head, insert_ga, add_instrumentation,
// and HtmlWriter.  This is considered a "no-filter" base for
// furious experiments.
bool RewriteOptions::SetupFuriousRewriters() {
  // Don't change anything if we're not in an experiment or have some
  // unset id.
  if (furious_id_ == furious::kFuriousNotSet ||
      furious_id_ == furious::kFuriousNoExperiment) {
    return true;
  }
  // Control: just make sure that the necessary stuff is on.
  // Do NOT try to set up things to look like the FuriousSpec
  // for this id: it doesn't match the rewrite options.
  FuriousSpec* spec = GetFuriousSpec(furious_id_);
  if (spec == NULL) {
    return false;
  }

  if (!spec->ga_id().empty()) {
    set_ga_id(spec->ga_id());
  }

  set_furious_ga_slot(spec->slot());

  if (spec->use_default()) {
    // We need these for the experiment to work properly.
    SetRequiredFuriousFilters();
    return true;
  }

  ClearFilters();
  SetRewriteLevel(spec->rewrite_level());
  EnableFilters(spec->enabled_filters());
  DisableFilters(spec->disabled_filters());
  // We need these for the experiment to work properly.
  SetRequiredFuriousFilters();
  set_css_inline_max_bytes(spec->css_inline_max_bytes());
  set_js_inline_max_bytes(spec->js_inline_max_bytes());
  set_image_inline_max_bytes(spec->image_inline_max_bytes());
  SetOptionsFromName(spec->filter_options());
  return true;
}

void RewriteOptions::SetRequiredFuriousFilters() {
  ForceEnableFilter(RewriteOptions::kAddHead);
  ForceEnableFilter(RewriteOptions::kAddInstrumentation);
  ForceEnableFilter(RewriteOptions::kInsertGA);
  ForceEnableFilter(RewriteOptions::kHtmlWriterFilter);
}

RewriteOptions::FuriousSpec::FuriousSpec(const StringPiece& spec,
                                         RewriteOptions* options,
                                         MessageHandler* handler)
    : id_(furious::kFuriousNotSet),
      ga_id_(options->ga_id()),
      ga_variable_slot_(options->furious_ga_slot()),
      percent_(0),
      rewrite_level_(kPassThrough),
      css_inline_max_bytes_(kDefaultCssInlineMaxBytes),
      js_inline_max_bytes_(kDefaultJsInlineMaxBytes),
      image_inline_max_bytes_(kDefaultImageInlineMaxBytes),
      use_default_(false) {
  Initialize(spec, handler);
}

RewriteOptions::FuriousSpec::FuriousSpec(int id)
    : id_(id),
      ga_id_(""),
      ga_variable_slot_(kDefaultFuriousSlot),
      percent_(0),
      rewrite_level_(kPassThrough),
      css_inline_max_bytes_(kDefaultCssInlineMaxBytes),
      js_inline_max_bytes_(kDefaultJsInlineMaxBytes),
      image_inline_max_bytes_(kDefaultImageInlineMaxBytes),
      use_default_(false) {
}

RewriteOptions::FuriousSpec::~FuriousSpec() { }

void RewriteOptions::FuriousSpec::Merge(const FuriousSpec& spec) {
  for (FilterSet::const_iterator iter = spec.enabled_filters_.begin();
       iter != spec.enabled_filters_.end(); ++iter) {
    enabled_filters_.insert(*iter);
  }
  for (FilterSet::const_iterator iter = spec.disabled_filters_.begin();
       iter != spec.disabled_filters_.end(); ++iter) {
    disabled_filters_.insert(*iter);
  }
  for (OptionSet::const_iterator iter = spec.filter_options_.begin();
       iter != spec.filter_options_.end(); ++iter) {
    filter_options_.insert(*iter);
  }
  ga_id_ = spec.ga_id_;
  ga_variable_slot_ = spec.ga_variable_slot_;
  percent_ = spec.percent_;
  rewrite_level_ = spec.rewrite_level_;
  css_inline_max_bytes_ = spec.css_inline_max_bytes_;
  js_inline_max_bytes_ = spec.js_inline_max_bytes_;
  image_inline_max_bytes_ = spec.image_inline_max_bytes_;
  use_default_ = spec.use_default_;
}

RewriteOptions::FuriousSpec* RewriteOptions::FuriousSpec::Clone() {
  FuriousSpec* ret = new FuriousSpec(id_);
  ret->Merge(*this);
  return ret;
}

// Options are written in the form:
// ModPagespeedExperimentSpec 'id= 2; percent= 20; RewriteLevel= CoreFilters;
// enable= resize_images; disable = is; inline_css = 25556; ga=UA-233842-1'
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
    } else if (StringCaseEqual(piece, "default")) {
      // "Default" means use whatever RewriteOptions are.
      use_default_ = true;
    } else if (StringCaseStartsWith(piece, "percent")) {
      StringPiece percent = PieceAfterEquals(piece);
      StringToInt(percent.as_string(), &percent_);
    } else if (StringCaseStartsWith(piece, "ga")) {
      StringPiece ga = PieceAfterEquals(piece);
      if (ga.length() > 0) {
        ga_id_ = GoogleString(ga.data(), ga.length());
      }
    } else if (StringCaseStartsWith(piece, "slot")) {
      StringPiece slot = PieceAfterEquals(piece);
      int stored_id = ga_variable_slot_;
      StringToInt(slot.as_string(), &ga_variable_slot_);
      // Valid custom variable slots are 1-5 inclusive.
      if (ga_variable_slot_ < 1 || ga_variable_slot_ > 5) {
        LOG(INFO) << "Invalid custom variable slot.";
        ga_variable_slot_ = stored_id;
      }
    } else if (StringCaseStartsWith(piece, "level")) {
      StringPiece level = PieceAfterEquals(piece);
      if (level.length() > 0) {
        ParseRewriteLevel(level, &rewrite_level_);
      }
    } else if (StringCaseStartsWith(piece, "enable")) {
      StringPiece enabled = PieceAfterEquals(piece);
      if (enabled.length() > 0) {
        AddCommaSeparatedListToFilterSet(enabled, &enabled_filters_, handler);
      }
    } else if (StringCaseStartsWith(piece, "disable")) {
      StringPiece disabled = PieceAfterEquals(piece);
      if (disabled.length() > 0) {
        AddCommaSeparatedListToFilterSet(disabled, &disabled_filters_, handler);
      }
    } else if (StringCaseStartsWith(piece, "options")) {
      StringPiece options = PieceAfterEquals(piece);
      if (options.length() > 0) {
        AddCommaSeparatedListToOptionSet(options, &filter_options_, handler);
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

void RewriteOptions::AddUrlValuedAttribute(
    const StringPiece& element, const StringPiece& attribute,
    semantic_type::Category category) {
  if (url_valued_attributes_ == NULL) {
    url_valued_attributes_.reset(new std::vector<ElementAttributeCategory>());
  }
  ElementAttributeCategory eac;
  element.CopyToString(&eac.element);
  attribute.CopyToString(&eac.attribute);
  eac.category = category;
  url_valued_attributes_->push_back(eac);
}

void RewriteOptions::UrlValuedAttribute(
    int index, StringPiece* element, StringPiece* attribute,
    semantic_type::Category* category) const {
  const ElementAttributeCategory& eac = (*url_valued_attributes_)[index];
  *element = StringPiece(eac.element);
  *attribute = StringPiece(eac.attribute);
  *category = eac.category;
}

bool RewriteOptions::IsUrlCacheValid(StringPiece url, int64 time_ms) const {
  int i = 0;
  int n = url_cache_invalidation_entries_.size();
  while (i < n && time_ms > url_cache_invalidation_entries_[i]->timestamp_ms) {
    ++i;
  }
  // Now all entries from 0 to i-1 have timestamp less than time_ms and hence
  // cannot invalidate a url cached at time_ms.
  // TODO(sriharis):  Should we use binary search instead of the above loop?
  // Probably does not make sense as long as the following while loop is there.

  // Once FastWildcardGroup is in, we should check if it makes sense to make a
  // FastWildcardGroup of Wildcards from position i to n-1, and Match against
  // it.
  while (i < n) {
    if (url_cache_invalidation_entries_[i]->url_pattern.Match(url)) {
      return false;
    }
    ++i;
  }
  return true;
}

void RewriteOptions::AddUrlCacheInvalidationEntry(
    StringPiece url_pattern, int64 timestamp_ms, bool is_strict) {
  if (!url_cache_invalidation_entries_.empty()) {
    // Check that this Add preserves the invariant that
    // url_cache_invalidation_entries_ is sorted on timestamp_ms.
    if (url_cache_invalidation_entries_.back()->timestamp_ms > timestamp_ms) {
      LOG(DFATAL) << "Timestamp " << timestamp_ms << " is less than the last "
                  << "timestamp already added: "
                  << url_cache_invalidation_entries_.back()->timestamp_ms;
      return;
    }
  }
  url_cache_invalidation_entries_.push_back(
      new UrlCacheInvalidationEntry(url_pattern, timestamp_ms, is_strict));
}

bool RewriteOptions::UpdateCacheInvalidationTimestampMs(int64 timestamp_ms,
                                                        const Hasher* hasher) {
  bool ret = false;
  ScopedMutex lock(cache_invalidation_timestamp_.mutex());
  if (cache_invalidation_timestamp_.value() < timestamp_ms) {
    bool recompute_signature = frozen_;
    frozen_ = false;
    cache_invalidation_timestamp_.checked_set(timestamp_ms);
    Modify();
    if (recompute_signature) {
      signature_.clear();
      ComputeSignature(hasher);
    }
    ret = true;
  }
  return ret;
}

bool RewriteOptions::IsUrlCacheInvalidationEntriesSorted() const {
  for (int i = 0, n = url_cache_invalidation_entries_.size(); i < n - 1; ++i) {
    if (url_cache_invalidation_entries_[i]->timestamp_ms >
        url_cache_invalidation_entries_[i + 1]->timestamp_ms) {
      return false;
    }
  }
  return true;
}

}  // namespace net_instaweb
