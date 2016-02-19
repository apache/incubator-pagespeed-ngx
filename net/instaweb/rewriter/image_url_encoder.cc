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
// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/rewriter/public/image_url_encoder.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/util/url_escaper.h"

namespace net_instaweb {

namespace {

const char kCodeSeparator = 'x';
const char kCodeWebpLossy = 'w';               // for decoding legacy URLs.
const char kCodeWebpLossyLosslessAlpha = 'v';  // for decoding legacy URLs.
const char kCodeMobileUserAgent = 'm';         // for decoding legacy URLs.
const char kMissingDimension = 'N';

// Constants for UserAgent cache key entries.
const char kWebpLossyUserAgentKey[] = "w";
const char kWebpLossyLossLessAlphaUserAgentKey[] = "v";
const char kWebpAnimatedUserAgentKey[] = "a";
// This used to not have a separate key, but we mixed up animated and it
// at one point, so this is now here to force a flush.
const char kWebpNoneUserAgentKey[] = ".";
const char kMobileUserAgentKey[] = "m";
const char kSaveDataKey[] = "d";
const char kSmallScreenKey[] = "ss";

bool IsValidCode(char code) {
  return ((code == kCodeSeparator) ||
          (code == kCodeWebpLossy) ||
          (code == kCodeWebpLossyLosslessAlpha) ||
          (code == kCodeMobileUserAgent));
}

// Decodes a single dimension (either N or an integer), removing it from *in and
// ensuring at least one character remains.  Returns true on success.  When N is
// seen, *has_dimension is set to true.  If decoding fails, *ok is set to false.
//
// Ensures that *in contains at least one character on exit.
uint32 DecodeDimension(StringPiece* in, bool* ok, bool* has_dimension) {
  uint32 result = 0;
  if (in->size() < 2) {
    *ok = false;
    *has_dimension = false;
  } else if ((*in)[0] == kMissingDimension) {
    // Dimension is absent.
    in->remove_prefix(1);
    *ok = true;
    *has_dimension = false;
  } else {
    *ok = false;
    *has_dimension = true;
    while (in->size() >= 2 && AccumulateDecimalValue((*in)[0], &result)) {
      in->remove_prefix(1);
      *ok = true;
    }
  }
  return result;
}

}  // namespace

ImageUrlEncoder::~ImageUrlEncoder() { }

void ImageUrlEncoder::Encode(const StringVector& urls,
                             const ResourceContext* data,
                             GoogleString* rewritten_url) const {
  DCHECK(data != NULL) << "null data passed to ImageUrlEncoder::Encode";
  DCHECK_EQ(1U, urls.size());
  if (data != NULL) {
    if (HasDimension(*data)) {
      const ImageDim& dims = data->desired_image_dims();
      if (dims.has_width()) {
        rewritten_url->append(IntegerToString(dims.width()));
      } else {
        rewritten_url->push_back(kMissingDimension);
      }
      if (dims.has_height()) {
        StrAppend(rewritten_url,
                  StringPiece(&kCodeSeparator, 1),
                  IntegerToString(dims.height()));
      } else {
        StrAppend(rewritten_url,
                  StringPiece(&kCodeSeparator, 1),
                  StringPiece(&kMissingDimension, 1));
      }
    }
    rewritten_url->push_back(kCodeSeparator);
  }

  UrlEscaper::EncodeToUrlSegment(urls[0], rewritten_url);
}

namespace {

// Stateless helper function for ImageUrlEncoder::Decode.
// Removes read dimensions from remaining, sets dims and returns true if
// dimensions are correctly parsed, returns false and leaves dims untouched on
// parse failure.
bool DecodeImageDimensions(StringPiece* remaining, ImageDim* dims) {
  if (remaining->size() < 4) {
    // url too short to hold dimensions.
    return false;
  }
  bool ok, has_width, has_height;
  uint32 width = DecodeDimension(remaining, &ok, &has_width);
  if (!ok || ((*remaining)[0] != kCodeSeparator)) {   // And check the separator
    return false;
  }

  // Consume the separator
  remaining->remove_prefix(1);
  uint32 height = DecodeDimension(remaining, &ok, &has_height);
  if (remaining->size() < 1 || !ok) {
    return false;
  }
  if (!IsValidCode((*remaining)[0])) {  // And check the terminator
    return false;
  }
  // Parsed successfully.
  // Now store the dimensions that were present.
  if (has_width) {
    dims->set_width(width);
  }
  if (has_height) {
    dims->set_height(height);
  } else if (!has_width) {
    // Both dimensions are missing!  NxN[xw] is not allowed, as it's ambiguous
    // with the shorter encoding.  We should never get here in real life.
    return false;
  }
  return true;
}

}  // namespace

// The generic Decode interface is supplied so that
// RewriteContext and/or RewriteDriver can decode any
// ResourceNamer::name() field and find the set of URLs that are
// referenced.
bool ImageUrlEncoder::Decode(const StringPiece& encoded,
                             StringVector* urls,
                             ResourceContext* data,
                             MessageHandler* handler) const {
  if (encoded.empty()) {
    return false;
  }
  ImageDim* dims = data->mutable_desired_image_dims();
  // Note that "remaining" is shortened from the left as we parse.
  StringPiece remaining(encoded);
  char terminator = remaining[0];
  if (IsValidCode(terminator)) {
    // No dimensions.  x... or w... or mx... or mw...
    // Do nothing.
  } else if (DecodeImageDimensions(&remaining, dims)) {
    // We've parsed the dimensions and they've been stripped from remaining.
    // Now set terminator properly.
    terminator = remaining[0];
  } else {
    return false;
  }
  // Remove the terminator
  remaining.remove_prefix(1);

  // Set mobile user agent & set webp only if its a legacy encoding.
  if (terminator == kCodeMobileUserAgent) {
    data->set_mobile_user_agent(true);
    // There must be a final kCodeWebpLossy,
    // kCodeWebpLossyLosslessAlpha, or kCodeSeparator. Otherwise,
    // invalid.
    // Check and strip it.
    if (remaining.empty()) {
      return false;
    }
    terminator = remaining[0];
    if (terminator != kCodeWebpLossy &&
        terminator != kCodeWebpLossyLosslessAlpha &&
        terminator != kCodeSeparator) {
      return false;
    }
    remaining.remove_prefix(1);
  }
  // Following terminator check is for Legacy Url Encoding.
  // If it's a legacy "x" encoding, we don't overwrite the libwebp_level.
  // Example: if a webp-capable UA requested a legacy "x"-encoded url, we would
  // wind up with a ResourceContext specifying a different webp-version of the
  // original resourcem, but at least it's safe to send that to the UA,
  // since we know it can handle it.
  //
  // In case it doesn't hit either of the following two conditions,
  // the libwebp level is taken as the one set previously. This will happen
  // mostly when the url is a Non-Legacy encoded one.
  if (terminator == kCodeWebpLossy) {
    data->set_libwebp_level(ResourceContext::LIBWEBP_LOSSY_ONLY);
  } else if (terminator == kCodeWebpLossyLosslessAlpha) {
    data->set_libwebp_level(ResourceContext::LIBWEBP_LOSSY_LOSSLESS_ALPHA);
  }

  GoogleString* url = StringVectorAdd(urls);
  if (UrlEscaper::DecodeFromUrlSegment(remaining, url)) {
    return true;
  } else {
    urls->pop_back();
    return false;
  }
}

void ImageUrlEncoder::SetLibWebpLevel(
    const RewriteOptions& options,
    const RequestProperties& request_properties,
    ResourceContext* resource_context) {
  ResourceContext::LibWebpLevel libwebp_level = ResourceContext::LIBWEBP_NONE;
  // We do enabled checks before Setting the Webp Level, since it avoids writing
  // two metadata cache keys for same output if webp rewriting is disabled.
  if (request_properties.SupportsWebpAnimated() &&
      (options.Enabled(RewriteOptions::kRecompressWebp) ||
       options.Enabled(RewriteOptions::kConvertToWebpAnimated))) {
    libwebp_level = ResourceContext::LIBWEBP_ANIMATED;
  } else if (request_properties.SupportsWebpLosslessAlpha() &&
             (options.Enabled(RewriteOptions::kRecompressWebp) ||
              options.Enabled(RewriteOptions::kConvertToWebpLossless))) {
    libwebp_level = ResourceContext::LIBWEBP_LOSSY_LOSSLESS_ALPHA;
  } else if (request_properties.SupportsWebpRewrittenUrls() &&
             (options.Enabled(RewriteOptions::kRecompressWebp) ||
              options.Enabled(RewriteOptions::kConvertToWebpLossless) ||
              options.Enabled(RewriteOptions::kConvertJpegToWebp))) {
    libwebp_level = ResourceContext::LIBWEBP_LOSSY_ONLY;
  }
  resource_context->set_libwebp_level(libwebp_level);
}

bool ImageUrlEncoder::IsWebpRewrittenUrl(const GoogleUrl& gurl) {
  ResourceNamer namer;
  if (!namer.DecodeIgnoreHashAndSignature(gurl.LeafSansQuery())) {
    return false;
  }

  // We only convert images to WebP whose URLs were created by
  // ImageRewriteFilter, whose ID is "ic".  Note that this code will
  // not ordinarily be awakened for other filters (notabley .ce.) but
  // is left in for paranoia in case this code is live for some path
  // of in-place resource optimization of cache-extended images.
  if (namer.id() != RewriteOptions::kImageCompressionId) {
    return false;
  }

  StringPiece webp_extension_with_dot = kContentTypeWebp.file_extension();
  return namer.ext() == webp_extension_with_dot.substr(1);
}

void ImageUrlEncoder::SetWebpAndMobileUserAgent(
    const RewriteDriver& driver,
    ResourceContext* context) {
  const RewriteOptions* options = driver.options();
  if (context == NULL) {
    return;
  }

  if (driver.options()->serve_rewritten_webp_urls_to_any_agent() &&
      !driver.fetch_url().empty() &&
      IsWebpRewrittenUrl(driver.decoded_base_url())) {
    // See https://developers.google.com/speed/webp/faq#which_web_browsers_natively_support_webp
    // which indicates that the latest versions of all browsers that support
    // webp, support webp lossless as well.
    context->set_libwebp_level(ResourceContext::LIBWEBP_LOSSY_LOSSLESS_ALPHA);
  } else {
    SetLibWebpLevel(*options, *driver.request_properties(), context);
  }

  if (options->Enabled(RewriteOptions::kDelayImages) &&
      options->Enabled(RewriteOptions::kResizeMobileImages) &&
      driver.request_properties()->IsMobile()) {
    context->set_mobile_user_agent(true);
  }
}

void ImageUrlEncoder::SetSmallScreen(const RewriteDriver& driver,
    ResourceContext* context) {
  // We used to do checking based on screen resolution, but we actually care
  // about is physically small screens even if they're high-density.
  context->set_may_use_small_screen_quality(
      driver.options()->HasValidSmallScreenQualities() &&
      driver.request_properties()->IsMobile());
}

// Each image in lossless format may have up to 2 optimized versions
// (2 formats: Webp and GIF/PNG), while each image in lossy format may have up
// to 6 optimized versions (2 formats: WebP and JPEG; 3 qualities: Save-Data
// quality, mobile quality, and regular quality).
//
// mobile_user_agent, if applies, doubles the optimized versions. However,
// this flag is usually not effective.
GoogleString ImageUrlEncoder::CacheKeyFromResourceContext(
    const ResourceContext& resource_context) {
  GoogleString user_agent_cache_key = "";
  switch (resource_context.libwebp_level()) {
    case ResourceContext::LIBWEBP_NONE:
      StrAppend(&user_agent_cache_key, kWebpNoneUserAgentKey);
      break;
    case ResourceContext::LIBWEBP_LOSSY_LOSSLESS_ALPHA:
      StrAppend(&user_agent_cache_key, kWebpLossyLossLessAlphaUserAgentKey);
      break;
    case ResourceContext::LIBWEBP_LOSSY_ONLY:
      StrAppend(&user_agent_cache_key, kWebpLossyUserAgentKey);
      break;
    case ResourceContext::LIBWEBP_ANIMATED:
      StrAppend(&user_agent_cache_key, kWebpAnimatedUserAgentKey);
      break;
  }
  if (resource_context.mobile_user_agent()) {
    StrAppend(&user_agent_cache_key, kMobileUserAgentKey);
  }

  // If the image will be compressed to a quality different than the regular
  // one, add a key to cache. The quality for Save-Data has higher precedence
  // than that for mobile, so does the key.
  if (resource_context.may_use_save_data_quality()) {
    StrAppend(&user_agent_cache_key, kSaveDataKey);
  } else if (resource_context.may_use_small_screen_quality()) {
    StrAppend(&user_agent_cache_key, kSmallScreenKey);
  }

  return user_agent_cache_key;
}

bool ImageUrlEncoder::AllowVaryOnUserAgent(
    const RewriteOptions& options,
    const RequestProperties& request_properties) {
  return (options.AllowVaryOnUserAgent() ||
          (options.AllowVaryOnAuto() && !request_properties.HasViaHeader()));
}

bool ImageUrlEncoder::AllowVaryOnAccept(
    const RewriteOptions& options,
    const RequestProperties& request_properties) {
  return (options.AllowVaryOnAccept() ||
          (options.AllowVaryOnAuto() && request_properties.HasViaHeader()));
}

}  // namespace net_instaweb
