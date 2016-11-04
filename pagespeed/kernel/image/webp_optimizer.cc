/*
 * Copyright 2009 Google Inc.
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

// Author: Victor Chudnovsky

#include "pagespeed/kernel/image/webp_optimizer.h"

#include <algorithm>
#include <cassert>
#include <cstdint>

#include "base/logging.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/image/scanline_utils.h"

extern "C" {
#ifdef USE_SYSTEM_LIBWEBP
#include "webp/decode.h"
#else
#include "third_party/libwebp/src/webp/decode.h"
#endif
}

namespace pagespeed {

namespace image_compression {

using image_compression::GetNumChannelsFromPixelFormat;
using net_instaweb::MessageHandler;

// Copied from libwebp/v0_2/examples/cwebp.c
static const char* const kWebPErrorMessages[] = {
  "OK",
  "OUT_OF_MEMORY: Out of memory allocating objects",
  "BITSTREAM_OUT_OF_MEMORY: Out of memory re-allocating byte buffer",
  "NULL_PARAMETER: NULL parameter passed to function",
  "INVALID_CONFIGURATION: configuration is invalid",
  "BAD_DIMENSION: Bad picture dimension. Maximum width and height "
  "allowed is 16383 pixels.",
  "PARTITION0_OVERFLOW: Partition #0 is too big to fit 512k.\n"
  "To reduce the size of this partition, try using less segments "
  "with the -segments option, and eventually reduce the number of "
  "header bits using -partition_limit. More details are available "
  "in the manual (`man cwebp`)",
  "PARTITION_OVERFLOW: Partition is too big to fit 16M",
  "BAD_WRITE: Picture writer returned an I/O error",
  "FILE_TOO_BIG: File would be too big to fit in 4G",
  "USER_ABORT: encoding abort requested by user"
};

// The libwebp error code returned in case of timeouts.
static const int kWebPErrorTimeout = VP8_ENC_ERROR_USER_ABORT;
const uint32_t kTransparentARGB = 0x00ffffff;


namespace {

// Fill a rectangular area within image with color. Caller must clip rectangle
// to within image.
void ImageFill(WebPPicture* image, size_px left, size_px top, size_px width,
               size_px height, uint32_t color) {
  uint32_t* row = image->argb + left + top * image->argb_stride;
  for (size_px row_number = 0; row_number < height; ++row_number) {
    std::fill(row, row + width, color);
    row += image->argb_stride;
  }
}

// Copy rectangular region of pixels from src to dst. Caller must ensure
// regions are within src and dst.
void BlitRect(const WebPPicture* src, WebPPicture* dst, int src_left,
              int src_top, int dst_left, int dst_top, int width, int height) {
  uint32_t* src_row = src->argb + src_left + src_top * src->argb_stride;
  uint32_t* dst_row = dst->argb + dst_left + dst_top * dst->argb_stride;
  for (int y = 0; y < height; ++y) {
    std::copy(src_row, src_row + width, dst_row);
    src_row += src->argb_stride;
    dst_row += dst->argb_stride;
  }
}

// Disposes the previous frame if necessary. Called prior to drawing frame.
bool DisposeImage(const FrameSpec* frame, const FrameSpec* previous_frame,
                  WebPPicture* image, WebPPicture** cache) {
  // Create or delete *cache.
  switch (frame->disposal) {
    case FrameSpec::DISPOSAL_RESTORE:
      // The current frame will need disposed by restoring the last non-disposed
      // frame. Cache it now.
      if (*cache == nullptr) {
        *cache = new WebPPicture;
        if (!WebPPictureInit(*cache) || !WebPPictureCopy(image, *cache)) {
          delete *cache;
          *cache = nullptr;
          return false;
        }
      }
      break;
    case FrameSpec::DISPOSAL_NONE:
      if (*cache != nullptr) {
        WebPPictureFree(*cache);
        *cache = nullptr;
      }
      break;
    default:
      break;
  }

  // Dispose previous frame.
  switch (previous_frame->disposal) {
    case FrameSpec::DISPOSAL_NONE:
      break;
    case FrameSpec::DISPOSAL_UNKNOWN:
    case FrameSpec::DISPOSAL_BACKGROUND:  // not supported
      ImageFill(image, previous_frame->left, previous_frame->top,
                previous_frame->width, previous_frame->height,
                kTransparentARGB);
      break;
    case FrameSpec::DISPOSAL_RESTORE:
      // On allocation failures, *cache may be nullptr.
      if (*cache == nullptr) return false;
      // Restore from the cached image.
      BlitRect(*cache, image, previous_frame->left, previous_frame->top,
               previous_frame->left, previous_frame->top, previous_frame->width,
               previous_frame->height);
      break;
  }
  return true;
}

// BlendChannel was copied from libwebp/examples/gif2webp.c
// Blend a single channel of 'src' over 'dst', given their alpha channel values.
uint8_t BlendChannel(uint32_t src, uint8_t src_a, uint32_t dst, uint8_t dst_a,
                     uint32_t scale, int shift) {
  const uint8_t src_channel = (src >> shift) & 0xff;
  const uint8_t dst_channel = (dst >> shift) & 0xff;
  const uint32_t blend_unscaled = src_channel * src_a + dst_channel * dst_a;
  assert(blend_unscaled < (1ULL << 32) / scale);
  return (blend_unscaled * scale) >> 24;
}

// BlendPixel was copied from libwebp/examples/gif2webp.c
// Blend 'src' over 'dst'.
uint32_t BlendPixel(uint32_t src, uint32_t dst) {
  const uint8_t src_a = (src >> 24) & 0xff;

  if (src_a == 255) {
    return src;
  } else if (src_a == 0) {
    return dst;
  } else {
    const uint8_t dst_a = (dst >> 24) & 0xff;
    // This is the approximate integer arithmetic for the actual formula:
    // dst_factor_a = (dst_a * (255 - src_a)) / 255.
    const uint8_t dst_factor_a = (dst_a * (256 - src_a)) >> 8;
    const uint8_t blend_a = src_a + dst_factor_a;
    const uint32_t scale = (1UL << 24) / blend_a;

    const uint8_t blend_r =
        BlendChannel(src, src_a, dst, dst_factor_a, scale, 0);
    const uint8_t blend_g =
        BlendChannel(src, src_a, dst, dst_factor_a, scale, 8);
    const uint8_t blend_b =
        BlendChannel(src, src_a, dst, dst_factor_a, scale, 16);
    assert(src_a + dst_factor_a < 256);

    return (blend_r << 0) | (blend_g << 8) | (blend_b << 16) |
           (static_cast<uint32_t>(blend_a) << 24);
  }
}

// WebPPicture.writer function that writes to a string.
int StringWriter(const uint8_t* data, size_t data_size,
                 const WebPPicture* picture) {
  GoogleString* output = static_cast<GoogleString*>(picture->custom_ptr);
  output->append(reinterpret_cast<const char*>(data), data_size);
  return 1;
}

}  // namespace

WebpConfiguration::~WebpConfiguration() {
}

void WebpConfiguration::CopyTo(WebPConfig* webp_config) const {
  webp_config->lossless = lossless;
  webp_config->quality = quality;
  webp_config->method = method;
  webp_config->target_size = target_size;
  webp_config->alpha_compression = alpha_compression;
  webp_config->alpha_filtering = alpha_filtering;
  webp_config->alpha_quality = alpha_quality;
}

WebpFrameWriter::WebpFrameWriter(MessageHandler* handler) :
    MultipleFrameWriter(handler), image_spec_(nullptr), next_frame_(0),
    next_scanline_(0), empty_frame_(false), frame_stride_px_(0),
    frame_position_px_(nullptr), frame_bytes_per_pixel_(0),
    webp_image_restore_(nullptr), webp_encoder_(nullptr),
    output_image_(nullptr), has_alpha_(false), image_prepared_(false),
    progress_hook_(nullptr), progress_hook_data_(nullptr) {
  WebPPictureInit(&webp_image_);
}

WebpFrameWriter::~WebpFrameWriter() {
  FreeWebpStructs();
}

void WebpFrameWriter::FreeWebpStructs() {
  WebPAnimEncoderDelete(webp_encoder_);
  webp_encoder_ = nullptr;

  WebPPictureFree(&webp_image_);

  WebPPictureFree(webp_image_restore_);
  delete webp_image_restore_;
  webp_image_restore_ = nullptr;
}


ScanlineStatus WebpFrameWriter::Initialize(const void* config,
                                           GoogleString* out) {
  FreeWebpStructs();
  if (config == nullptr) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler(),
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            FRAME_WEBPWRITER,
                            "missing WebpConfiguration*");
  }

  const WebpConfiguration* webp_config =
      static_cast<const WebpConfiguration*>(config);

  if (!WebPConfigInit(&libwebp_config_)) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            FRAME_WEBPWRITER, "WebPConfigInit()");
  }

  webp_config->CopyTo(&libwebp_config_);

  if (!WebPValidateConfig(&libwebp_config_)) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            FRAME_WEBPWRITER, "WebPValidateConfig()");
  }

  if (webp_config->progress_hook) {
    progress_hook_ = webp_config->progress_hook;
    progress_hook_data_ = webp_config->user_data;
  }

  kmin_ = webp_config->kmin;
  kmax_ = webp_config->kmax;

  output_image_ = out;

  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

int WebpFrameWriter::ProgressHook(int percent, const WebPPicture* picture) {
  const WebpFrameWriter* webp_writer =
      static_cast<WebpFrameWriter*>(picture->user_data);
  return webp_writer->progress_hook_(percent, webp_writer->progress_hook_data_);
}

ScanlineStatus WebpFrameWriter::PrepareImage(const ImageSpec* image_spec) {
  DVLOG(1) << image_spec->ToString();
  if (image_prepared_) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler(),
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            FRAME_WEBPWRITER, "image already prepared");
  }

  DVLOG(1) << "PrepareImage: num_frames: " << image_spec->num_frames;

  if ((image_spec->height > WEBP_MAX_DIMENSION) ||
      (image_spec->width > WEBP_MAX_DIMENSION)) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_UNSUPPORTED_FEATURE,
                            FRAME_WEBPWRITER,
                            "each image dimension must be at most %d",
                            WEBP_MAX_DIMENSION);
  }

  if ((image_spec->height < 1) || (image_spec->width < 1)) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_UNSUPPORTED_FEATURE,
                            FRAME_WEBPWRITER,
                            "each image dimension must be at least 1");
  }

  if (!WebPPictureInit(&webp_image_)) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            FRAME_WEBPWRITER, "WebPPictureInit()");
  }

  webp_image_.width = image_spec->width;
  webp_image_.height = image_spec->height;
  webp_image_.use_argb = true;
#ifndef NDEBUG
  memset(&stats_, 0, sizeof stats_);
  webp_image_.stats = &stats_;
#endif

  if (!WebPPictureAlloc(&webp_image_)) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            FRAME_WEBPWRITER, "WebPPictureAlloc()");
  }

  // Clear image with kTransparentARGB.
  ImageFill(&webp_image_, 0, 0, webp_image_.width, webp_image_.height,
            kTransparentARGB);

  webp_image_.user_data = this;
  if (progress_hook_) {
    webp_image_.progress_hook = ProgressHook;
  }

  image_spec_ = image_spec;
  next_frame_ = 0;
  image_prepared_ = true;
  timestamp_ = 0;
  next_scanline_ = 0;

  // For animated images, create the animated encoder.
  if (image_spec_->num_frames > 1) {
    WebPAnimEncoderOptions options;
    if (!WebPAnimEncoderOptionsInit(&options)) {
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                              SCANLINE_STATUS_INTERNAL_ERROR, FRAME_WEBPWRITER,
                              "WebPAnimEncoderOptionsInit()");
    }

    // Key frame parameters.
    if (kmin_ > 0) {
      if (kmin_ >= kmax_) {
        return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler(),
                          SCANLINE_STATUS_INVOCATION_ERROR,
                          FRAME_WEBPWRITER,
                          "Keyframe parameters error: kmin >= kmax");
      } else if (kmin_ < (kmax_ / 2 + 1)) {
        return PS_LOGGED_STATUS(
                          PS_LOG_DFATAL,
                          message_handler(),
                          SCANLINE_STATUS_INVOCATION_ERROR,
                          FRAME_WEBPWRITER,
                          "Keyframe parameters error: kmin < (kmax / 2 + 1)");
      } else {
        options.kmax = kmax_;
        options.kmin = kmin_;
      }
    } else {
      options.kmax = ~0;
      options.kmin = options.kmax - 1;
    }

    options.anim_params.bgcolor = RgbaToPackedArgb(image_spec_->bg_color);
    options.anim_params.loop_count =
        static_cast<int>(image_spec_->loop_count - 1);

    options.minimize_size = 0;
    options.allow_mixed = 0;
    webp_encoder_ =
        WebPAnimEncoderNew(image_spec->width, image_spec->height, &options);

    if (webp_encoder_ == nullptr) {
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                              SCANLINE_STATUS_MEMORY_ERROR, FRAME_WEBPWRITER,
                              "WebPAnimEncoderNew()");
    }
    frame_position_px_ = nullptr;
    frame_stride_px_ = 0;
  }

  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus WebpFrameWriter::CacheCurrentFrame() {
  // Not an animated image.
  if (image_spec_->num_frames <= 1) {
    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  }

  // If we're not even on the first frame, no-op.
  if (next_frame_ < 1) {
    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  }

  // Don't add empty frames.
  if (empty_frame_) {
    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  }

  // All scanlines must be written before caching a frame.
  if (next_scanline_ < frame_spec_.height) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler(),
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            FRAME_WEBPWRITER,
                            "CacheCurrentFrame: not all scanlines written");
  }

  if (progress_hook_) {
    CHECK(webp_image_.progress_hook == ProgressHook);
    CHECK(webp_image_.user_data == this);
  }
  const int current_time = timestamp_;
  timestamp_ += frame_spec_.duration_ms;
  if (!WebPAnimEncoderAdd(webp_encoder_, &webp_image_, current_time,
                          &libwebp_config_)) {
    if (webp_image_.error_code == kWebPErrorTimeout) {
      // This seems to never be reached.
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                              SCANLINE_STATUS_TIMEOUT_ERROR,
                              FRAME_WEBPWRITER,
                              "WebPFrameCacheAddFrame(): %s",
                              kWebPErrorMessages[webp_image_.error_code]);
    } else {
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                              SCANLINE_STATUS_INTERNAL_ERROR,
                              FRAME_WEBPWRITER,
                              "WebPFrameCacheAddFrame(): %s\n%s\n%s",
                              kWebPErrorMessages[webp_image_.error_code],
                              image_spec_->ToString().c_str(),
                              frame_spec_.ToString().c_str());
    }
  }

  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus WebpFrameWriter::PrepareNextFrame(const FrameSpec* frame_spec) {
  if (!image_prepared_) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler(),
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            FRAME_WEBPWRITER,
                            "PrepareNextFrame: image not prepared");
  }

  if (next_frame_ >= image_spec_->num_frames) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler(),
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            FRAME_WEBPWRITER,
                            "PrepareNextFrame: no next frame");
  }

  ScanlineStatus status = CacheCurrentFrame();
  if (!status.Success()) {
    return status;
  }

  // Bounds-check the frame.
  if (!image_spec_->CanContainFrame(*frame_spec)) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            FRAME_WEBPWRITER,
                            "PrepareNextFrame: frame does not fit in image:\n"
                            "%s\n%s",
                            image_spec_->ToString().c_str(),
                            frame_spec->ToString().c_str());
  }
  if (next_frame_ == 0) {
    previous_frame_spec_.width = image_spec_->width;
    previous_frame_spec_.height = image_spec_->height;
  } else {
    previous_frame_spec_ = frame_spec_;
  }
  ++next_frame_;

  frame_spec_ = *frame_spec;

  should_expand_gray_to_rgb_ = false;
  PixelFormat new_pixel_format = frame_spec_.pixel_format;
  switch (new_pixel_format) {
    case RGB_888:
      has_alpha_ = false;
      break;
    case RGBA_8888:
      has_alpha_ = true;
      break;
    case GRAY_8:
      // GRAY_8 will be expanded to RGB_888.
      has_alpha_ = false;
      should_expand_gray_to_rgb_ = true;
      new_pixel_format = RGB_888;
      break;
    default:
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                              SCANLINE_STATUS_INTERNAL_ERROR,
                              FRAME_WEBPWRITER,
                              "unknown pixel format: %d",
                              new_pixel_format);
  }
  DVLOG(1) << "Pixel format:" << GetPixelFormatString(frame_spec_.pixel_format);
  if (next_frame_ > 1) {
    if (!DisposeImage(&frame_spec_, &previous_frame_spec_, &webp_image_,
                      &webp_image_restore_)) {
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                              SCANLINE_STATUS_INTERNAL_ERROR, FRAME_WEBPWRITER,
                              "dispose image fail");
    }
  }

  empty_frame_ = (frame_spec_.width < 1) || (frame_spec_.height < 1);
  if (empty_frame_) {
    frame_stride_px_ = frame_spec_.width;
    frame_position_px_ = nullptr;
  } else {
    frame_stride_px_ = webp_image_.argb_stride;
    frame_position_px_ = webp_image_.argb + frame_spec_.left +
                         frame_spec_.top * webp_image_.argb_stride;
  }

  frame_bytes_per_pixel_ = GetBytesPerPixel(frame_spec_.pixel_format);
  next_scanline_ = 0;
  return status;
}

ScanlineStatus WebpFrameWriter::WriteNextScanline(const void *scanline_bytes) {
  if (next_scanline_ >= frame_spec_.height) {
      return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler(),
                              SCANLINE_STATUS_INVOCATION_ERROR,
                              FRAME_WEBPWRITER,
                              "WriteNextScanline: too many scanlines");
  }
  const uint8_t* const in_bytes =
      reinterpret_cast<const uint8_t*>(scanline_bytes);
  if (!empty_frame_) {
    if (should_expand_gray_to_rgb_) {
      // Replicate the luminance to RGB.
      for (size_t idx = 0; idx < frame_spec_.width; ++idx) {
        frame_position_px_[idx] = GrayscaleToPackedArgb(in_bytes[idx]);
      }
    } else if (has_alpha_) {
      // Note: this branch and the next only differ in the packing
      // function used. It is tempting to assign a function pointer
      // based on has_alpha_ and then implement the loop only
      // once. However, since this is an "inner loop" iterating over a
      // series of pixels, we want to take advantage of the inline
      // forms of the packing functions for speed.
      if (next_frame_ > 1) {
        for (size_t px_col = 0, byte_col = 0; px_col < frame_spec_.width;
             ++px_col, byte_col += frame_bytes_per_pixel_) {
          frame_position_px_[px_col] =
              BlendPixel(RgbaToPackedArgb(in_bytes + byte_col),
                         frame_position_px_[px_col]);
        }
      } else {
        for (size_t px_col = 0, byte_col = 0; px_col < frame_spec_.width;
             ++px_col, byte_col += frame_bytes_per_pixel_) {
          frame_position_px_[px_col] = RgbaToPackedArgb(in_bytes + byte_col);
        }
      }
    } else {
      for (size_t px_col = 0, byte_col = 0;
           px_col < frame_spec_.width;
           ++px_col, byte_col += frame_bytes_per_pixel_) {
        frame_position_px_[px_col] =
            RgbToPackedArgb(in_bytes + byte_col);
      }
    }
    frame_position_px_ += frame_stride_px_;
  }

  ++next_scanline_;
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus WebpFrameWriter::FinalizeWrite() {
  ScanlineStatus status = CacheCurrentFrame();
  if (!status.Success()) {
    return status;
  }
  if (image_spec_->num_frames <= 1) {
    webp_image_.writer = StringWriter;
    webp_image_.custom_ptr = static_cast<void*>(output_image_);
    if (WebPEncode(&libwebp_config_, &webp_image_) == 0) {
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                              SCANLINE_STATUS_INTERNAL_ERROR, FRAME_WEBPWRITER,
                              "WebPEncode error");
    }
    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  } else {
    if (WebPAnimEncoderAdd(webp_encoder_, nullptr, timestamp_, nullptr) == 0) {
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                              SCANLINE_STATUS_INTERNAL_ERROR, FRAME_WEBPWRITER,
                              "WebPAnimEncoderAdd error");
    }
    WebPData webp_data;
    if (WebPAnimEncoderAssemble(webp_encoder_, &webp_data) == 0) {
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                              SCANLINE_STATUS_INTERNAL_ERROR, FRAME_WEBPWRITER,
                              "WebPAnimEncoderAssemble error");
    }

    output_image_->append(reinterpret_cast<const char *>(webp_data.bytes),
                          webp_data.size);
    WebPDataClear(&webp_data);

    PS_DLOG_INFO(message_handler(), \
        "Stats: coded_size: %d; lossless_size: %d; alpha size: %d;",
        stats_.coded_size, stats_.lossless_size, stats_.alpha_data_size);

    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  }
}

WebpScanlineReader::WebpScanlineReader(MessageHandler* handler)
  : image_buffer_(nullptr),
    buffer_length_(0),
    pixel_format_(UNSUPPORTED),
    height_(0),
    width_(0),
    bytes_per_row_(0),
    row_(0),
    was_initialized_(false),
    message_handler_(handler) {
}

WebpScanlineReader::~WebpScanlineReader() {
  Reset();
}

bool WebpScanlineReader::Reset() {
  image_buffer_ = nullptr;
  buffer_length_ = 0;
  pixel_format_ = UNSUPPORTED;
  height_ = 0;
  width_ = 0;
  bytes_per_row_ = 0;
  row_ = 0;
  pixels_.reset();
  was_initialized_ = false;
  return true;
}

// Initialize the reader with the given image stream. Note that image_buffer
// must remain unchanged until the *first* call to ReadNextScanline().
ScanlineStatus WebpScanlineReader::InitializeWithStatus(
    const void* image_buffer,
    size_t buffer_length) {
  if (was_initialized_) {
    Reset();
  }

  WebPBitstreamFeatures features;
  if (WebPGetFeatures(reinterpret_cast<const uint8_t*>(image_buffer),
                      buffer_length, &features)
        != VP8_STATUS_OK) {
    return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                            SCANLINE_STATUS_PARSE_ERROR,
                            SCANLINE_WEBPREADER, "WebPGetFeatures()");
  }

  // TODO(huibao): Upgrade libwebp for open source and check if the
  // input is an animated WebP.

  // Determine the pixel format and the number of channels.
  if (features.has_alpha) {
    pixel_format_ = RGBA_8888;
  } else {
    pixel_format_ = RGB_888;
  }

  // Copy the information to the object properties.
  image_buffer_ = reinterpret_cast<const uint8_t*>(image_buffer);
  buffer_length_ = buffer_length;
  width_ = features.width;
  height_ = features.height;
  bytes_per_row_ = width_ * GetNumChannelsFromPixelFormat(pixel_format_,
                                                          message_handler_);
  row_ = 0;
  was_initialized_ = true;

  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus WebpScanlineReader::ReadNextScanlineWithStatus(
    void** out_scanline_bytes) {
  if (!was_initialized_ || !HasMoreScanLines()) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler_,
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            SCANLINE_WEBPREADER,
                            "The reader was not initialized or the image does "
                            "not have any more scanlines.");
  }

  // The first time ReadNextScanline() is called, we decode the entire image.
  if (row_ == 0) {
    pixels_.reset(new uint8_t[bytes_per_row_ * height_]);
    if (pixels_ == nullptr) {
      Reset();
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler_,
                              SCANLINE_STATUS_MEMORY_ERROR,
                              SCANLINE_WEBPREADER,
                              "Failed to allocate memory.");
    }

     WebPDecoderConfig config;
     CHECK(WebPInitDecoderConfig(&config));

     // Specify the desired output colorspace:
     if (pixel_format_ == RGB_888) {
       config.output.colorspace = MODE_RGB;
     } else {
       config.output.colorspace = MODE_RGBA;
     }

     // Have config.output point to an external buffer:
     config.output.u.RGBA.rgba = pixels_.get();
     config.output.u.RGBA.stride = bytes_per_row_;
     config.output.u.RGBA.size = bytes_per_row_ * height_;
     config.output.is_external_memory = true;

     bool decode_ok = (WebPDecode(image_buffer_, buffer_length_, &config)
                       == VP8_STATUS_OK);

     // Clean up WebP decoder because it is not needed any more,
     // regardless of whether whether decoding was successful or not.
     WebPFreeDecBuffer(&config.output);

     if (!decode_ok) {
       Reset();
       return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                               SCANLINE_STATUS_INTERNAL_ERROR,
                               SCANLINE_WEBPREADER,
                               "Failed to decode the WebP image.");
     }
  }

  // Point output to the corresponding row of the already decoded image.
  *out_scanline_bytes =
      static_cast<void*>(pixels_.get() + row_ * bytes_per_row_);

  ++row_;
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

}  // namespace image_compression

}  // namespace pagespeed
