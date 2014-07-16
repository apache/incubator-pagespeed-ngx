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
    MultipleFrameWriter(handler), image_spec_(NULL), next_frame_(0),
    next_scanline_(0), empty_frame_(false), frame_stride_px_(0),
    frame_position_px_(NULL), frame_bytes_per_pixel_(0), webp_image_(NULL),
    webp_frame_cache_(NULL), webp_mux_(NULL), output_image_(NULL),
    has_alpha_(false), image_prepared_(false), progress_hook_(NULL),
    progress_hook_data_(NULL) {
}

WebpFrameWriter::~WebpFrameWriter() {
  FreeWebpStructs();
}

void WebpFrameWriter::FreeWebpStructs() {
  // Shortcut the initial case, which will happen every time this
  // class is used.
  if ((webp_frame_cache_ == NULL) &&
      (webp_image_ == NULL) &&
      (webp_mux_ == NULL)) {
    return;
  }

  WebPFrameCacheDelete(webp_frame_cache_);
  webp_frame_cache_ = NULL;

  WebPPictureFree(webp_image_);
  delete webp_image_;
  webp_image_ = NULL;

  WebPMuxDelete(webp_mux_);
  webp_mux_ = NULL;
}


ScanlineStatus WebpFrameWriter::Initialize(const void* config,
                                           GoogleString* out) {
  FreeWebpStructs();
  webp_mux_ = WebPMuxNew();
  if (webp_mux_ == NULL) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            FRAME_WEBPWRITER,
                            "WebPMuxNew() failure");
  }

  if (config == NULL) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler(),
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            FRAME_WEBPWRITER,
                            "missing WebpConfiguration*");
  }

  const WebpConfiguration* webp_config =
      static_cast<const WebpConfiguration*>(config);

  if (!WebPConfigInit(&webp_config_)) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            FRAME_WEBPWRITER, "WebPConfigInit()");
  }

  webp_config->CopyTo(&webp_config_);

  if (!WebPValidateConfig(&webp_config_)) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            FRAME_WEBPWRITER, "WebPValidateConfig()");
  }

  if (webp_config->progress_hook) {
    progress_hook_ = webp_config->progress_hook;
    progress_hook_data_ = webp_config->user_data;
  }

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
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            FRAME_WEBPWRITER,
                            "image dimensions larger than the maximum of %d",
                            WEBP_MAX_DIMENSION);
  }

  if ((image_spec->height < 1) || (image_spec->width < 1)) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            FRAME_WEBPWRITER,
                            "each image dimension must be at least 1");
  }

  webp_image_ = new WebPPicture();

  if (!WebPPictureInit(webp_image_)) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            FRAME_WEBPWRITER, "WebPPictureInit()");
  }

  webp_image_->width = image_spec->width;
  webp_image_->height = image_spec->height;
  webp_image_->use_argb = true;
#ifndef NDEBUG
  webp_image_->stats = &stats_;
#endif

  if (!WebPPictureAlloc(webp_image_)) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            FRAME_WEBPWRITER, "WebPPictureAlloc()");
  }
  WebPUtilClearPic(webp_image_, NULL);

  webp_image_->user_data = this;
  if (progress_hook_) {
    webp_image_->progress_hook = ProgressHook;
  }

  image_spec_ = image_spec;
  next_frame_ = 0;
  image_prepared_ = true;

  // Key frame parameters: do not insert unnecessary key frames.
  static const size_t kMax = ~0;
  static const size_t kMin = kMax -1;
  webp_frame_cache_ = WebPFrameCacheNew(
      image_spec->width, image_spec->height, kMin, kMax,
      false /* don't allow mixing lossy and lossless frames */);

  frame_position_px_ = NULL;
  frame_stride_px_ = 0;
  next_scanline_ = 0;

  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

WebPMuxAnimDispose FrameDisposalToWebPDisposal(
    FrameSpec::DisposalMethod frame_disposal) {
  switch (frame_disposal) {
    case FrameSpec::DISPOSAL_UNKNOWN:
    case FrameSpec::DISPOSAL_NONE:
      return WEBP_MUX_DISPOSE_NONE;
    case FrameSpec::DISPOSAL_BACKGROUND:
    case FrameSpec::DISPOSAL_RESTORE:
      return WEBP_MUX_DISPOSE_BACKGROUND;
  }
  return WEBP_MUX_DISPOSE_NONE;
}

ScanlineStatus WebpFrameWriter::CacheCurrentFrame() {
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

  struct WebPMuxFrameInfo webp_frame_info;
  memset(&webp_frame_info, 0, sizeof(webp_frame_info));
  webp_frame_info.id = WEBP_CHUNK_ANMF;
  webp_frame_info.dispose_method =
      FrameDisposalToWebPDisposal(frame_spec_.disposal);
  webp_frame_info.blend_method = WEBP_MUX_BLEND;
  webp_frame_info.duration = frame_spec_.duration_ms;

  // We need to pass image to add frame.
  WebPFrameRect frame_rect = {
    static_cast<int>(frame_spec_.left),
    static_cast<int>(frame_spec_.top),
    static_cast<int>(frame_spec_.width),
    static_cast<int>(frame_spec_.height)
  };

  if (progress_hook_) {
    CHECK(webp_image_->progress_hook == ProgressHook);
    CHECK(webp_image_->user_data == this);
  }

  if (!WebPFrameCacheAddFrame(webp_frame_cache_, &webp_config_, &frame_rect,
                              webp_image_, &webp_frame_info)) {
    if (webp_image_->error_code == kWebPErrorTimeout) {
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                              SCANLINE_STATUS_TIMEOUT_ERROR,
                              FRAME_WEBPWRITER,
                              "WebPFrameCacheAddFrame(): %s",
                              kWebPErrorMessages[webp_image_->error_code]);
    } else {
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                              SCANLINE_STATUS_INTERNAL_ERROR,
                              FRAME_WEBPWRITER,
                              "WebPFrameCacheAddFrame(): %s\n%s\n%s",
                              kWebPErrorMessages[webp_image_->error_code],
                              image_spec_->ToString().c_str(),
                              frame_spec_.ToString().c_str());
    }
  }

  if (WebPFrameCacheFlush(webp_frame_cache_, false /*verbose*/, webp_mux_) !=
      WEBP_MUX_OK) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            FRAME_WEBPWRITER,
                            "WebPFrameCacheFlush() error");
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

  empty_frame_ = (frame_spec_.width < 1) || (frame_spec_.height < 1);
  if (empty_frame_) {
    frame_stride_px_ = frame_spec_.width;
    frame_position_px_ = NULL;
  } else {
    if (!WebPPictureView(webp_image_,
                         frame_spec_.left, frame_spec_.top,
                         frame_spec_.width, frame_spec_.height,
                         &webp_frame_)) {
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                              SCANLINE_STATUS_INTERNAL_ERROR,
                              FRAME_WEBPWRITER,
                              "WebPPictureView() failure: %s",
                              frame_spec_.ToString().c_str());
    }
    frame_stride_px_ = webp_frame_.argb_stride;
    frame_position_px_ = webp_frame_.argb;
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
  if (!empty_frame_) {
    if (should_expand_gray_to_rgb_) {
      // Replicate the luminance to RGB.
      const uint8_t* const in_bytes =
          reinterpret_cast<const uint8_t*>(scanline_bytes);
      for (int idx = 0; idx < webp_frame_.width; ++idx) {
        frame_position_px_[idx] = GrayscaleToPackedArgb(in_bytes[idx]);
      }
    } else if (has_alpha_) {
      // Note: this branch and the next only differ in the packing
      // function used. It is tempting to assign a function pointer
      // based on has_alpha_ and then implement the loop only
      // once. However, since this is an "inner loop" iterating over a
      // series of pixels, we want to take advantage of the inline
      // forms of the packing functions for speed.
      for (size_t px_col = 0, byte_col = 0;
           px_col < frame_spec_.width;
           ++px_col, byte_col += frame_bytes_per_pixel_) {
        frame_position_px_[px_col] =
            RgbaToPackedArgb(static_cast<const uint8_t*>(scanline_bytes) +
                             byte_col);
      }
    } else {
      for (size_t px_col = 0, byte_col = 0;
           px_col < frame_spec_.width;
           ++px_col, byte_col += frame_bytes_per_pixel_) {
        frame_position_px_[px_col] =
            RgbToPackedArgb(static_cast<const uint8_t*>(scanline_bytes) +
                            byte_col);
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

  if (WebPFrameCacheFlushAll(webp_frame_cache_, false /*verbose*/, webp_mux_) !=
      WEBP_MUX_OK) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            FRAME_WEBPWRITER,
                            "WebPFrameCacheFlushAll error");
  }

  if (next_frame_ > 1) {
    // This was an animated image.
    WebPMuxAnimParams anim = {
      RgbaToPackedArgb(image_spec_->bg_color),
      static_cast<int>(image_spec_->loop_count - 1)
    };
    if (WebPMuxSetAnimationParams(webp_mux_, &anim) != WEBP_MUX_OK) {
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                              SCANLINE_STATUS_INTERNAL_ERROR,
                              FRAME_WEBPWRITER,
                              "WebPMuxSetAnimationParams error");
    }
  }

  WebPData webp_data = { NULL, 0 };
  if (WebPMuxAssemble(webp_mux_, &webp_data) != WEBP_MUX_OK) {
    if (webp_image_->error_code == kWebPErrorTimeout) {
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                              SCANLINE_STATUS_TIMEOUT_ERROR,
                              FRAME_WEBPWRITER,
                              "WebPMuxAssemble: %s",
                              kWebPErrorMessages[webp_image_->error_code]);
    } else {
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler(),
                              SCANLINE_STATUS_INTERNAL_ERROR,
                              FRAME_WEBPWRITER,
                              "WebPMuxAssemble: %s",
                              kWebPErrorMessages[webp_image_->error_code]);
    }
  }

  output_image_->append(reinterpret_cast<const char *>(webp_data.bytes),
                        webp_data.size);
  WebPDataClear(&webp_data);

  PS_DLOG_INFO(message_handler(), \
      "Stats: coded_size: %d; lossless_size: %d; alpha size: %d;"
      " layer size: %d",
      stats_.coded_size, stats_.lossless_size, stats_.alpha_data_size,
      stats_.layer_data_size);

  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

WebpScanlineReader::WebpScanlineReader(MessageHandler* handler)
  : image_buffer_(NULL),
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
  image_buffer_ = NULL;
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
    if (pixels_ == NULL) {
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
