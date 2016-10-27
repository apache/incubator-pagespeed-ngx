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

#ifndef PAGESPEED_KERNEL_IMAGE_IMAGE_OPTIMIZER_H_
#define PAGESPEED_KERNEL_IMAGE_IMAGE_OPTIMIZER_H_

#include <memory>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/image_types.pb.h"
#include "pagespeed/kernel/image/image_optimizer.pb.h"
#include "pagespeed/kernel/image/image_util.h"

namespace net_instaweb {
class Timer;
}

namespace pagespeed {

namespace image_compression {

// Optimizes an image. The supported formats include GIF (both single-frame
// and animated), PNG, and JPEG. They can be converted to PNG, JPEG, or WebP
// (including lossy, lossless, or animated WebP). They can be resized to
// smaller dimensions. You specify the allowed options and the requested
// dimensions, and the method returns the format, dimensions, and contents
// for the actual output.
//
// If you specify a timer and the max_timeout_ms parameter, the method
// applies them to WebP images (other images are not affected by these
// parameters).
//
// This class can only be used to optimize one image, i.e., an ImageOptimizer
// object can only have the Optimize method called once.
class ImageOptimizer {
 public:
  explicit ImageOptimizer(net_instaweb::MessageHandler* message_handler) :
      message_handler_(message_handler) {
  }

  // Applies all optimizations, for example, removing metadata, reducing chroma
  // sampling, and reducing dimension, to the image.
  //
  // TODO(huibao): Instead of buffering the optimized image contents in a string
  // let the Optimize method take a writer and stream the contents to the output
  // directly.
  bool Optimize(StringPiece original_contents,
                GoogleString* optimized_contents,
                ImageFormat* optimized_format);

  void set_options(const pagespeed::image_compression::ImageOptions& options) {
    options_ = options;
  }

  // Specifies the dimensions for resizing the image to. This parameter only
  // applies to single-frame images. You can specify either width or height,
  // or both. If you specify only one dimension, the image will be resized
  // with the original aspect ratio. If the dimensions cannot be honored,
  // the image can still be optimized in other ways. You can get the actual
  // dimension of the optimized image by using the OutputDimension method.
  void set_requested_dimension(
      const pagespeed::image_compression::ImageDimensions&
      requested_dimensions) {
    requested_dim_ = requested_dimensions;
  }

  // Returns the actual dimensions of the optimized image, even when the image
  // is not resized.
  int optimized_width() { return optimized_width_; }
  int optimized_height() { return optimized_height_; }

  // Timer and was_timed_out only apply to WebP images.
  void set_timer(net_instaweb::Timer* timer) { timer_ = timer; }
  bool was_timed_out() const { return was_timed_out_; }

  // Returns whether the image was encoded in lossy format, if the optimization
  // succeeded.
  bool UsesLossyFormat() const { return !desired_lossless_; }

 private:
  bool Run();
  bool ComputeDesiredFormat();
  bool ComputeResizedDimension();
  bool ComputeDesiredQualityProgressive();
  ImageFormat ImageTypeToImageFormat(net_instaweb::ImageType image_type);
  bool ConfigureWriter();
  bool RewriteSingleFrameImage();
  bool RewriteAnimatedImage();

  // External data.
  net_instaweb::MessageHandler* const message_handler_;
  pagespeed::image_compression::ImageOptions options_;
  StringPiece original_contents_;
  pagespeed::image_compression::ImageDimensions requested_dim_;
  GoogleString* optimized_contents_ = nullptr;
  net_instaweb::Timer* timer_ = nullptr;
  std::unique_ptr<ConversionTimeoutHandler> timeout_handler_;
  bool was_timed_out_ = false;

  // Information about input image.
  ImageFormat original_format_;
  int original_width_ = -1;
  int original_height_ = -1;
  bool is_progressive_ = false;
  bool is_animated_ = false;
  bool is_transparent_ = false;
  bool is_photo_ = false;
  int original_quality_ = -1;

  // Information about desired output.
  ImageFormat optimized_format_;
  int optimized_width_ = -1;
  int optimized_height_ = -1;
  int desired_quality_ = -1;
  bool desired_progressive_ = false;
  bool desired_lossless_ = false;
  std::unique_ptr<ScanlineWriterConfig> writer_config_;
  bool is_valid_ = true;

  DISALLOW_COPY_AND_ASSIGN(ImageOptimizer);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_IMAGE_OPTIMIZER_H__
