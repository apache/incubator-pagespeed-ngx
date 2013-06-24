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

#include "net/instaweb/rewriter/public/image_rewrite_filter.h"

#include <limits.h>
#include <utility>

#include <algorithm>                    // for min
#include "base/logging.h"               // for CHECK, etc
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/rendered_image.pb.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/css_url_encoder.h"
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/rewriter/public/image.h"
#include "net/instaweb/rewriter/public/local_storage_cache_filter.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/statistics_work_bound.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/work_bound.h"
#include "pagespeed/kernel/util/simple_random.h"

namespace net_instaweb {

class UrlSegmentEncoder;

namespace {

// Determines the image options to be used for the given image. If neither
// of large screen and small screen values are set, use the base value. If
// any of them are set explicity, use the set value depending on the size of
// the screen. The only exception is if the image is being compressed for a
// small screen and the quality for small screen is set to a higher value.
// In this case, use the value that is explicitly set to be lower of the two.
int64 DetermineImageOptions(
    int64 base_value, int64 large_screen_value, int64 small_screen_value,
    bool is_small_screen) {
  int64 quality = (large_screen_value == -1) ? base_value : large_screen_value;
  if (is_small_screen && small_screen_value != -1) {
    quality = (quality == -1) ? small_screen_value :
        std::min(quality, small_screen_value);
  }
  return quality;
}

int64 GetPageWidth(const int64 page_height,
                   const int64 image_width,
                   const int64 image_height) {
  return (page_height * image_width + image_height / 2) / image_height;
}

int64 GetPageHeight(const int64 page_width,
                    const int64 image_height,
                    const int64 image_width) {
  return (page_width * image_height + image_width / 2) / image_width;
}

void SetDesiredDimensionsIfRequired(ImageDim* desired_dim,
                                    const ImageDim& image_dim) {
  int32 page_width = desired_dim->width();  // Rendered width.
  int32 page_height = desired_dim->height();  // Rendered height.
  const int64 image_width = image_dim.width();
  const int64 image_height = image_dim.height();
  if (!desired_dim->has_width()) {
    // Fill in a missing page height:
    //   page_height * (image_width / image_height),
    // rounding the result.
    // To avoid fractions we instead group as
    //   (page_height * image_width) / image_height and do the
    // math in int64 to avoid overflow in the numerator.  The additional
    // image_height / 2 causes us to round rather than truncate.
    desired_dim->set_height(page_height);
    desired_dim->set_width(static_cast<int32>(GetPageWidth(
        page_height, image_width, image_height)));
  } else if (!desired_dim->has_height()) {
    desired_dim->set_width(page_width);
    desired_dim->set_height(static_cast<int32>(GetPageHeight(
        page_width, image_height, image_width)));
  }
}

}  // namespace

// Expose kRelatedFilters and kRelatedOptions as class variables for the benefit
// of static-init-time merging in css_filter.cc.
const RewriteOptions::Filter ImageRewriteFilter::kRelatedFilters[] = {
  RewriteOptions::kConvertGifToPng,
  RewriteOptions::kConvertJpegToProgressive,
  RewriteOptions::kConvertJpegToWebp,
  RewriteOptions::kConvertPngToJpeg,
  RewriteOptions::kConvertToWebpLossless,
  RewriteOptions::kJpegSubsampling,
  RewriteOptions::kRecompressJpeg,
  RewriteOptions::kRecompressPng,
  RewriteOptions::kRecompressWebp,
  RewriteOptions::kResizeImages,
  RewriteOptions::kResizeMobileImages,
  RewriteOptions::kSquashImagesForMobileScreen,
  RewriteOptions::kStripImageColorProfile,
  RewriteOptions::kStripImageMetaData
};
const int ImageRewriteFilter::kRelatedFiltersSize = arraysize(kRelatedFilters);

const RewriteOptions::OptionEnum ImageRewriteFilter::kRelatedOptions[] = {
  RewriteOptions::kImageJpegNumProgressiveScans,
  RewriteOptions::kImageJpegNumProgressiveScansForSmallScreens,
  RewriteOptions::kImageJpegRecompressionQuality,
  RewriteOptions::kImageJpegRecompressionQualityForSmallScreens,
  RewriteOptions::kImageLimitOptimizedPercent,
  RewriteOptions::kImageLimitResizeAreaPercent,
  RewriteOptions::kImageMaxRewritesAtOnce,
  RewriteOptions::kImagePreserveURLs,
  RewriteOptions::kImageRecompressionQuality,
  RewriteOptions::kImageResolutionLimitBytes,
  RewriteOptions::kImageWebpRecompressionQuality,
  RewriteOptions::kImageWebpRecompressionQualityForSmallScreens,
  RewriteOptions::kProgressiveJpegMinBytes
};
const int ImageRewriteFilter::kRelatedOptionsSize = arraysize(kRelatedOptions);

// names for Statistics variables.
const char ImageRewriteFilter::kImageRewrites[] = "image_rewrites";
const char ImageRewriteFilter::kImageNoRewritesHighResolution[] =
    "image_norewrites_high_resolution";
const char kImageRewritesDroppedIntentionally[] =
    "image_rewrites_dropped_intentionally";
const char ImageRewriteFilter::kImageRewritesDroppedDecodeFailure[] =
    "image_rewrites_dropped_decode_failure";
const char ImageRewriteFilter::kImageRewritesDroppedServerWriteFail[] =
    "image_rewrites_dropped_server_write_fail";
const char ImageRewriteFilter::kImageRewritesDroppedMIMETypeUnknown[] =
    "image_rewrites_dropped_mime_type_unknown";
const char ImageRewriteFilter::kImageRewritesDroppedNoSavingResize[] =
    "image_rewrites_dropped_nosaving_resize";
const char ImageRewriteFilter::kImageRewritesDroppedNoSavingNoResize[] =
    "image_rewrites_dropped_nosaving_noresize";
const char ImageRewriteFilter::kImageRewritesDroppedDueToLoad[] =
    "image_rewrites_dropped_due_to_load";
const char ImageRewriteFilter::kImageRewritesSquashingForMobileScreen[] =
    "image_rewrites_squashing_for_mobile_screen";
const char kImageRewriteTotalBytesSaved[] = "image_rewrite_total_bytes_saved";
const char kImageRewriteTotalOriginalBytes[] =
    "image_rewrite_total_original_bytes";
const char kImageRewriteUses[] = "image_rewrite_uses";
const char kImageInline[] = "image_inline";
const char ImageRewriteFilter::kImageOngoingRewrites[] =
    "image_ongoing_rewrites";
const char ImageRewriteFilter::kImageResizedUsingRenderedDimensions[] =
    "image_resized_using_rendered_dimensions";
const char kImageWebpRewrites[] = "image_webp_rewrites";
const char ImageRewriteFilter::kInlinableImageUrlsPropertyName[] =
    "ImageRewriter-inlinable-urls";
const char ImageRewriteFilter::kImageRewriteLatencyOkMs[] =
    "image_rewrite_latency_ok_ms";
const char ImageRewriteFilter::kImageRewriteLatencyFailedMs[] =
    "image_rewrite_latency_failed_ms";

const char ImageRewriteFilter::kImageWebpFromGifTimeouts[] =
    "image_webp_conversion_gif_timeouts";
const char ImageRewriteFilter::kImageWebpFromPngTimeouts[] =
    "image_webp_conversion_png_timeouts";
const char ImageRewriteFilter::kImageWebpFromJpegTimeouts[] =
    "image_webp_conversion_jpeg_timeouts";

const char ImageRewriteFilter::kImageWebpFromGifSuccessMs[] =
    "image_webp_conversion_gif_success_ms";
const char ImageRewriteFilter::kImageWebpFromPngSuccessMs[] =
    "image_webp_conversion_png_success_ms";
const char ImageRewriteFilter::kImageWebpFromJpegSuccessMs[] =
    "image_webp_conversion_jpeg_success_ms";

const char ImageRewriteFilter::kImageWebpFromGifFailureMs[] =
    "image_webp_conversion_gif_failure_ms";
const char ImageRewriteFilter::kImageWebpFromPngFailureMs[] =
    "image_webp_conversion_png_failure_ms";
const char ImageRewriteFilter::kImageWebpFromJpegFailureMs[] =
    "image_webp_conversion_jpeg_failure_ms";

const char ImageRewriteFilter::kImageWebpWithAlphaTimeouts[] =
    "image_webp_alpha_timeouts";
const char ImageRewriteFilter::kImageWebpWithAlphaSuccessMs[] =
    "image_webp_alpha_success_ms";
const char ImageRewriteFilter::kImageWebpWithAlphaFailureMs[] =
    "image_webp_alpha_failure_ms";

const char ImageRewriteFilter::kImageWebpOpaqueTimeouts[] =
    "image_webp_opaque_timeouts";
const char ImageRewriteFilter::kImageWebpOpaqueSuccessMs[] =
    "image_webp_opaque_success_ms";
const char ImageRewriteFilter::kImageWebpOpaqueFailureMs[] =
    "image_webp_opaque_failure_ms";

const int kNotCriticalIndex = INT_MAX;

// This is the resized placeholder image width for mobile.
const int kDelayImageWidthForMobile = 320;

namespace {

void LogImageBackgroundRewriteActivity(
    RewriteDriver* driver,
    RewriterApplication::Status status,
    const GoogleString& url,
    const char* id,
    int original_size,
    int optimized_size,
    bool is_recompressed,
    ImageType original_image_type,
    ImageType optimized_image_type,
    bool is_resized) {
  const RewriteOptions* options = driver->options();
  if (!options->log_background_rewrites()) {
    return;
  }

  AbstractLogRecord* log_record =
      driver->request_context()->GetBackgroundRewriteLog(
          driver->server_context()->thread_system(),
          options->allow_logging_urls_in_log_record(),
          options->log_url_indices(),
          options->max_rewrite_info_log_size());

  // Write log for background rewrites.
  log_record->LogImageBackgroundRewriteActivity(status, url, id, original_size,
      optimized_size, is_recompressed, original_image_type,
      optimized_image_type, is_resized);
}

}  // namespace

class ImageRewriteFilter::Context : public SingleRewriteContext {
 public:
  Context(int64 css_image_inline_max_bytes,
          ImageRewriteFilter* filter, RewriteDriver* driver,
          RewriteContext* parent, ResourceContext* resource_context,
          bool is_css, int html_index, bool in_noscript_element)
      : SingleRewriteContext(driver, parent, resource_context),
        css_image_inline_max_bytes_(css_image_inline_max_bytes),
        filter_(filter),
        driver_(driver),
        is_css_(is_css),
        html_index_(html_index),
        in_noscript_element_(in_noscript_element) {}
  virtual ~Context() {}

  virtual void Render();
  virtual void RewriteSingle(const ResourcePtr& input,
                             const OutputResourcePtr& output);
  virtual const char* id() const { return filter_->id(); }
  virtual OutputResourceKind kind() const { return kRewrittenResource; }
  virtual const UrlSegmentEncoder* encoder() const;

  // Implements UserAgentCacheKey method of RewriteContext.
  virtual GoogleString UserAgentCacheKey(
      const ResourceContext* resource_context) const;

  // Implements EncodeUserAgentIntoResourceContext of RewriteContext.
  virtual void EncodeUserAgentIntoResourceContext(
      ResourceContext* context);

 private:
  friend class ImageRewriteFilter;

  int64 css_image_inline_max_bytes_;
  ImageRewriteFilter* filter_;
  RewriteDriver* driver_;
  bool is_css_;
  const int html_index_;
  bool in_noscript_element_;
  DISALLOW_COPY_AND_ASSIGN(Context);
};

void SetWebpCompressionOptions(
    const ResourceContext& resource_context,
    const RewriteOptions& options,
    const StringPiece& url,
    Image::ConversionVariables* webp_conversion_variables,
    Image::CompressionOptions* image_options) {
  switch (resource_context.libwebp_level()) {
      case ResourceContext::LIBWEBP_NONE:
        image_options->preferred_webp = Image::WEBP_NONE;
        image_options->allow_webp_alpha = false;
        VLOG(1) << "User agent is not webp capable";
        break;
      case ResourceContext::LIBWEBP_LOSSY_ONLY:
        image_options->preferred_webp = Image::WEBP_LOSSY;
        image_options->allow_webp_alpha = false;
        VLOG(1) << "User agent is webp lossy capable ";
        break;
      case ResourceContext::LIBWEBP_LOSSY_LOSSLESS_ALPHA:
        image_options->allow_webp_alpha = true;
        if (options.Enabled(RewriteOptions::kConvertToWebpLossless)) {
          image_options->preferred_webp = Image::WEBP_LOSSLESS;
          VLOG(1) << "User agent is webp lossless+alpha capable "
                  << "and lossless images preferred";
        } else {
          image_options->preferred_webp = Image::WEBP_LOSSY;
          VLOG(1) << "User agent is webp lossless+alpha capable "
                  << "and lossy images preferred";
        }
        break;
      default:
        LOG(DFATAL) << "Unhandled libwebp_level";
  }
  image_options->webp_conversion_variables = webp_conversion_variables;
}

void ImageRewriteFilter::Context::RewriteSingle(
    const ResourcePtr& input_resource,
    const OutputResourcePtr& output_resource) {
  bool is_ipro = IsNestedIn(RewriteOptions::kInPlaceRewriteId);
  AttachDependentRequestTrace(is_ipro ? "IproProcessImage" : "ProcessImage");
  RewriteDone(
      filter_->RewriteLoadedResourceImpl(this, input_resource, output_resource),
      0);
}

void ImageRewriteFilter::Context::Render() {
  if (num_output_partitions() != 1) {
    // Partition failed since one of the inputs was unavailable; nothing to do.
    return;
  }

  CHECK_EQ(1, num_slots());

  const CachedResult* result = output_partition(0);
  bool rewrote_url = false;
  ResourceSlot* resource_slot = slot(0).get();
  if (is_css_) {
    rewrote_url = filter_->FinishRewriteCssImageUrl(css_image_inline_max_bytes_,
                                                    result, resource_slot);
  } else {
    if (!has_parent()) {
      // We use manual rendering for HTML, as we have to consider whether to
      // inline, and may also pass in width and height attributes.
      HtmlResourceSlot* html_slot = static_cast<HtmlResourceSlot*>(
          resource_slot);
      rewrote_url = filter_->FinishRewriteImageUrl(
          result, resource_context(),
          html_slot->element(), html_slot->attribute(), html_index_,
          resource_slot);
    }
    // Use standard rendering in case the rewrite is nested and not inside CSS.
  }
  if (rewrote_url) {
    // We wrote out the URL ourselves; don't let the default handling mess it up
    // (in particular replacing data: with out-of-line version)
    resource_slot->set_disable_rendering(true);
  }
}

const UrlSegmentEncoder* ImageRewriteFilter::Context::encoder() const {
  return filter_->encoder();
}

GoogleString ImageRewriteFilter::Context::UserAgentCacheKey(
    const ResourceContext* resource_context) const {
  if (resource_context != NULL) {
    // cache-key is sensitive to whether the UA supports webp or not.
    return ImageUrlEncoder::CacheKeyFromResourceContext(*resource_context);
  }
  return "";
}

void ImageRewriteFilter::Context::EncodeUserAgentIntoResourceContext(
    ResourceContext* context) {
  return filter_->EncodeUserAgentIntoResourceContext(context);
}

ImageRewriteFilter::ImageRewriteFilter(RewriteDriver* driver)
    : RewriteFilter(driver),
      image_counter_(0) {
  Statistics* stats = server_context_->statistics();
  image_rewrites_ = stats->GetVariable(kImageRewrites);
  image_resized_using_rendered_dimensions_ =
      stats->GetVariable(kImageResizedUsingRenderedDimensions);
  image_norewrites_high_resolution_ = stats->GetVariable(
      kImageNoRewritesHighResolution);
  image_rewrites_dropped_intentionally_ =
      stats->GetVariable(kImageRewritesDroppedIntentionally);
  image_rewrites_dropped_decode_failure_ =
      stats->GetVariable(kImageRewritesDroppedDecodeFailure);
  image_rewrites_dropped_server_write_fail_ =
      stats->GetVariable(kImageRewritesDroppedServerWriteFail);
  image_rewrites_dropped_mime_type_unknown_ =
      stats->GetVariable(kImageRewritesDroppedMIMETypeUnknown);
  image_rewrites_dropped_nosaving_resize_ =
      stats->GetVariable(kImageRewritesDroppedNoSavingResize);
  image_rewrites_dropped_nosaving_noresize_ =
      stats->GetVariable(kImageRewritesDroppedNoSavingNoResize);
  image_rewrites_dropped_due_to_load_ =
      stats->GetTimedVariable(kImageRewritesDroppedDueToLoad);
  image_rewrites_squashing_for_mobile_screen_ =
      stats->GetTimedVariable(kImageRewritesSquashingForMobileScreen);
  image_rewrite_total_bytes_saved_ =
      stats->GetVariable(kImageRewriteTotalBytesSaved);
  image_rewrite_total_original_bytes_ =
      stats->GetVariable(kImageRewriteTotalOriginalBytes);
  image_rewrite_uses_ = stats->GetVariable(kImageRewriteUses);
  image_inline_count_ = stats->GetVariable(kImageInline);
  image_webp_rewrites_ = stats->GetVariable(kImageWebpRewrites);

  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_GIF)->timeout_count =
      stats->GetVariable(kImageWebpFromGifTimeouts);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_PNG)->timeout_count =
      stats->GetVariable(kImageWebpFromPngTimeouts);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_JPEG)->timeout_count =
      stats->GetVariable(kImageWebpFromJpegTimeouts);

  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_GIF)->success_ms =
      stats->GetHistogram(kImageWebpFromGifSuccessMs);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_PNG)->success_ms =
      stats->GetHistogram(kImageWebpFromPngSuccessMs);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_JPEG)->success_ms =
      stats->GetHistogram(kImageWebpFromJpegSuccessMs);

  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_GIF)->failure_ms =
      stats->GetHistogram(kImageWebpFromGifFailureMs);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_PNG)->failure_ms =
      stats->GetHistogram(kImageWebpFromPngFailureMs);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_JPEG)->failure_ms =
      stats->GetHistogram(kImageWebpFromJpegFailureMs);

  webp_conversion_variables_.Get(
      Image::ConversionVariables::NONOPAQUE)->timeout_count =
      stats->GetVariable(kImageWebpWithAlphaTimeouts);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::NONOPAQUE)->success_ms =
      stats->GetHistogram(kImageWebpWithAlphaSuccessMs);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::NONOPAQUE)->failure_ms =
      stats->GetHistogram(kImageWebpWithAlphaFailureMs);

  webp_conversion_variables_.Get(
      Image::ConversionVariables::OPAQUE)->timeout_count =
      stats->GetVariable(kImageWebpOpaqueTimeouts);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::OPAQUE)->success_ms =
      stats->GetHistogram(kImageWebpOpaqueSuccessMs);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::OPAQUE)->failure_ms =
      stats->GetHistogram(kImageWebpOpaqueFailureMs);

  image_rewrite_latency_ok_ms_ = stats->GetHistogram(kImageRewriteLatencyOkMs);
  image_rewrite_latency_failed_ms_ =
      stats->GetHistogram(kImageRewriteLatencyFailedMs);

  Variable* image_ongoing_rewrites = stats->GetVariable(kImageOngoingRewrites);
  work_bound_.reset(
      new StatisticsWorkBound(image_ongoing_rewrites,
                              driver->options()->image_max_rewrites_at_once()));
}

ImageRewriteFilter::~ImageRewriteFilter() {}

void ImageRewriteFilter::InitStats(Statistics* statistics) {
#ifndef NDEBUG
  for (int i = 1; i < kRelatedFiltersSize; ++i) {
    CHECK_LT(kRelatedFilters[i - 1], kRelatedFilters[i])
        << "kRelatedFilters not in enum-value order";
  }
  for (int i = 1; i < kRelatedOptionsSize; ++i) {
    CHECK_LT(kRelatedOptions[i - 1], kRelatedOptions[i])
        << "kRelatedOptions not in enum-value order";
  }
#endif

  statistics->AddVariable(kImageRewrites);
  statistics->AddVariable(kImageResizedUsingRenderedDimensions);
  statistics->AddVariable(kImageNoRewritesHighResolution);
  statistics->AddVariable(kImageRewritesDroppedIntentionally);
  statistics->AddVariable(kImageRewritesDroppedDecodeFailure);
  statistics->AddVariable(kImageRewritesDroppedMIMETypeUnknown);
  statistics->AddVariable(kImageRewritesDroppedServerWriteFail);
  statistics->AddVariable(kImageRewritesDroppedNoSavingResize);
  statistics->AddVariable(kImageRewritesDroppedNoSavingNoResize);
  statistics->AddTimedVariable(kImageRewritesDroppedDueToLoad,
                               ServerContext::kStatisticsGroup);
  statistics->AddTimedVariable(kImageRewritesSquashingForMobileScreen,
                               ServerContext::kStatisticsGroup);
  statistics->AddVariable(kImageRewriteTotalBytesSaved);
  statistics->AddVariable(kImageRewriteTotalOriginalBytes);
  statistics->AddVariable(kImageRewriteUses);
  statistics->AddVariable(kImageInline);
  statistics->AddVariable(kImageWebpRewrites);
  // We want image_ongoing_rewrites to be global even if we do per-vhost
  // stats, as it's used for a StatisticsWorkBound.
  statistics->AddGlobalVariable(kImageOngoingRewrites);
  statistics->AddHistogram(kImageRewriteLatencyOkMs);
  statistics->AddHistogram(kImageRewriteLatencyFailedMs);

  statistics->AddVariable(kImageWebpFromGifTimeouts);
  statistics->AddVariable(kImageWebpFromPngTimeouts);
  statistics->AddVariable(kImageWebpFromJpegTimeouts);

  statistics->AddHistogram(kImageWebpFromGifSuccessMs);
  statistics->AddHistogram(kImageWebpFromPngSuccessMs);
  statistics->AddHistogram(kImageWebpFromJpegSuccessMs);

  statistics->AddHistogram(kImageWebpFromGifFailureMs);
  statistics->AddHistogram(kImageWebpFromPngFailureMs);
  statistics->AddHistogram(kImageWebpFromJpegFailureMs);

  statistics->AddVariable(kImageWebpWithAlphaTimeouts);
  statistics->AddHistogram(kImageWebpWithAlphaSuccessMs);
  statistics->AddHistogram(kImageWebpWithAlphaFailureMs);

  statistics->AddVariable(kImageWebpOpaqueTimeouts);
  statistics->AddHistogram(kImageWebpOpaqueSuccessMs);
  statistics->AddHistogram(kImageWebpOpaqueFailureMs);
}

void ImageRewriteFilter::SetupRenderedImageDimensionsMap(
    const RenderedImages& rendered_images) {
  RenderedImageDimensionsMap* map = new RenderedImageDimensionsMap;
  for (int i = 0; i < rendered_images.image_size(); ++i) {
    const RenderedImages_Image& images = rendered_images.image(i);
    (*map)[images.src()] = std::make_pair(
        images.rendered_width(), images.rendered_height());
  }
  rendered_images_map_.reset(map);
}

void ImageRewriteFilter::StartDocumentImpl() {
  image_counter_ = 0;
  inlinable_urls_.clear();
  driver_->log_record()->LogRewriterHtmlStatus(
      RewriteOptions::kImageCompressionId, RewriterHtmlApplication::ACTIVE);
  rendered_images_map_.reset(NULL);
  if (driver_->options()->Enabled(
      RewriteOptions::kResizeToRenderedImageDimensions)) {
    CriticalImagesFinder* finder =
        driver_->server_context()->critical_images_finder();
    scoped_ptr<RenderedImages> rendered_images(
        finder->ExtractRenderedImageDimensionsFromCache(driver_));
    if (rendered_images != NULL) {
      SetupRenderedImageDimensionsMap(*rendered_images);
    }
  }
}

// Allocate and initialize CompressionOptions object based on RewriteOptions and
// ResourceContext.
Image::CompressionOptions* ImageRewriteFilter::ImageOptionsForLoadedResource(
    const ResourceContext& resource_context, const ResourcePtr& input_resource,
    bool is_css) {
  Image::CompressionOptions* image_options = new Image::CompressionOptions();
  int64 input_size = static_cast<int64>(input_resource->contents().size());
  // Disable webp conversion for images in CSS if the original image size is
  // greater than max_image_bytes_in_css_for_webp. This is because webp does not
  // support progressive which causes a perceptible delay in the loading of
  // large background images.
  const RewriteOptions* options = driver_->options();
  if ((resource_context.libwebp_level() != ResourceContext::LIBWEBP_NONE) &&
      // TODO(vchudnov): Consider whether we want to treat CSS images
      // differently.
      (!is_css || input_size <= options->max_image_bytes_for_webp_in_css())) {
    SetWebpCompressionOptions(resource_context, *options, input_resource->url(),
                              &webp_conversion_variables_,
                              image_options);
  }
  image_options->jpeg_quality =
      DetermineImageOptions(options->image_recompress_quality(),
          options->image_jpeg_recompress_quality(),
          options->image_jpeg_recompress_quality_for_small_screens(),
          resource_context.use_small_screen_quality());
  image_options->webp_quality =
      DetermineImageOptions(options->image_recompress_quality(),
          options->image_webp_recompress_quality(),
          options->image_webp_recompress_quality_for_small_screens(),
          resource_context.use_small_screen_quality());
  image_options->jpeg_num_progressive_scans =
      DetermineImageOptions(-1, options->image_jpeg_num_progressive_scans(),
          options->image_jpeg_num_progressive_scans_for_small_screens(),
          resource_context.use_small_screen_quality());
  image_options->progressive_jpeg =
      options->Enabled(RewriteOptions::kConvertJpegToProgressive) &&
      input_size >= options->progressive_jpeg_min_bytes();
  image_options->progressive_jpeg_min_bytes =
      options->progressive_jpeg_min_bytes();
  image_options->convert_png_to_jpeg =
      options->Enabled(RewriteOptions::kConvertPngToJpeg);
  image_options->convert_gif_to_png =
      options->Enabled(RewriteOptions::kConvertGifToPng);
  image_options->convert_jpeg_to_webp =
      options->Enabled(RewriteOptions::kConvertJpegToWebp);
  image_options->recompress_jpeg =
      options->Enabled(RewriteOptions::kRecompressJpeg);
  image_options->recompress_png =
      options->Enabled(RewriteOptions::kRecompressPng);
  image_options->recompress_webp =
      options->Enabled(RewriteOptions::kRecompressWebp);
  image_options->retain_color_profile =
      !options->Enabled(RewriteOptions::kStripImageColorProfile);
  image_options->retain_exif_data =
      !options->Enabled(RewriteOptions::kStripImageMetaData);
  image_options->retain_color_sampling =
      !options->Enabled(RewriteOptions::kJpegSubsampling);
  image_options->webp_conversion_timeout_ms =
      options->image_webp_timeout_ms();

  return image_options;
}

// Resize image if necessary, returning true if this resizing succeeds and false
// if it's unnecessary or fails.
bool ImageRewriteFilter::ResizeImageIfNecessary(
    const RewriteContext* rewrite_context, const GoogleString& url,
    ResourceContext* resource_context, Image* image, CachedResult* cached) {
  bool resized = false;
  // Begin by resizing the image if necessary
  ImageDim image_dim;
  image->Dimensions(&image_dim);

  // Here we are computing the size of the image as described by the html on the
  // page or as desired by mobile screen resolutions. If we succeed in doing so,
  // that will be the desired image size. Otherwise we may fill in
  // desired_image_dims later based on actual image size.
  ImageDim* desired_dim = resource_context->mutable_desired_image_dims();
  const ImageDim* post_resize_dim = &image_dim;
  if (ShouldResize(*resource_context, url, image, desired_dim)) {
    const char* message;  // Informational message for logging only.
    if (image->ResizeTo(*desired_dim)) {
      post_resize_dim = desired_dim;
      message = "Resized";
      resized = true;
    } else {
      message = "Couldn't resize";
    }
    driver_->InfoAt(rewrite_context, "%s image `%s' from %dx%d to %dx%d",
                    message, url.c_str(),
                    image_dim.width(), image_dim.height(),
                    desired_dim->width(), desired_dim->height());
  }

  // Cache image dimensions, including any resizing we did.
  // This happens regardless of whether we rewrite the image contents.
  if (ImageUrlEncoder::HasValidDimensions(*post_resize_dim)) {
    ImageDim* dims = cached->mutable_image_file_dims();
    dims->set_width(post_resize_dim->width());
    dims->set_height(post_resize_dim->height());
  }
  return resized;
}

// Determines whether an image should be resized based on the current options.
//
// Returns the dimensions to resize to in *desired_dimensions.
bool ImageRewriteFilter::ShouldResize(const ResourceContext& resource_context,
                                      const GoogleString& url,
                                      Image* image,
                                      ImageDim* desired_dim) {
  const RewriteOptions* options = driver_->options();
  if (!options->Enabled(RewriteOptions::kResizeImages) &&
      !options->Enabled(RewriteOptions::kResizeToRenderedImageDimensions)) {
    return false;
  }

  if (image->content_type()->type() != ContentType::kGif ||
      options->Enabled(RewriteOptions::kConvertGifToPng) ||
      options->Enabled(RewriteOptions::kDelayImages)) {
    *desired_dim = resource_context.desired_image_dims();
    ImageDim image_dim;
    image->Dimensions(&image_dim);
    if (options->Enabled(RewriteOptions::kResizeToRenderedImageDimensions)) {
      // Respect the aspect ratio of the image when doing the resize.
      SetDesiredDimensionsIfRequired(desired_dim, image_dim);
    } else {
      UpdateDesiredImageDimsIfNecessary(
          image_dim, resource_context, desired_dim);
      if (options->Enabled(RewriteOptions::kResizeImages) &&
          ImageUrlEncoder::HasValidDimension(*desired_dim) &&
          ImageUrlEncoder::HasValidDimensions(image_dim)) {
        SetDesiredDimensionsIfRequired(desired_dim, image_dim);
      }
    }
    if (ImageUrlEncoder::HasValidDimension(*desired_dim) &&
        ImageUrlEncoder::HasValidDimensions(image_dim)) {
      const int64 page_area =
          static_cast<int64>(desired_dim->width()) *
          desired_dim->height();
      const int64 image_area =
          static_cast<int64>(image_dim.width()) * image_dim.height();
      if (page_area * 100 <
          image_area * options->image_limit_resize_area_percent()) {
        return true;
      }
    }
  }
  return false;
}

RewriteResult ImageRewriteFilter::RewriteLoadedResourceImpl(
      Context* rewrite_context, const ResourcePtr& input_resource,
      const OutputResourcePtr& result) {
  rewrite_context->TracePrintf("Image rewrite: %s",
                               input_resource->url().c_str());
  MessageHandler* message_handler = driver_->message_handler();
  StringVector urls;
  ResourceContext resource_context;
  const RewriteOptions* options = driver_->options();

  resource_context.CopyFrom(*rewrite_context->resource_context());

  if (!encoder_.Decode(result->name(),
                       &urls, &resource_context, message_handler)) {
    image_rewrites_dropped_intentionally_->Add(1);
    image_rewrites_dropped_decode_failure_->Add(1);
    return kRewriteFailed;
  }

  // If requested, drop random image rewrites. Eventually, frequently requested
  // images will get optimized but the long tail won't be optimized much. We're
  // not particularly concerned about the quality of the PRNG here as it's just
  // deciding if we should optimize an image or not.
  int drop_percentage = options->rewrite_random_drop_percentage();
  if (drop_percentage > 0 &&
      !rewrite_context->IsNestedIn(RewriteOptions::kCssFilterId)) {
    // Note that we don't randomly drop if this is a nested context of the CSS
    // filter as we don't want to partially rewrite a CSS file.
    SimpleRandom* simple_random =
        rewrite_context->FindServerContext()->simple_random();
    if (drop_percentage > static_cast<int>(simple_random->Next() % 100)) {
      return kTooBusy;
    }
  }

  Image::CompressionOptions* image_options =
      ImageOptionsForLoadedResource(resource_context, input_resource,
                                    rewrite_context->is_css_);
  scoped_ptr<Image> image(
      NewImage(input_resource->contents(), input_resource->url(),
               server_context_->filename_prefix(), image_options,
               driver_->timer(), message_handler));

  // Initialize logging data.
  ImageType original_image_type = image->image_type();
  ImageType optimized_image_type = original_image_type;
  int original_size = image->input_size();
  int optimized_size = original_size;
  bool is_recompressed = false;
  bool is_resized = false;

  if (original_image_type == IMAGE_UNKNOWN) {
    image_rewrites_dropped_intentionally_->Add(1);
    image_rewrites_dropped_mime_type_unknown_->Add(1);
    driver_->InfoAt(
        rewrite_context, "%s: Image MIME type could not be "
        "discovered from reading magic bytes; rewriting dropped.",
        input_resource->url().c_str());
    return kRewriteFailed;
  }
  // We used to reject beacon images based on their size (1x1 or less) here, but
  // now rely on caching headers instead as this was missing a lot of padding
  // images that were ripe for inlining.
  RewriteResult rewrite_result = kTooBusy;

  ImageDim image_dim;
  image->Dimensions(&image_dim);
  int64 image_width = image_dim.width(), image_height = image_dim.height();
  if ((image_width*image_height*4) > options->image_resolution_limit_bytes()) {
    image_rewrites_dropped_intentionally_->Add(1);
    image_norewrites_high_resolution_->Add(1);
    return kRewriteFailed;
  }
  if (work_bound_->TryToWork()) {
    rewrite_result = kRewriteFailed;
    int64 rewrite_time_start_ms = server_context_->timer()->NowMs();

    CachedResult* cached = result->EnsureCachedResultCreated();
    is_resized = ResizeImageIfNecessary(
        rewrite_context, input_resource->url(),
        &resource_context, image.get(), cached);

    // Now re-compress the (possibly resized) image, and decide if it's
    // saved us anything.
    if (is_resized || options->ImageOptimizationEnabled()) {
      // Call output_size() before image_type(). When output_size() is called,
      // the image will be recompressed and the image type may be changed
      // in order to get the smallest output.
      optimized_size = image->output_size();
      optimized_image_type = image->image_type();
      is_recompressed = true;

      // The image has been recompressed (and potentially resized). However,
      // the recompressed image may not be used unless the file size is reduced.
      if (image->output_size() * 100 <
          image->input_size() * options->image_limit_optimized_percent()) {
        // Here output image type could potentially be different from input
        // type.
        const ContentType* output_type =
            ImageToContentType(input_resource->url(), image.get());

        // Consider inlining output image (no need to check input, it's bigger)
        // This needs to happen before Write to persist.
        SaveIfInlinable(image->Contents(), image->image_type(), cached);

        server_context_->MergeNonCachingResponseHeaders(input_resource, result);
        if (driver_->Write(
                ResourceVector(1, input_resource), image->Contents(),
                output_type, StringPiece() /* no charset for images */,
                result.get())) {
          driver_->InfoAt(
              rewrite_context,
              "Shrinking image `%s' (%u bytes) to `%s' (%u bytes)",
              input_resource->url().c_str(),
              static_cast<unsigned>(image->input_size()),
              result->url().c_str(),
              static_cast<unsigned>(image->output_size()));

          // Update stats.
          image_rewrites_->Add(1);
          image_rewrite_total_bytes_saved_->Add(
              image->input_size() - image->output_size());
          image_rewrite_total_original_bytes_->Add(image->input_size());
          if (result->type()->type() == ContentType::kWebp) {
            image_webp_rewrites_->Add(1);
          }

          rewrite_result = kRewriteOk;
        } else {
          // Server fails to write merged files.
          image_rewrites_dropped_server_write_fail_->Add(1);
          GoogleString msg(StringPrintf(
              "Server fails writing image content for `%s'; "
              "rewriting dropped.",
              input_resource->url().c_str()));
          driver_->InfoAt(rewrite_context, "%s", msg.c_str());
          rewrite_context->TracePrintf("%s", msg.c_str());
        }
      } else if (is_resized) {
        // Eliminate any image dimensions from a resize operation that
        // succeeded, but yielded overly-large output.
        image_rewrites_dropped_nosaving_resize_->Add(1);
        GoogleString msg(StringPrintf(
            "Shrink of image `%s' (%u -> %u bytes) doesn't save space; "
            "dropped.",
            input_resource->url().c_str(),
            static_cast<unsigned>(image->input_size()),
            static_cast<unsigned>(image->output_size())));
        driver_->InfoAt(rewrite_context, "%s", msg.c_str());
        rewrite_context->TracePrintf("%s", msg.c_str());
        ImageDim* dims = cached->mutable_image_file_dims();
        dims->clear_width();
        dims->clear_height();
      } else if (options->ImageOptimizationEnabled()) {
        // Fails due to overly-large output without resize.
        image_rewrites_dropped_nosaving_noresize_->Add(1);
        GoogleString msg(StringPrintf(
            "Recompressing image `%s' (%u -> %u bytes) doesn't save space; "
            "dropped.",
            input_resource->url().c_str(),
            static_cast<unsigned>(image->input_size()),
            static_cast<unsigned>(image->output_size())));
        driver_->InfoAt(rewrite_context, "%s", msg.c_str());
        rewrite_context->TracePrintf("%s", msg.c_str());
      }
    }
    cached->set_size(rewrite_result == kRewriteOk ? image->output_size() :
                     image->input_size());

    // Try inlining input image if output hasn't been inlined already.
    if (!cached->has_inlined_data()) {
      SaveIfInlinable(input_resource->contents(), original_image_type, cached);
    }

    int64 image_size = static_cast<int64>(image->output_size());
    if (options->Enabled(RewriteOptions::kDelayImages) &&
        !rewrite_context->in_noscript_element_ &&
        !cached->has_low_resolution_inlined_data() &&
        image_size >= options->min_image_size_low_resolution_bytes() &&
        image_size <= options->max_image_size_low_resolution_bytes()) {
      Image::CompressionOptions* image_options =
          new Image::CompressionOptions();
      SetWebpCompressionOptions(resource_context, *options,
                                input_resource->url(),
                                &webp_conversion_variables_,
                                image_options);

      image_options->jpeg_quality = options->image_recompress_quality();
      if (options->image_jpeg_recompress_quality() != -1) {
        // if jpeg quality is explicitly set, it takes precedence over
        // generic image quality.
        image_options->jpeg_quality = options->image_jpeg_recompress_quality();
      }
      image_options->webp_quality = options->image_recompress_quality();
      if (options->image_webp_recompress_quality() != -1) {
        image_options->webp_quality = options->image_webp_recompress_quality();
      }
      image_options->progressive_jpeg = false;
      image_options->convert_png_to_jpeg =
          options->Enabled(RewriteOptions::kConvertPngToJpeg);

      // Set to true since we optimize a gif to png before resize.
      image_options->convert_gif_to_png = true;
      image_options->recompress_jpeg = true;
      image_options->recompress_png = true;
      image_options->recompress_webp = true;

      // Since these are replaced with their high res versions, stripping
      // them off for low res images will further reduce bytes.
      image_options->retain_color_profile = false;
      image_options->retain_exif_data = false;
      image_options->retain_color_sampling = false;
      image_options->jpeg_num_progressive_scans =
          options->image_jpeg_num_progressive_scans();

      scoped_ptr<Image> low_image;
      if (driver_->options()->use_blank_image_for_inline_preview()) {
        image_options->use_white_for_blank_image = true;
        low_image.reset(BlankImageWithOptions(image_width, image_height,
            IMAGE_PNG, server_context_->filename_prefix(),
            driver_->timer(), message_handler, image_options));
        low_image->EnsureLoaded(true);
      } else {
        low_image.reset(NewImage(image->Contents(), input_resource->url(),
            server_context_->filename_prefix(), image_options,
            driver_->timer(), message_handler));
      }
      low_image->SetTransformToLowRes();
      if (image->Contents().size() > low_image->Contents().size()) {
        // TODO(pulkitg): Add a some sort of guarantee on how small inline
        // images will be.
        if (resource_context.mobile_user_agent()) {
          ResizeLowQualityImage(low_image.get(), input_resource, cached);
        } else {
          cached->set_low_resolution_inlined_data(low_image->Contents().data(),
                                                  low_image->Contents().size());
        }
        cached->set_low_resolution_inlined_image_type(
            static_cast<int>(low_image->image_type()));
      }
    }
    work_bound_->WorkComplete();
    if (rewrite_result == kRewriteOk) {
      image_rewrite_latency_ok_ms_->Add(
          server_context_->timer()->NowMs() - rewrite_time_start_ms);
    } else {
      image_rewrite_latency_failed_ms_->Add(
          server_context_->timer()->NowMs() - rewrite_time_start_ms);
    }

  } else {
    image_rewrites_dropped_due_to_load_->IncBy(1);
    GoogleString msg(StringPrintf("%s: Too busy to rewrite image.",
                                  input_resource->url().c_str()));
    rewrite_context->TracePrintf("%s", msg.c_str());
    message_handler->Message(kInfo, "%s", msg.c_str());
  }

  // All other conditions were updated in other code paths above.
  if (rewrite_result == kRewriteFailed) {
    image_rewrites_dropped_intentionally_->Add(1);
  } else if (rewrite_result == kRewriteOk) {
    rewrite_context->TracePrintf("Image rewrite success (%u -> %u)",
                                 static_cast<unsigned>(image->input_size()),
                                 static_cast<unsigned>(image->output_size()));
  }

  LogImageBackgroundRewriteActivity(driver(),
      rewrite_result == kRewriteOk ?
          RewriterApplication::APPLIED_OK : RewriterApplication::NOT_APPLIED,
      input_resource->url(), LoggingId(), original_size, optimized_size,
      is_recompressed, original_image_type, optimized_image_type, is_resized);

  return rewrite_result;
}

// Generate resized low quality image if the image width is not smaller than
// kDelayImageWidthForMobile. If image width is smaller than
// kDelayImageWidthForMobile, "delay_images" optimization is not very useful
// and no low quality image will be generated.
void ImageRewriteFilter::ResizeLowQualityImage(
    Image* low_image, const ResourcePtr& input_resource, CachedResult* cached) {
  ImageDim image_dim;
  low_image->Dimensions(&image_dim);
  if (image_dim.width() >= kDelayImageWidthForMobile) {
    const RewriteOptions* options = driver_->options();
    Image::CompressionOptions* image_options =
        new Image::CompressionOptions();
    image_options->jpeg_quality = options->image_recompress_quality();
    if (options->image_jpeg_recompress_quality() != -1) {
      // if jpeg quality is explicitly set, it takes precedence over
      // generic image quality.
      image_options->jpeg_quality = options->image_jpeg_recompress_quality();
    }
    image_options->webp_quality = options->image_recompress_quality();
    if (options->image_webp_recompress_quality() != -1) {
      image_options->webp_quality = options->image_webp_recompress_quality();
    }
    image_options->progressive_jpeg = false;
    image_options->convert_png_to_jpeg =
        options->Enabled(RewriteOptions::kConvertPngToJpeg);
    image_options->convert_gif_to_png =
        options->Enabled(RewriteOptions::kConvertGifToPng);
    image_options->recompress_jpeg =
        options->Enabled(RewriteOptions::kRecompressJpeg);
    image_options->recompress_png =
        options->Enabled(RewriteOptions::kRecompressPng);
    image_options->recompress_webp =
        options->Enabled(RewriteOptions::kRecompressWebp);
    scoped_ptr<Image> image(
        NewImage(low_image->Contents(), input_resource->url(),
                 server_context_->filename_prefix(), image_options,
                 driver_->timer(), driver_->message_handler()));
    image->SetTransformToLowRes();
    ImageDim resized_dim;
    resized_dim.set_width(kDelayImageWidthForMobile);
    resized_dim.set_height((static_cast<int64>(resized_dim.width()) *
                            image_dim.height()) / image_dim.width());
    MessageHandler* message_handler = driver_->message_handler();
    bool resized = image->ResizeTo(resized_dim);
    StringPiece contents = image->Contents();
    StringPiece old_contents = low_image->Contents();
    if (resized && contents.size() < old_contents.size()) {
      cached->set_low_resolution_inlined_data(contents.data(), contents.size());
      message_handler->Message(
          kInfo,
          "Resized low quality image (%s) from "
          "%dx%d(%d bytes) to %dx%d(%d bytes)",
          input_resource->url().c_str(),
          image_dim.width(), image_dim.height(),
          static_cast<int>(old_contents.size()),
          resized_dim.width(), resized_dim.width(),
          static_cast<int>(contents.size()));
    } else {
      message_handler->Message(
          kInfo,
          "Couldn't resize low quality image (%s) or resized image file is "
          "not smaller: "
          "%dx%d(%d bytes) => %dx%d(%d bytes)",
          input_resource->url().c_str(),
          image_dim.width(), image_dim.height(),
          static_cast<int>(old_contents.size()),
          resized_dim.width(), resized_dim.height(),
          static_cast<int>(contents.size()));
    }
  }
}

void ImageRewriteFilter::SaveIfInlinable(const StringPiece& contents,
                                         const ImageType image_type,
                                         CachedResult* cached) {
  // We retain inlining information if the image size is < the largest possible
  // inlining threshold, as an image might be used in both html and css and we
  // may see it first from the one with a smaller threshold. Note that this can
  // cause us to save inline information for an image that won't ever actually
  // be inlined (because it's too big to inline in html, say, and doesn't occur
  // in css).
  int64 image_inline_max_bytes =
      driver_->options()->MaxImageInlineMaxBytes();
  if (static_cast<int64>(contents.size()) < image_inline_max_bytes) {
    cached->set_inlined_data(contents.data(), contents.size());
    cached->set_inlined_image_type(static_cast<int>(image_type));
  }
}

// Convert (possibly NULL) Image* to corresponding (possibly NULL) ContentType*
const ContentType* ImageRewriteFilter::ImageToContentType(
    const GoogleString& origin_url, Image* image) {
  const ContentType* content_type = NULL;
  if (image != NULL) {
    // Even if we know the content type from the extension coming
    // in, the content-type can change as a result of compression,
    // e.g. gif to png, or jpeg to webp.
    return image->content_type();
  }
  return content_type;
}

void ImageRewriteFilter::BeginRewriteImageUrl(HtmlElement* element,
                                              HtmlElement::Attribute* src) {
  scoped_ptr<ResourceContext> resource_context(new ResourceContext);
  const RewriteOptions* options = driver_->options();

  // In case of RewriteOptions::image_preserve_urls() we do not want to use
  // image dimension information from HTML/CSS.
  if ((options->Enabled(RewriteOptions::kResizeImages) ||
       options->Enabled(RewriteOptions::kResizeToRenderedImageDimensions))&&
      !driver_->options()->image_preserve_urls()) {
    ImageDim* desired_dim = resource_context->mutable_desired_image_dims();
    GetDimensions(element, desired_dim, src);
    if ((desired_dim->width() == 0 || desired_dim->height() == 0 ||
         (desired_dim->width() == 1 && desired_dim->height() == 1))) {
      // This is either a beacon image, or an attempt to prefetch.  Drop the
      // desired dimensions so that the image is not resized.
      resource_context->clear_desired_image_dims();
    }
  }
  StringPiece url(src->DecodedValueOrNull());

  EncodeUserAgentIntoResourceContext(resource_context.get());

  ResourcePtr input_resource = CreateInputResource(src->DecodedValueOrNull());
  if (input_resource.get() != NULL) {
    // If the image will be inlined and the local storage cache is enabled, add
    // the LSC marker attribute to this element so that the LSC filter knows to
    // insert the relevant javascript functions.
    if (driver_->request_properties()->SupportsImageInlining()) {
      LocalStorageCacheFilter::InlineState state;
      LocalStorageCacheFilter::AddStorableResource(src->DecodedValueOrNull(),
                                                   driver_,
                                                   true /* ignore cookie */,
                                                   element, &state);
    }
    Context* context = new Context(0 /* No CSS inlining, it's html */,
                                   this, driver_, NULL /*not nested */,
                                   resource_context.release(),
                                   false /*not css */, image_counter_++,
                                   noscript_element() != NULL);
    ResourceSlotPtr slot(driver_->GetSlot(input_resource, element, src));
    context->AddSlot(slot);
    if (driver_->options()->image_preserve_urls()) {
      slot->set_disable_rendering(true);
    }
    driver_->InitiateRewrite(context);
  }
}

bool ImageRewriteFilter::FinishRewriteCssImageUrl(
    int64 css_image_inline_max_bytes,
    const CachedResult* cached, ResourceSlot* slot) {
  GoogleString data_url;
  if (driver_->request_properties()->SupportsImageInlining() &&
      TryInline(css_image_inline_max_bytes, cached, slot, &data_url)) {
    // TODO(jmaessen): Can we make output URL reflect actual *usage*
    // of image inlining and/or webp images?
    const RewriteOptions* options = driver_->options();
    DCHECK(!options->cache_small_images_unrewritten())
        << "Modifying a URL slot despite "
        << "image_inlining_identify_and_cache_without_rewriting set.";
    slot->DirectSetUrl(data_url);
    image_inline_count_->Add(1);
    return true;
  } else if (cached->optimizable()) {
    image_rewrite_uses_->Add(1);
  }
  // Fall back to nested rewriting, which will also left trim the url if that
  // is required.
  return false;
}

namespace {

// Skip ascii whitespace, returning pointer to first non-whitespace character in
// accordance with:
//   http://www.whatwg.org/specs/web-apps/current-work/multipage/
//                  common-microsyntaxes.html#space-character
const char* SkipAsciiWhitespace(const char* position) {
  while (*position <= ' ' &&  // Quickly skip if no leading whitespace
         (*position == ' ' || *position == '\x09' || *position == '\x0A' ||
          *position == '\x0C' || *position == '\x0D')) {
    ++position;
  }
  return position;
}

bool GetDimensionAttribute(
    const HtmlElement* element, HtmlName::Keyword name, int* value) {
  const HtmlElement::Attribute* attribute = element->FindAttribute(name);
  if (attribute == NULL) {
    return false;
  }
  const char* position = attribute->DecodedValueOrNull();
  return ImageRewriteFilter::ParseDimensionAttribute(position, value);
}

// If the element has a width attribute, set it in page_dim.
void SetWidthFromAttribute(const HtmlElement* element, ImageDim* page_dim) {
  int32 width;
  if (GetDimensionAttribute(element, HtmlName::kWidth, &width)) {
    page_dim->set_width(width);
  }
}

// If the element has a height attribute, set it in page_dim.
void SetHeightFromAttribute(const HtmlElement* element, ImageDim* page_dim) {
  int32 height;
  if (GetDimensionAttribute(element, HtmlName::kHeight, &height)) {
    page_dim->set_height(height);
  }
}

void DeleteMatchingImageDimsAfterInline(
    const CachedResult* cached, HtmlElement* element) {
  // We used to take the absence of desired_image_dims here as license to delete
  // dimensions.  That was incorrect, as sometimes there were dimensions in the
  // page but the image was being enlarged on page and we can't strip the
  // enlargement out safely.  Now we also strip desired_image_dims when the
  // image is 1x1 or less.  As a result, we go back to the html to determine
  // whether it's safe to strip the width and height attributes, doing so only
  // if all dimensions that are present match the actual post-optimization image
  // dimensions.
  if (cached->has_image_file_dims()) {
    int attribute_width, attribute_height = -1;
    if (GetDimensionAttribute(element, HtmlName::kWidth, &attribute_width)) {
      if (cached->image_file_dims().width() == attribute_width) {
        // Width matches, height must either be absent or match.
        if (!element->FindAttribute(HtmlName::kHeight)) {
          // No height, just delete width.
          element->DeleteAttribute(HtmlName::kWidth);
        } else if (GetDimensionAttribute(
                element, HtmlName::kHeight, &attribute_height) &&
            cached->image_file_dims().height() == attribute_height) {
          // Both dimensions match, delete both.
          element->DeleteAttribute(HtmlName::kWidth);
          element->DeleteAttribute(HtmlName::kHeight);
        }
      }
    } else if (!element->FindAttribute(HtmlName::kWidth) &&
        GetDimensionAttribute(element, HtmlName::kHeight, &attribute_height) &&
        cached->image_file_dims().height() == attribute_height) {
      // No width, matching height
      element->DeleteAttribute(HtmlName::kHeight);
    }
  }
}

}  // namespace

bool ImageRewriteFilter::FinishRewriteImageUrl(
    const CachedResult* cached, const ResourceContext* resource_context,
    HtmlElement* element, HtmlElement::Attribute* src, int image_index,
    ResourceSlot* slot) {
  GoogleString src_value(src->DecodedValueOrNull());
  if (src_value.empty()) {
    return false;
  }

  const RewriteOptions* options = driver_->options();
  bool rewrote_url = false;
  bool image_inlined = false;
  const bool is_critical_image = IsHtmlCriticalImage(src_value);

  // See if we have a data URL, and if so use it if the browser can handle it
  // TODO(jmaessen): get rid of a string copy here. Tricky because ->SetValue()
  // copies implicitly.
  GoogleString data_url;
  if (driver_->request_properties()->SupportsImageInlining() &&
      (!driver_->options()->inline_only_critical_images() ||
       is_critical_image) &&
      TryInline(driver_->options()->ImageInlineMaxBytes(),
                cached, slot, &data_url)) {
    const RewriteOptions* options = driver_->options();
    DCHECK(!options->cache_small_images_unrewritten())
        << "Modifying a URL slot despite "
        << "image_inlining_identify_and_cache_without_rewriting set.";
    src->SetValue(data_url);
    DeleteMatchingImageDimsAfterInline(cached, element);
    // Note the use of the ORIGINAL url not the data url.
    LocalStorageCacheFilter::AddLscAttributes(src_value, *cached,
                                              driver_, element);
    image_inline_count_->Add(1);
    rewrote_url = true;
    image_inlined = true;
  }

  if (!image_inlined && !slot->disable_rendering()) {
    // Not inlined means we cannot store it in local storage.
    LocalStorageCacheFilter::RemoveLscAttributes(element, driver_);
    if (cached->optimizable()) {
      // Rewritten HTTP url
      src->SetValue(cached->url());
      image_rewrite_uses_->Add(1);
      rewrote_url = true;
    }
    if (options->Enabled(RewriteOptions::kInsertImageDimensions) &&
        (element->keyword() == HtmlName::kImg ||
         element->keyword() == HtmlName::kInput) &&
        !HasAnyDimensions(element) &&
        cached->has_image_file_dims() &&
        ImageUrlEncoder::HasValidDimensions(cached->image_file_dims())) {
      // Add image dimensions. We don't bother to resize if either dimension is
      // specified with units (em, %) rather than as absolute pixels. But note
      // that we DO attempt to include image dimensions even if we otherwise
      // choose not to optimize an image.
      const ImageDim& file_dims = cached->image_file_dims();
      driver_->AddAttribute(element, HtmlName::kWidth, file_dims.width());
      driver_->AddAttribute(element, HtmlName::kHeight, file_dims.height());
    }
  }

  bool low_res_src_inserted = false;
  bool try_low_res_src_insertion = false;
  ImageType low_res_image_type = IMAGE_UNKNOWN;
  if (options->Enabled(RewriteOptions::kDelayImages) &&
      (element->keyword() == HtmlName::kImg ||
       element->keyword() == HtmlName::kInput)) {
    try_low_res_src_insertion = true;
    int max_preview_image_index =
        driver_->options()->max_inlined_preview_images_index();
    if (!image_inlined &&
        !slot->disable_rendering() &&
        is_critical_image &&
        driver_->request_properties()->SupportsImageInlining() &&
        cached->has_low_resolution_inlined_data() &&
        (max_preview_image_index < 0 ||
         image_index < max_preview_image_index)) {
      low_res_image_type = static_cast<ImageType>(
          cached->low_resolution_inlined_image_type());

      const ContentType* content_type =
          Image::TypeToContentType(low_res_image_type);
      DCHECK(content_type != NULL) << "Invalid Image Type: "
                                   << low_res_image_type;
      if (content_type != NULL) {
        GoogleString data_url;
        DataUrl(*content_type, BASE64, cached->low_resolution_inlined_data(),
                &data_url);
        driver_->AddAttribute(element, HtmlName::kPagespeedLowResSrc, data_url);
        driver_->increment_num_inline_preview_images();
        low_res_src_inserted = true;
      } else {
        driver_->message_handler()->Message(kError,
                                            "Invalid low res image type: %d",
                                            low_res_image_type);
      }
    }
  }

  // Absolutify the image url for logging.
  GoogleUrl image_gurl(driver_->base_url(), src_value);
  driver_->log_record()->LogImageRewriteActivity(
      LoggingId(),
      image_gurl.spec_c_str(),
      (rewrote_url ?
       RewriterApplication::APPLIED_OK :
       RewriterApplication::NOT_APPLIED),
      image_inlined,
      is_critical_image,
      cached->optimizable(),
      cached->size(),
      try_low_res_src_insertion,
      low_res_src_inserted,
      low_res_image_type,
      cached->low_resolution_inlined_data().size());
  return rewrote_url;
}

bool ImageRewriteFilter::IsHtmlCriticalImage(StringPiece image_url) const {
  CriticalImagesFinder* finder =
      driver_->server_context()->critical_images_finder();
  if (!finder->IsMeaningful(driver_)) {
    // Default to all images being critical if we don't have meaningful critical
    // image information.
    return true;
  }
  GoogleUrl image_gurl(driver_->base_url(), image_url);
  return finder->IsHtmlCriticalImage(image_gurl.spec_c_str(), driver_);
}

bool ImageRewriteFilter::StoreUrlInPropertyCache(const StringPiece& url) {
  if (url.length() == 0) {
    return true;
  }
  PropertyPage* property_page = driver()->property_page();
  if (property_page == NULL) {
    LOG(WARNING) << "image_inlining_identify_and_cache_without_rewriting "
                 << "without PropertyPage.";
    return false;
  }
  const PropertyCache::Cohort* cohort =
      driver()->server_context()->dom_cohort();
  if (cohort == NULL) {
    LOG(WARNING) << "image_inlining_identify_and_cache_without_rewriting "
                 << "without configured DOM cohort.";
    return false;
  }
  PropertyValue* value = property_page->GetProperty(
      cohort, kInlinableImageUrlsPropertyName);
  VLOG(3) << "image_inlining_identify_and_cache_without_rewriting value "
          << "inserted into pcache: " << url;
  GoogleString new_value(StrCat("\"", url, "\""));
  if (value->has_value()) {
    StrAppend(&new_value, ",", value->value());
  }
  property_page->UpdateValue(
      cohort, kInlinableImageUrlsPropertyName, new_value);
  return true;
}

bool ImageRewriteFilter::HasAnyDimensions(HtmlElement* element) {
  if (element->FindAttribute(HtmlName::kWidth)) {
    return true;
  }
  if (element->FindAttribute(HtmlName::kHeight)) {
    return true;
  }
  css_util::StyleExtractor extractor(element);
  return extractor.HasAnyDimensions();
}

bool ImageRewriteFilter::ParseDimensionAttribute(
    const char* position, int* value) {
  if (position == NULL) {
    return false;
  }
  // Note that we rely heavily on null-termination of char* here to cause our
  // control flow to fall through when we reach end of string.  Numbered steps
  // correspond to the steps in the spec.
  //   http://www.whatwg.org/specs/web-apps/current-work/multipage/
  //                  common-microsyntaxes.html#percentages-and-dimensions
  // 3) Skip ascii whitespace
  position = SkipAsciiWhitespace(position);
  // 5) Skip leading plus
  if (*position == '+') {
    ++position;
  }
  unsigned int result = 0;  // unsigned for consistent overflow behavior.
  // 6,7,9) Process digits
  while ('0' <= *position && *position <= '9') {
    unsigned int new_result = result * 10 + *position - '0';
    if (new_result < result) {
      // Integer overflow.  Reject.
      return false;
    }
    result = new_result;
    ++position;
  }
  // 6,7,8) Reject if no digits or only zeroes, or conversion to signed will
  // fail.
  if (result < 1 || INT_MAX < result) {
    return false;
  }
  // 11) Process fraction (including 45. with nothing after the . )
  if (*position == '.') {
    ++position;
    if ('5' <= *position && *position <= '9' && result < INT_MAX) {
      // Round based on leading fraction digit, avoiding overflow.
      ++result;
      ++position;
    }
    // Discard all fraction digits.
    while ('0' <= *position && *position <= '9') {
      ++position;
    }
  }
  // Skip whitespace before a possible trailing px.  The spec allows other junk,
  // or a trailing percent, but we can't resize percentages and older browsers
  // don't resize when they encounter junk.
  position = SkipAsciiWhitespace(position);
  if (position[0] == 'p' && position[1] == 'x') {
    position = SkipAsciiWhitespace(position + 2);
  }
  // Reject if there's trailing junk.
  if (*position != '\0') {
    return false;
  }
  // 14) return result as length.
  *value = static_cast<int>(result);
  return true;
}

void ImageRewriteFilter::GetDimensions(HtmlElement* element,
                                       ImageDim* page_dim,
                                       const HtmlElement::Attribute* src) {
  css_util::StyleExtractor extractor(element);
  css_util::DimensionState state = extractor.state();
  int32 width = extractor.width();
  int32 height = extractor.height();
  // If the image has rendered dimensions stored in the property cache, update
  // the desired image dimensions.
  if (driver_->options()->Enabled(
      RewriteOptions::kResizeToRenderedImageDimensions) &&
      rendered_images_map_ != NULL) {
    StringPiece src_value(src->DecodedValueOrNull());
    if (!src_value.empty()) {
      GoogleUrl src_gurl(driver_->base_url(), src_value);
      if (src_gurl.is_valid()) {
        RenderedImageDimensionsMap::iterator iterator =
            rendered_images_map_->find(src_gurl.spec_c_str());
        if (iterator != rendered_images_map_->end()) {
          std::pair<int32, int32> &dimensions = iterator->second;
          if (dimensions.first != 0 && dimensions.second != 0) {
            image_resized_using_rendered_dimensions_->Add(1);
            page_dim->set_width(dimensions.first);
            page_dim->set_height(dimensions.second);
            return;
          }
        }
      }
    }
  }
  // If we didn't get a height dimension above, but there is a height
  // value in the style attribute, that means there's a height value
  // we can't process. This height will trump the height attribute in the
  // image tag, so we need to avoid resizing.
  // The same is true of width.
  switch (state) {
    case css_util::kNotParsable:
      break;
    case css_util::kHasBothDimensions:
      page_dim->set_width(width);
      page_dim->set_height(height);
      break;
    case css_util::kHasHeightOnly:
      page_dim->set_height(height);
      SetWidthFromAttribute(element, page_dim);
      break;
    case css_util::kHasWidthOnly:
      page_dim->set_width(width);
      SetHeightFromAttribute(element, page_dim);
      break;
    case css_util::kNoDimensions:
      SetWidthFromAttribute(element, page_dim);
      SetHeightFromAttribute(element, page_dim);
      break;
  }
}

bool ImageRewriteFilter::TryInline(
    int64 image_inline_max_bytes, const CachedResult* cached_result,
    ResourceSlot* slot, GoogleString* data_url) {
  if (!cached_result->has_inlined_data()) {
    return false;
  }
  StringPiece data = cached_result->inlined_data();
  if (static_cast<int64>(data.size()) >= image_inline_max_bytes) {
    return false;
  }
  // This is the decision point for whether or not an image is suitable for
  // inlining. After this point, we may skip inlining an image, but not
  // because of properties of the image.
  const RewriteOptions* options = driver_->options();
  if (options->cache_small_images_unrewritten()) {
    // Skip rewriting, record the URL for storage in the property cache,
    // suppress future rewrites to this slot, and return immediately.
    GoogleString url(slot->resource()->url());

    // Duplicate URLs are suppressed.
    if (inlinable_urls_.insert(url).second) {
      // This write to the property value allows downstream filters to observe
      // inlinable images within the same flush window. Note that this does not
      // induce a write to the underlying cache -- the value is written only
      // when the filter chain has finished execution.
      StoreUrlInPropertyCache(url);
    }
    // We disable rendering to prevent any rewriting of the URL that we'll
    // advertise in the property cache.
    slot->set_disable_rendering(true);
    return false;
  }
  DataUrl(
      *Image::TypeToContentType(
          static_cast<ImageType>(cached_result->inlined_image_type())),
      BASE64, data, data_url);
  return true;
}

void ImageRewriteFilter::EndElementImpl(HtmlElement* element) {
  // Don't rewrite if the image is broken by a flush.
  if (driver_->HasChildrenInFlushWindow(element)) {
    return;
  }
  // Don't rewrite if we cannot find the src attribute or if it's not an image.
  semantic_type::Category category;
  HtmlElement::Attribute* src = resource_tag_scanner::ScanElement(
      element, driver_, &category);
  if (category != semantic_type::kImage ||
      src == NULL || src->DecodedValueOrNull() == NULL) {
    return;
  }

  // Don't rewrite if there is a pagespeed_no_transform attribute.
  if (element->FindAttribute(HtmlName::kPagespeedNoTransform)) {
    // Remove the attribute
    element->DeleteAttribute(HtmlName::kPagespeedNoTransform);
    return;
  }

  // Ask the LSC filter to work out how to handle this element. A return
  // value of true means we don't have to rewrite it so can skip that.
  // The state is carried forward to after we initiate rewriting since
  // we might still have to modify the element.
  LocalStorageCacheFilter::InlineState state;
  if (LocalStorageCacheFilter::AddStorableResource(src->DecodedValueOrNull(),
                                                   driver_,
                                                   false /* check cookie */,
                                                   element, &state)) {
    return;
  }

  BeginRewriteImageUrl(element, src);
}

const UrlSegmentEncoder* ImageRewriteFilter::encoder() const {
  return &encoder_;
}

void ImageRewriteFilter::EncodeUserAgentIntoResourceContext(
    ResourceContext* context) const {
  ImageUrlEncoder::SetWebpAndMobileUserAgent(*driver_, context);
  CssUrlEncoder::SetInliningImages(*driver_->request_properties(), context);
  if (SquashImagesForMobileScreenEnabled()) {
    ImageUrlEncoder::SetUserAgentScreenResolution(driver_, context);
  }
  ImageUrlEncoder::SetSmallScreen(*driver_, context);
}

RewriteContext* ImageRewriteFilter::MakeRewriteContext() {
  ResourceContext* resource_context = new ResourceContext;
  EncodeUserAgentIntoResourceContext(resource_context);
  return new Context(0 /*No CSS inlining, it's html */,
                     this, driver_, NULL /*not nested */,
                     resource_context, false /*not css */,
                     kNotCriticalIndex,
                     false /*not in noscript */);
}

RewriteContext* ImageRewriteFilter::MakeNestedRewriteContextForCss(
    int64 css_image_inline_max_bytes,
    RewriteContext* parent, const ResourceSlotPtr& slot) {
  // Copy over the ResourceContext from the parent RewriteContext so that we
  // preserve request specific options, such as whether WebP rewriting is
  // allowed.
  ResourceContext* cloned_context = new ResourceContext;
  const ResourceContext* parent_context = parent->resource_context();
  if (parent_context != NULL) {
    cloned_context->CopyFrom(*parent_context);
  }

  if (cloned_context->libwebp_level() != ResourceContext::LIBWEBP_NONE) {
    // CopyFrom parent_context is not sufficient because parent_context checks
    // only UserAgentSupportsWebp when creating the context, but while
    // rewriting the image, rewrite options should also be checked.
    ImageUrlEncoder::SetLibWebpLevel(*driver_->request_properties(),
        cloned_context);
  }
  Context* context = new Context(css_image_inline_max_bytes,
                                 this, NULL /* driver*/, parent,
                                 cloned_context, true /*is css */,
                                 kNotCriticalIndex,
                                 false /*not in noscript */);
  context->AddSlot(slot);
  return context;
}

RewriteContext* ImageRewriteFilter::MakeNestedRewriteContext(
    RewriteContext* parent, const ResourceSlotPtr& slot) {
  ResourceContext* resource_context = new ResourceContext;
  DCHECK(parent != NULL);
  DCHECK(parent->resource_context() != NULL);
  if (parent != NULL && parent->resource_context() != NULL) {
    resource_context->CopyFrom(*(parent->resource_context()));
  }
  Context* context = new Context(
      0 /*No Css inling */, this, NULL /* driver */, parent, resource_context,
      false /*not css */, kNotCriticalIndex, false /*not in noscript */);
  context->AddSlot(slot);
  return context;
}

bool ImageRewriteFilter::SquashImagesForMobileScreenEnabled() const {
  const RewriteOptions* options = driver_->options();
  return options->Enabled(RewriteOptions::kResizeImages) &&
      options->Enabled(RewriteOptions::kSquashImagesForMobileScreen) &&
      driver_->request_properties()->IsMobile();
}

bool ImageRewriteFilter::UpdateDesiredImageDimsIfNecessary(
    const ImageDim& image_dim, const ResourceContext& resource_context,
    ImageDim* desired_dim) {
  bool updated = false;
  if (!resource_context.has_user_agent_screen_resolution()) {
    return false;
  }

  const ImageDim& screen_dim = resource_context.user_agent_screen_resolution();

  // Update the desired dimensions for screen if image squashing could make
  // the image size even smaller and there is no desired dimensions detected.
  // This is mainly for the data reduction purpose of mobile devices.
  // Note that squashing may break the layout of a web page if the page depends
  // on the original image size.
  // TODO(bolian): Consider squash images in the HTML path if dimensions are
  // present. But should also override the existing dimensions in the markup.
  if (ImageUrlEncoder::HasValidDimensions(image_dim) &&
      ImageUrlEncoder::HasValidDimensions(screen_dim) &&
      (image_dim.width() > screen_dim.width() ||
       image_dim.height() > screen_dim.height()) &&
      !desired_dim->has_width() &&
      !desired_dim->has_height()) {
    // We want to have one of the desired image dimensions the same as the
    // corresponding dimension of the screen and the other no larger than that
    // of the screen.
    const double width_ratio =
        static_cast<double>(screen_dim.width()) / image_dim.width();
    const double height_ratio =
        static_cast<double>(screen_dim.height()) / image_dim.height();
    if (width_ratio <= height_ratio) {
      desired_dim->set_width(screen_dim.width());
    } else {
      desired_dim->set_height(screen_dim.height());
    }
    image_rewrites_squashing_for_mobile_screen_->IncBy(1);
    updated = true;
  }
  return updated;
}

const RewriteOptions::Filter* ImageRewriteFilter::RelatedFilters(
    int* num_filters) const {
  *num_filters = kRelatedFiltersSize;
  return kRelatedFilters;
}

const RewriteOptions::OptionEnum* ImageRewriteFilter::RelatedOptions(
    int* num_options) const {
  *num_options = kRelatedOptionsSize;
  return kRelatedOptions;
}

void ImageRewriteFilter::DisableRelatedFilters(RewriteOptions* options) {
  for (int i = 0; i < kRelatedFiltersSize; ++i) {
    options->DisableFilter(kRelatedFilters[i]);
  }
}

}  // namespace net_instaweb
