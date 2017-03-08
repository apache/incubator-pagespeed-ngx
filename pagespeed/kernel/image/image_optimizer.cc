/*
 * Copyright 2016 Google Inc.
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

// Author: huibao@google.com (Huibao Lin)

#include "pagespeed/kernel/image/image_optimizer.h"

#include <algorithm>
#include <memory>

extern "C" {
#ifdef USE_SYSTEM_LIBPNG
#include "png.h"  // NOLINT
#else
#include "third_party/libpng/src/png.h"
#endif
}  // extern "C"

#include "base/logging.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/image/image_analysis.h"
#include "pagespeed/kernel/image/image_converter.h"
#include "pagespeed/kernel/image/image_frame_interface.h"
#include "pagespeed/kernel/image/image_resizer.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/image/jpeg_optimizer.h"
#include "pagespeed/kernel/image/pixel_format_optimizer.h"
#include "pagespeed/kernel/image/png_optimizer.h"
#include "pagespeed/kernel/image/read_image.h"
#include "pagespeed/kernel/image/scanline_interface.h"
#include "pagespeed/kernel/image/scanline_status.h"
#include "pagespeed/kernel/image/webp_optimizer.h"

using net_instaweb::MessageHandler;
using net_instaweb::Timer;
using pagespeed::image_compression::ImageDimensions;
using pagespeed::image_compression::WebpConfiguration;
using pagespeed::image_compression::ConversionTimeoutHandler;
using pagespeed::image_compression::JpegCompressionOptions;
using pagespeed::image_compression::PngCompressParams;
using pagespeed::image_compression::ScanlineWriterConfig;
using pagespeed::image_compression::ImageFormat;
using pagespeed::image_compression::MultipleFrameReader;
using pagespeed::image_compression::MultipleFrameWriter;
using pagespeed::image_compression::ScanlineStatus;
using pagespeed::image_compression::ImageOptions;
using pagespeed::image_compression::ShouldConvertToProgressive;
using pagespeed::image_compression::PixelFormatOptimizer;
using pagespeed::image_compression::ScanlineReaderInterface;
using pagespeed::image_compression::ScanlineResizer;
using pagespeed::image_compression::ScanlineWriterInterface;
using pagespeed::image_compression::ImageConverter;
using pagespeed::image_compression::ComputeImageType;
using pagespeed::image_compression::IMAGE_GIF;
using pagespeed::image_compression::IMAGE_PNG;
using pagespeed::image_compression::IMAGE_JPEG;
using pagespeed::image_compression::IMAGE_WEBP;
using pagespeed::image_compression::IMAGE_UNKNOWN;

namespace pagespeed {

namespace image_compression {

bool ImageOptimizer::ComputeDesiredFormat() {
  desired_lossless_ = false;
  optimized_format_ = IMAGE_UNKNOWN;
  if (is_animated_) {
    if (options_.allow_webp_animated()) {
      optimized_format_ = IMAGE_WEBP;
      desired_lossless_ = true;
    }
  } else if (is_transparent_) {
    if (options_.allow_webp_lossless_or_alpha()) {
      optimized_format_ = IMAGE_WEBP;
      desired_lossless_ = true;
    } else if (options_.allow_png()) {
      optimized_format_ = IMAGE_PNG;
      desired_lossless_ = true;
    }
  } else {
    // single frame and opaque
    if (is_photo_ &&
        (original_format_ == IMAGE_JPEG ||
         options_.allow_convert_lossless_to_lossy())) {
      // We can use lossy format.
      if (options_.allow_webp_lossy()) {
        optimized_format_ = IMAGE_WEBP;
      } else if (options_.allow_jpeg()) {
        optimized_format_ = IMAGE_JPEG;
      }
    } else if (options_.allow_webp_lossless_or_alpha()) {
      optimized_format_ = IMAGE_WEBP;
      desired_lossless_ = true;
    } else if (options_.allow_png()) {
      optimized_format_ = IMAGE_PNG;
      desired_lossless_ = true;
    }
  }

  return optimized_format_ != IMAGE_UNKNOWN;
}

// Computes the dimension for the resized image. In the requested dimensions
// you can specify the width, height, or both. This method will compute
// the unspecified dimension. If the input dimensions are valid and the
// output dimensions are smaller than the input, this method returns TRUE.
bool ImageOptimizer::ComputeResizedDimension() {
  if (original_width_ < 1 || original_height_ < 1 ||
      (requested_dim_.has_width() && requested_dim_.width() < 1) ||
      (requested_dim_.has_height() && requested_dim_.height() < 1)) {
    return false;
  }

  // Do not resize the image in these cases:
  //  - input is an animated image
  //  - requested dimension is larger than the original in either way.
  // By returning TRUE, the image can still have other optimizations.
  if (is_animated_ ||
      (requested_dim_.has_width() &&
       requested_dim_.width() > original_width_) ||
      (requested_dim_.has_height() &&
       requested_dim_.height() > original_height_)) {
    optimized_width_ = original_width_;
    optimized_height_ = original_height_;
    return true;
  }

  if (!requested_dim_.has_width() && !requested_dim_.has_height()) {
    optimized_width_ = original_width_;
    optimized_height_ = original_height_;
  } else if (!requested_dim_.has_width()) {
    optimized_height_ = requested_dim_.height();
    optimized_width_ =
        (optimized_height_ * original_width_ + original_height_ / 2) /
        original_height_;
  } else if (!requested_dim_.has_height()) {
    optimized_width_ = requested_dim_.width();
    optimized_height_ =
        (optimized_width_ * original_height_ + original_width_ / 2) /
        original_width_;
  } else {
    optimized_width_ = requested_dim_.width();
    optimized_height_ = requested_dim_.height();
  }

  return true;
}

bool ImageOptimizer::ComputeDesiredQualityProgressive() {
  // Determines quality level and whether to use progressive format.
  desired_progressive_ = false;
  int quality = original_quality_;
  if (quality == -1) {
    quality = 100;
  }
  if (optimized_format_ == IMAGE_JPEG) {
    quality = std::min(quality, options_.max_jpeg_quality());
    const int kMinJpegProgressiveBytes = 10240;
    desired_progressive_ =
        ShouldConvertToProgressive(
            quality, kMinJpegProgressiveBytes,
            original_contents_.length(), optimized_width_, optimized_height_);

  } else if (is_animated_) {
    quality = std::min(quality, options_.max_webp_animated_quality());
  } else {
    quality = std::min(quality, options_.max_webp_quality());
  }

  if (quality >= 0 && quality <= 100) {
    desired_quality_ = quality;
    return true;
  }
  return false;
}

// TODO(huibao): Unify ImageFormat and ImageType.
ImageFormat ImageOptimizer::ImageTypeToImageFormat(
    net_instaweb::ImageType image_type) {
  ImageFormat image_format = IMAGE_UNKNOWN;
  switch (image_type) {
    case net_instaweb::IMAGE_UNKNOWN:
      image_format = IMAGE_UNKNOWN;
      break;
    case net_instaweb::IMAGE_JPEG:
      image_format = IMAGE_JPEG;
      break;
    case net_instaweb::IMAGE_PNG:
      image_format = IMAGE_PNG;
      break;
    case net_instaweb::IMAGE_GIF:
      image_format = IMAGE_GIF;
      break;
    case net_instaweb::IMAGE_WEBP:
    case net_instaweb::IMAGE_WEBP_LOSSLESS_OR_ALPHA:
    case net_instaweb::IMAGE_WEBP_ANIMATED:
      image_format = IMAGE_WEBP;
      break;
  }
  return image_format;
}

// Returns a configuration for writing JPEG, PNG, or WebP image.
// Caller needs to cast the pointer back to the correct class.
bool ImageOptimizer::ConfigureWriter() {
  std::unique_ptr<PngCompressParams> png_config;
  std::unique_ptr<JpegCompressionOptions> jpeg_config;
  std::unique_ptr<WebpConfiguration> webp_config;

  bool result = false;
  switch (optimized_format_) {
    case IMAGE_UNKNOWN:
    case IMAGE_GIF:
      break;
    case IMAGE_PNG:
      png_config.reset(
          new PngCompressParams(
              options_.try_best_compression_for_png(),
              false /* never use progressive format */));
      writer_config_.reset(png_config.release());
      result = true;
      break;
    case IMAGE_JPEG:
      jpeg_config.reset(new JpegCompressionOptions);
      jpeg_config->retain_color_profile = false;
      jpeg_config->retain_exif_data = false;
      jpeg_config->lossy = true;
      jpeg_config->progressive = desired_progressive_;
      jpeg_config->lossy_options.quality = desired_quality_;
      writer_config_.reset(jpeg_config.release());
      result = true;
      break;
    case IMAGE_WEBP:
      webp_config.reset(new WebpConfiguration);
      // Quality/speed trade-off (0=fast, 6=slower-better).
      // This is the default value in libpagespeed. We should evaluate
      // whether this is the optimal value, and consider making it
      // tunable.
      webp_config->method = 3;
      webp_config->kmin = 3;
      webp_config->kmax = 5;
      webp_config->user_data = timeout_handler_.get();
      webp_config->progress_hook = ConversionTimeoutHandler::Continue;
      webp_config->lossless = desired_lossless_;

      // In the lossless mode, the "quality" aprameter does not affect the
      // visual quality of encoded image, however, it affects the number of
      // bytes which the encoded image has. For consistent output, we set it
      // to a constant value.
      webp_config->quality = (desired_lossless_ ? 100 : desired_quality_);

      if (is_transparent_) {
        webp_config->alpha_quality = 100;
        webp_config->alpha_compression = 1;
      } else {
        webp_config->alpha_quality = 0;
        webp_config->alpha_compression = 0;
      }
      writer_config_.reset(webp_config.release());
      result = true;
      break;
      // no default:
  }

  return result;
}

// Rewrites a single frame image. Optimizations to apply includes
// resizing dimensions, reducing color channels, and converting to better
// format.
bool ImageOptimizer::RewriteSingleFrameImage() {
  std::unique_ptr<ScanlineReaderInterface> reader(
      CreateScanlineReader(original_format_, original_contents_.data(),
                           original_contents_.length(), message_handler_));

  if (reader == nullptr) {
    PS_LOG_INFO(message_handler_, "Cannot open the image.");
    return false;
  }

  std::unique_ptr<PixelFormatOptimizer> optimizer(
      new PixelFormatOptimizer(message_handler_));
  if (!optimizer->Initialize(reader.release()).Success()) {
    return false;
  }

  bool need_resizing =
      (optimized_width_ < static_cast<int>(optimizer->GetImageWidth()) ||
       optimized_height_ < static_cast<int>(optimizer->GetImageHeight()));

  ScanlineReaderInterface* processor = nullptr;
  std::unique_ptr<ScanlineResizer> resizer;
  if (need_resizing) {
    resizer.reset(new ScanlineResizer(message_handler_));
    if (!resizer->Initialize(optimizer.get(), optimized_width_,
                             optimized_height_)) {
      return false;
    }
    processor = resizer.get();
  } else {
    processor = optimizer.get();
  }

  std::unique_ptr<ScanlineWriterInterface> writer(
      CreateScanlineWriter(optimized_format_, processor->GetPixelFormat(),
                           processor->GetImageWidth(),
                           processor->GetImageHeight(),
                           writer_config_.get(), optimized_contents_,
                           message_handler_));
  if (writer == nullptr) {
    PS_LOG_INFO(message_handler_,
                "Cannot create an image for output.");
    return false;
  }

  bool result = ImageConverter::ConvertImage(processor, writer.get());
  return result;
}

// Rewrite an animated image. Currently this is limited to converting
// an animated GIF image to animated WebP.
//
// TODO(huibao): Apply resizing and pixel format optimization to animated
// image.
bool ImageOptimizer::RewriteAnimatedImage() {
  ScanlineStatus status;
  std::unique_ptr<MultipleFrameReader>
      reader(
          CreateImageFrameReader(
              IMAGE_GIF,
              original_contents_.data(), original_contents_.length(),
              message_handler_, &status));
  if (!status.Success()) {
    PS_LOG_INFO(message_handler_, "Cannot read the animated GIF image.");
    return false;
  }

  std::unique_ptr<MultipleFrameWriter>
      writer(
          CreateImageFrameWriter(
              IMAGE_WEBP,
              writer_config_.get(), optimized_contents_, message_handler_,
              &status));
  if (!status.Success()) {
    PS_LOG_INFO(message_handler_,
                 "Cannot create an animated WebP image for output.");
    return false;
  }

  status =
      ImageConverter::ConvertMultipleFrameImage(reader.get(), writer.get());
  return status.Success();
}

bool ImageOptimizer::Run() {
  if (options_.max_timeout_ms() > 0 && timer_ != nullptr) {
    timeout_handler_.reset(
        new ConversionTimeoutHandler(options_.max_timeout_ms(), timer_,
                                     message_handler_));
    if (timeout_handler_ != nullptr) {
      timeout_handler_->Start(optimized_contents_);
    }
  } else {
    timeout_handler_.reset();
  }

  original_format_ =
      ImageTypeToImageFormat(ComputeImageType(original_contents_));
  if (original_format_ == IMAGE_UNKNOWN || original_format_ == IMAGE_WEBP) {
    return false;
  }

  if (!AnalyzeImage(original_format_, original_contents_.data(),
                    original_contents_.length(), &original_width_,
                    &original_height_, &is_progressive_, &is_animated_,
                    &is_transparent_, &is_photo_, &original_quality_, nullptr,
                    message_handler_)) {
    return false;
  }

  if (!ComputeDesiredFormat() ||
      !ComputeResizedDimension() ||
      !ComputeDesiredQualityProgressive() ||
      !ConfigureWriter()) {
    return false;
  }

  bool result = false;
  optimized_contents_->clear();
  if (is_animated_) {
    result = RewriteAnimatedImage();
  } else {
    result = RewriteSingleFrameImage();
  }

  // Stops timer and reports whether timeout happened.
  was_timed_out_ = false;
  if (timeout_handler_ != nullptr) {
    timeout_handler_->Stop();
    was_timed_out_ = timeout_handler_->was_timed_out();
  }

  if (result && options_.must_reduce_bytes() &&
      optimized_contents_->length() > original_contents_.length()) {
    result = false;
  }

  return result;
}

bool ImageOptimizer::Optimize(
    StringPiece original_contents, GoogleString* optimized_contents,
    ImageFormat* optimized_format) {
  // This method can only be called once.
  CHECK(is_valid_);
  is_valid_ = false;

  // All output buffers cannot be NULL.
  CHECK(optimized_contents != nullptr && optimized_format != nullptr);

  original_contents_ = original_contents;
  optimized_contents_ = optimized_contents;

  bool result = Run();
  if (result) {
    *optimized_format = optimized_format_;
  }
  return result;
}

}  // namespace image_compression

}  // namespace pagespeed
