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
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_escaper.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

namespace {

const char kCodeSeparator = 'x';
const char kCodeWebpLossy = 'w';               // for decoding legacy URLs.
const char kCodeWebpLossyLosslessAlpha = 'v';  // for decoding legacy URLs.
const char kCodeMobileUserAgent = 'm';         // for decoding legacy URLs.
const char kMissingDimension = 'N';

// Constants for UserAgent cache key enteries.
const char kWebpLossyUserAgentKey[] = "w";
const char kWebpLossyLossLessAlphaUserAgentKey[] = "v";
const char kMobileUserAgentKey[] = "m";
const char kUserAgentScreenResolutionKey[] = "sr";
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

struct ScreenResolution {
  int width;
  int height;
};

// Used by kSquashImagesForMobileScreen as target screen resolution.
// Keep the list small and in descending order of width.
// We use normalized screen resolution to reduce cache fragmentation.
static const ScreenResolution kNormalizedScreenResolutions[] = {
    {1080, 1920},
    {800, 1280},
    {600, 1024},
    {480, 800},
};

#ifndef NDEBUG
void CheckScreenResolutionOrder() {
  for (int i = 1, n = arraysize(kNormalizedScreenResolutions); i < n; ++i) {
    DCHECK_LT(kNormalizedScreenResolutions[i].width,
              kNormalizedScreenResolutions[i - 1].width);
  }
}
#endif

}  // namespace

const int ImageUrlEncoder::kSmallScreenSizeThresholdArea = 1280 * 800;

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
  if (request_properties.SupportsWebpRewrittenUrls() &&
      (options.Enabled(RewriteOptions::kRecompressWebp) ||
       options.Enabled(RewriteOptions::kConvertToWebpLossless) ||
       options.Enabled(RewriteOptions::kConvertJpegToWebp))) {
    if (request_properties.SupportsWebpLosslessAlpha() &&
        (options.Enabled(RewriteOptions::kRecompressWebp) ||
         options.Enabled(RewriteOptions::kConvertToWebpLossless))) {
      libwebp_level = ResourceContext::LIBWEBP_LOSSY_LOSSLESS_ALPHA;
    } else {
      libwebp_level = ResourceContext::LIBWEBP_LOSSY_ONLY;
    }
  }
  resource_context->set_libwebp_level(libwebp_level);
}

bool ImageUrlEncoder::IsWebpRewrittenUrl(const GoogleUrl& gurl) {
  ResourceNamer namer;
  if (!namer.Decode(gurl.LeafSansQuery())) {
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
  int width = 0, height = 0;
  if (driver.request_properties()->GetScreenResolution(&width, &height)) {
    if (width * height <= kSmallScreenSizeThresholdArea) {
      context->set_use_small_screen_quality(true);
    }
  } else {
    // If we did not find the screen resolution in kKnownScreenDimensions,
    // default to the IsMobile() check to set the small screen quality.
    context->set_use_small_screen_quality(
        driver.request_properties()->IsMobile());
  }
}

void ImageUrlEncoder::SetUserAgentScreenResolution(
    RewriteDriver* driver, ResourceContext* context) {
  if (context == NULL) {
    return;
  }
  int screen_width = 0;
  int screen_height = 0;
  if (driver->request_properties()->GetScreenResolution(
      &screen_width, &screen_height) &&
      GetNormalizedScreenResolution(
          screen_width, screen_height, &screen_width, &screen_height)) {
    ImageDim *dims = context->mutable_user_agent_screen_resolution();
    dims->set_width(screen_width);
    dims->set_height(screen_height);
  }
}

GoogleString ImageUrlEncoder::CacheKeyFromResourceContext(
    const ResourceContext& resource_context) {
  GoogleString user_agent_cache_key = "";
  if (resource_context.libwebp_level() ==
      ResourceContext::LIBWEBP_LOSSY_LOSSLESS_ALPHA) {
    StrAppend(&user_agent_cache_key, kWebpLossyLossLessAlphaUserAgentKey);
  }
  if (resource_context.libwebp_level() ==
      ResourceContext::LIBWEBP_LOSSY_ONLY) {
    StrAppend(&user_agent_cache_key, kWebpLossyUserAgentKey);
  }
  if (resource_context.mobile_user_agent()) {
    StrAppend(&user_agent_cache_key, kMobileUserAgentKey);
  }
  if (resource_context.has_use_small_screen_quality() &&
      resource_context.use_small_screen_quality()) {
    StrAppend(&user_agent_cache_key, kSmallScreenKey);
  }
  if (resource_context.has_user_agent_screen_resolution() &&
      resource_context.user_agent_screen_resolution().has_width() &&
      resource_context.user_agent_screen_resolution().has_height()) {
    StrAppend(
        &user_agent_cache_key,
        kUserAgentScreenResolutionKey,
        IntegerToString(
            resource_context.user_agent_screen_resolution().width()),
        StringPiece(&kCodeSeparator, 1),
        IntegerToString(
            resource_context.user_agent_screen_resolution().height()));
  }
  return user_agent_cache_key;
}

// Returns true if screen_width is less than any width in
// kNormalizedScreenResolutions, in which case the normalized resolution with
// the smallest width that is not less than screen_width will be returned.
bool ImageUrlEncoder::GetNormalizedScreenResolution(
    int screen_width, int screen_height, int* normalized_width,
    int* normalized_height) {
#ifndef NDEBUG
  CheckScreenResolutionOrder();
#endif

  bool normalized = false;
  for (int i = 0, n = arraysize(kNormalizedScreenResolutions); i < n; ++i) {
    if (kNormalizedScreenResolutions[i].width >= screen_width) {
      *normalized_width = kNormalizedScreenResolutions[i].width;
      *normalized_height = kNormalizedScreenResolutions[i].height;
      normalized = true;
    } else {
      break;
    }
  }
  return normalized;
}

}  // namespace net_instaweb
