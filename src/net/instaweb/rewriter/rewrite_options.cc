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
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/wildcard_group.h"

namespace {

// This version index serves as global signature key.  Much of the
// data emitted in signatures is based on the current enum layout,
// which can change as we add new filters, etc.  So every time there
// is a binary-incompatible change to the enumer level, we bump this
// version.
//
// Updating this value will have the indirect effect of flushing the metadata
// cache.
//
// This version number should be incremented if any default-values are changed,
// either in the add_option() call or via options->set_default.
const int kOptionsVersion = 7;

}  // namespace

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

const int64 RewriteOptions::kDefaultMaxHtmlCacheTimeMs = 0;
const int64 RewriteOptions::kDefaultMinResourceCacheTimeToRewriteMs = 0;

const int64 RewriteOptions::kDefaultCacheInvalidationTimestamp = -1;
const int64 RewriteOptions::kDefaultIdleFlushTimeMs = 10;

// Limit on concurrent ongoing image rewrites.
// TODO(jmaessen): Determine a sane default for this value.
const int RewriteOptions::kDefaultImageMaxRewritesAtOnce = 8;

// IE limits URL size overall to about 2k characters.  See
// http://support.microsoft.com/kb/208427/EN-US
const int RewriteOptions::kMaxUrlSize = 2083;

// Jpeg quality that needs to be used while recompressing. If set to -1, we
// use source image quality parameters, and is lossless.
const int RewriteOptions::kDefaultImageJpegRecompressQuality = -1;

// See http://code.google.com/p/modpagespeed/issues/detail?id=9.  By
// default, Apache evidently limits each URL path segment (between /)
// to about 256 characters.  This is not a fundamental URL limitation
// but is Apache specific.  Ben Noordhuis has provided a workaround
// of hooking map_to_storage to skip the directory-mapping phase in
// Apache.  See http://code.google.com/p/modpagespeed/issues/detail?id=176
const int RewriteOptions::kDefaultMaxUrlSegmentSize = 1024;

const GoogleString RewriteOptions::kDefaultBeaconUrl =
    "/mod_pagespeed_beacon?ets=";

const char RewriteOptions::kClassName[] = "RewriteOptions";

namespace {

const RewriteOptions::Filter kCoreFilterSet[] = {
  RewriteOptions::kAddHead,
  RewriteOptions::kCombineCss,
  RewriteOptions::kConvertMetaTags,
  RewriteOptions::kExtendCache,
  RewriteOptions::kHtmlWriterFilter,
  RewriteOptions::kInlineCss,
  RewriteOptions::kInlineImages,
  RewriteOptions::kInlineImportToLink,
  RewriteOptions::kInlineJavascript,
  RewriteOptions::kInsertImageDimensions,
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
  RewriteOptions::kMakeGoogleAnalyticsAsync,
  RewriteOptions::kRewriteDomains,
};

// Note: These filters should not be included even if the level is "All".
const RewriteOptions::Filter kDangerousFilterSet[] = {
  RewriteOptions::kDivStructure,
  RewriteOptions::kInlineImagesInCss,  // TODO(jmaessen): Make it safe
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

const char * RewriteOptions::FilterName(
    const RewriteOptions::Filter filter) const {
  switch (filter) {
    case kAddHead:                         return "Add Head";
    case kAddInstrumentation:              return "Add Instrumentation";
    case kCollapseWhitespace:              return "Collapse Whitespace";
    case kCombineCss:                      return "Combine Css";
    case kCombineHeads:                    return "Combine Heads";
    case kCombineJavascript:               return "Combine Javascript";
    case kConvertJpegToWebp:               return "Convert Jpeg To Webp";
    case kConvertMetaTags:                 return "Convert Meta Tags";
    case kDivStructure:                    return "Div Structure";
    case kElideAttributes:                 return "Elide Attributes";
    case kExtendCache:                     return "Extend Cache";
    case kHtmlWriterFilter:                return "Flushes html";
    case kInlineCss:                       return "Inline Css";
    case kInlineImages:                    return "Inline Images";
    case kInlineImagesInCss:               return "Inline Images in Css";
    case kInlineImportToLink:              return "Inline @import to Link";
    case kInlineJavascript:                return "Inline Javascript";
    case kInsertImageDimensions:           return "Insert Image Dimensions";
    case kLeftTrimUrls:                    return "Left Trim Urls";
    case kMakeGoogleAnalyticsAsync:        return "Make Google Analytics Async";
    case kMoveCssToHead:                   return "Move Css To Head";
    case kOutlineCss:                      return "Outline Css";
    case kOutlineJavascript:               return "Outline Javascript";
    case kRecompressImages:                return "Recompress Images";
    case kRemoveComments:                  return "Remove Comments";
    case kRemoveQuotes:                    return "Remove Quotes";
    case kResizeImages:                    return "Resize Images";
    case kRewriteCss:                      return "Rewrite Css";
    case kRewriteDomains:                  return "Rewrite Domains";
    case kRewriteJavascript:               return "Rewrite Javascript";
    case kRewriteStyleAttributes:          return "Rewrite Style Attributes";
    case kRewriteStyleAttributesWithUrl:
      return "Rewrite Style Attributes With Url";
    case kSpriteImages:                    return "Sprite Images";
    case kStripScripts:                    return "Strip Scripts";
    case kEndOfFilters:                    return "End of Filters";
  }
  return "Unknown Filter";
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
      frozen_(false) {
  // Sanity-checks -- will be active only when compiled for debug.
#ifndef NDEBUG
  CheckFilterSetOrdering(kCoreFilterSet, arraysize(kCoreFilterSet));
  CheckFilterSetOrdering(kTestFilterSet, arraysize(kTestFilterSet));
  CheckFilterSetOrdering(kDangerousFilterSet, arraysize(kDangerousFilterSet));
#endif

  // TODO(jmarantz): consider adding these on demand so that the cost of
  // initializing an empty RewriteOptions object is closer to zero.
  add_option(kPassThrough, &level_);
  add_option(kDefaultCssInlineMaxBytes, &css_inline_max_bytes_);
  add_option(kDefaultImageInlineMaxBytes, &image_inline_max_bytes_);
  add_option(kDefaultCssImageInlineMaxBytes, &css_image_inline_max_bytes_);
  add_option(kDefaultJsInlineMaxBytes, &js_inline_max_bytes_);
  add_option(kDefaultCssOutlineMinBytes, &css_outline_min_bytes_);
  add_option(kDefaultJsOutlineMinBytes, &js_outline_min_bytes_);
  add_option(kDefaultMaxHtmlCacheTimeMs, &max_html_cache_time_ms_);
  add_option(kDefaultMinResourceCacheTimeToRewriteMs,
             &min_resource_cache_time_to_rewrite_ms_);
  add_option(kDefaultCacheInvalidationTimestamp,
             &cache_invalidation_timestamp_);
  add_option(kDefaultIdleFlushTimeMs, &idle_flush_time_ms_);
  add_option(kDefaultImageMaxRewritesAtOnce, &image_max_rewrites_at_once_);
  add_option(kDefaultMaxUrlSegmentSize, &max_url_segment_size_);
  add_option(kMaxUrlSize, &max_url_size_);
  add_option(true, &enabled_);
  add_option(false, &botdetect_enabled_);
  add_option(true, &combine_across_paths_);
  add_option(false, &log_rewrite_timing_);
  add_option(false, &lowercase_html_names_);
  add_option(false, &always_rewrite_css_);
  add_option(false, &respect_vary_);
  add_option(false, &flush_html_);
  add_option(kDefaultBeaconUrl, &beacon_url_);
  add_option(kDefaultImageJpegRecompressQuality,
             &image_jpeg_recompress_quality_);

  // Enable HtmlWriterFilter by default.
  EnableFilter(kHtmlWriterFilter);
}

RewriteOptions::~RewriteOptions() {
}

RewriteOptions::OptionBase::~OptionBase() {
}


void RewriteOptions::DisallowTroublesomeResources() {
  // http://code.google.com/p/modpagespeed/issues/detail?id=38
  Disallow("*js_tinyMCE*");  // js_tinyMCE.js
  // Official timeMCE URLs: tiny_mce.js, tiny_mce_src.js, tiny_mce_gzip.php, ...
  Disallow("*tiny_mce*");
  // I've also seen tinymce.js
  Disallow("*tinymce*");

  // http://code.google.com/p/modpagespeed/issues/detail?id=207
  // jquery-ui-1.8.2.custom.min.js, jquery-1.4.4.min.js, jquery.fancybox-...
  //
  // TODO(sligocki): Is jquery actually a problem? Perhaps specific
  // jquery libraries (like tiny MCE). Investigate before disabling.
  // Disallow("*jquery*");

  // http://code.google.com/p/modpagespeed/issues/detail?id=186
  // ckeditor.js, ckeditor_basic.js, ckeditor_basic_source.js, ...
  // Appears to be fixed upstream. Leaving here for reference.
  // Disallow("*ckeditor*");

  // http://code.google.com/p/modpagespeed/issues/detail?id=216
  // Appears to be an issue with old version of jsminify.
  // Disallow("*swfobject*");  // swfobject.js

  // TODO(sligocki): Add disallow for the JS broken in:
  // http://code.google.com/p/modpagespeed/issues/detail?id=142
  // Not clear which JS file is broken and proxying is not working correctly.
}

bool RewriteOptions::EnableFiltersByCommaSeparatedList(
    const StringPiece& filters, MessageHandler* handler) {
  return AddCommaSeparatedListToFilterSet(
      filters, handler, &enabled_filters_);
}

bool RewriteOptions::DisableFiltersByCommaSeparatedList(
    const StringPiece& filters, MessageHandler* handler) {
  return AddCommaSeparatedListToFilterSet(
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

bool RewriteOptions::AddCommaSeparatedListToFilterSet(
    const StringPiece& filters, MessageHandler* handler, FilterSet* set) {
  DCHECK(!frozen_);
  StringPieceVector names;
  SplitStringPieceToVector(filters, ",", &names, true);
  bool ret = true;
  size_t prev_set_size = set->size();
  for (int i = 0, n = names.size(); i < n; ++i) {
    const StringPiece& option = names[i];
    Filter filter = Lookup(option);
    if (filter == kEndOfFilters) {
      // Handle a compound filter name.  This is much less common, so we don't
      // have any special infrastructure for it; just code.
      if (option == "rewrite_images") {
        set->insert(kInlineImages);
        set->insert(kInsertImageDimensions);
        set->insert(kRecompressImages);
        set->insert(kResizeImages);
      } else {
        handler->Message(kWarning, "Invalid filter name: %s",
                         option.as_string().c_str());
        ret = false;
      }
    } else {
      set->insert(filter);
    }
  }
  modified_ |= (set->size() != prev_set_size);
  return ret;
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
  if (Enabled(kInlineImagesInCss)) {
    return css_image_inline_max_bytes_.value();
  } else {
    return 0;
  }
}

int64 RewriteOptions::MaxImageInlineMaxBytes() const {
  return std::max(ImageInlineMaxBytes(),
                  CssImageInlineMaxBytes());
}

void RewriteOptions::Merge(const RewriteOptions& first,
                           const RewriteOptions& second) {
  DCHECK(!frozen_);
  modified_ = first.modified_ || second.modified_;
  enabled_filters_ = first.enabled_filters_;
  disabled_filters_ = first.disabled_filters_;
  for (FilterSet::const_iterator p = second.enabled_filters_.begin(),
           e = second.enabled_filters_.end(); p != e; ++p) {
    Filter filter = *p;
    // Enabling in 'second' trumps Disabling in first.
    disabled_filters_.erase(filter);
    enabled_filters_.insert(filter);
  }

  for (FilterSet::const_iterator p = second.disabled_filters_.begin(),
           e = second.disabled_filters_.end(); p != e; ++p) {
    Filter filter = *p;
    // Disabling in 'second' trumps enabling in anything.
    disabled_filters_.insert(filter);
    enabled_filters_.erase(filter);
  }

  // Note that from the perspective of this class, we can be merging
  // RewriteOptions subclasses & superclasses, so don't read anything
  // that doesn't exist.  However this is almost certainly the wrong
  // thing to do -- we should ensure that within a system all the
  // RewriteOptions that are instantiated are the same sublcass, so
  // DCHECK that they have the same number of options.
  size_t options_to_read = std::max(first.all_options_.size(),
                                    second.all_options_.size());
  DCHECK_EQ(first.all_options_.size(), second.all_options_.size());
  DCHECK_EQ(options_to_read, all_options_.size());
  size_t options_to_merge = std::min(options_to_read, all_options_.size());
  for (size_t i = 0; i < options_to_merge; ++i) {
    // Be careful to merge only options that exist in all three.
    // TODO(jmarantz): this logic is not 100% sound if there are two
    // different subclasses in play.  We should resolve this at a higher
    // level and assert that the option subclasses are the same.
    if (i >= first.all_options_.size()) {
      all_options_[i]->Merge(second.all_options_[i], second.all_options_[i]);
    } else if (i >= second.all_options_.size()) {
      all_options_[i]->Merge(first.all_options_[i], first.all_options_[i]);
    } else {
      all_options_[i]->Merge(first.all_options_[i], second.all_options_[i]);
    }
  }

  // Pick the larger of the two cache invalidation timestamps. Following
  // calculation assumes the default value of cache invalidation timestamp
  // to be -1.
  //
  // Note: this gets merged by order in the above loop, and then this
  // block of code overrides the merged value.
  //
  // TODO(jmarantz): fold this logic into a new OptionBase subclass whose
  // Merge method does the right thing.
  if (first.cache_invalidation_timestamp_.value() !=
      RewriteOptions::kDefaultCacheInvalidationTimestamp ||
      second.cache_invalidation_timestamp_.value() !=
          RewriteOptions::kDefaultCacheInvalidationTimestamp) {
    cache_invalidation_timestamp_.set(
        std::max(first.cache_invalidation_timestamp_.value(),
                 second.cache_invalidation_timestamp_.value()));
  }

  // Note that the domain-lawyer merge works one-at-a-time, which is easier
  // to unit test.  So we have to call it twice.
  domain_lawyer_.Merge(first.domain_lawyer_);
  domain_lawyer_.Merge(second.domain_lawyer_);

  file_load_policy_.Merge(first.file_load_policy_);
  file_load_policy_.Merge(second.file_load_policy_);

  allow_resources_.CopyFrom(first.allow_resources_);
  allow_resources_.AppendFrom(second.allow_resources_);

  retain_comments_.CopyFrom(first.retain_comments_);
  retain_comments_.AppendFrom(second.retain_comments_);
}

RewriteOptions* RewriteOptions::Clone() const {
  RewriteOptions* options = new RewriteOptions;
  options->CopyFrom(*this);
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
  signature_ = IntegerToString(kOptionsVersion);
  for (int i = kFirstFilter; i != kEndOfFilters; ++i) {
    if (Enabled(static_cast<Filter>(i))) {
      StrAppend(&signature_, "_", IntegerToString(static_cast<int>(i)));
    }
  }
  signature_ += "O";
  for (int i = 0, n = all_options_.size(); i < n; ++i) {
    // Keep the signature relatively short by only including options
    // with values overridden from the default.
    OptionBase* option = all_options_[i];
    if (option->was_set()) {
      StrAppend(&signature_, IntegerToString(i), ":",
                option->Signature(hasher), "_");
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
    if (Enabled(static_cast<Filter>(i))) {
      StrAppend(&output, IntegerToString(i), "\t",
                FilterName(static_cast<Filter>(i)), "\n");
    }
  }
  output += "\nOptions\n";
  for (int i = 0, n = all_options_.size(); i < n; ++i) {
    // Only including options with values overridden from the default.
    OptionBase* option = all_options_[i];
    if (option->was_set()) {
      StrAppend(&output, IntegerToString(i), "\t", option->ToString(), "\n");
    }
  }
  // TODO(mmohabey): Incorporate ToString() from the domain_lawyer,
  // file_load_policy, allow_resources, and retain_comments.
  return output;
}

void RewriteOptions::Modify() {
  DCHECK(!frozen_);
  modified_ = true;
}

const char* RewriteOptions::class_name() const {
  return RewriteOptions::kClassName;
}

}  // namespace net_instaweb
