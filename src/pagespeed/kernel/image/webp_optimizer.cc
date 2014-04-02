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

#include <cstdlib>

#include "base/logging.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/image/scanline_utils.h"

extern "C" {
#ifdef USE_SYSTEM_LIBWEBP
#include "webp/decode.h"
#else
#include "third_party/libwebp/webp/decode.h"
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

// Byte-emission function for use via WebPPicture.writer.
int WriteWebpIncrementally(const uint8_t* data, size_t data_size,
                           const WebPPicture* const pic) {
  GoogleString* const out = static_cast<GoogleString*>(pic->custom_ptr);
  out->append(reinterpret_cast<const char*>(data), data_size);

  PS_DLOG_INFO( \
      static_cast<WebpScanlineWriter*>(pic->user_data)->message_handler(), \
      "Writing to webp: %d bytes. Total size: %d", \
      static_cast<int>(data_size), static_cast<int>(out->size()));

  return true;
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

WebpScanlineWriter::WebpScanlineWriter(MessageHandler* handler)
    : stride_bytes_(0), rgb_(NULL), rgb_end_(NULL), position_bytes_(NULL),
      config_(NULL), webp_image_(NULL), has_alpha_(false),
      init_ok_(false), imported_(false), got_all_scanlines_(false),
      progress_hook_(NULL), progress_hook_data_(NULL),
      message_handler_(handler) {
}

WebpScanlineWriter::~WebpScanlineWriter() {
  if (imported_) {
    WebPPictureFree(&picture_);
  }
  delete config_;
  free(rgb_);
}

ScanlineStatus WebpScanlineWriter::InitializeWriteWithStatus(
    const void* const params,
    GoogleString* const out) {

  if (params == NULL) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler_,
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            SCANLINE_WEBPWRITER,
                            "missing WebpConfiguration*");
  }
  const WebpConfiguration* webp_config =
      static_cast<const WebpConfiguration*>(params);

  if (!init_ok_) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler_,
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            SCANLINE_WEBPWRITER,
                            "prior initialization failure");
  }

  // Since config_ might have been modified during a previous call to
  // FinalizeWrite() and can't be re-used, create a fresh copy.
  delete config_;
  config_ = new WebPConfig();
  if (!WebPConfigInit(config_)) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler_,
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            SCANLINE_WEBPWRITER, "WebPConfigInit()");
  }

  webp_config->CopyTo(config_);

  if (!WebPValidateConfig(config_)) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler_,
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            SCANLINE_WEBPWRITER, "WebPValidateConfig()");
  }

  webp_image_ = out;
  if (webp_config->progress_hook) {
    progress_hook_ = webp_config->progress_hook;
    progress_hook_data_ = webp_config->user_data;
  }
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

int WebpScanlineWriter::ProgressHook(int percent, const WebPPicture* picture) {
  const WebpScanlineWriter* webp_writer =
      static_cast<WebpScanlineWriter*>(picture->user_data);
  return webp_writer->progress_hook_(percent, webp_writer->progress_hook_data_);
}

ScanlineStatus WebpScanlineWriter::InitWithStatus(const size_t width,
                                                  const size_t height,
                                                  PixelFormat pixel_format) {
  if (init_ok_) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler_,
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            SCANLINE_WEBPWRITER, "already initialized");
  }

  if ((height > WEBP_MAX_DIMENSION) ||
      (width > WEBP_MAX_DIMENSION)) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler_,
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            SCANLINE_WEBPWRITER,
                            "image dimensions larger than the maximum of %d",
                            WEBP_MAX_DIMENSION);
  }

  should_expand_gray_to_rgb_ = false;
  PixelFormat new_pixel_format = pixel_format;
  switch (pixel_format) {
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
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler_,
                              SCANLINE_STATUS_UNSUPPORTED_FEATURE,
                              SCANLINE_WEBPWRITER,
                              "unhandled or unknown pixel format: %d",
                              pixel_format);
  }
  PS_DLOG_INFO(message_handler_, "Pixel format: %s", \
      GetPixelFormatString(pixel_format));
  if (!WebPPictureInit(&picture_)) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler_,
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            SCANLINE_WEBPWRITER, "WebPPictureInit()");
  }

  picture_.width = width;
  picture_.height = height;
  picture_.use_argb = true;
#ifndef NDEBUG
  picture_.stats = &stats_;
#endif

  COMPILE_ASSERT(sizeof(*rgb_) == 1, Expected_size_of_one_byte);
  stride_bytes_ = picture_.width * sizeof(*rgb_) *
      GetNumChannelsFromPixelFormat(new_pixel_format, message_handler_);

  int size_bytes = stride_bytes_ * picture_.height;
  if (rgb_ != NULL) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler_,
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            SCANLINE_WEBPWRITER,
                            "rgb_ previously initialized");
  }
  if ((rgb_ = static_cast<uint8_t*>(malloc(size_bytes))) == NULL) {
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler_,
                            SCANLINE_STATUS_MEMORY_ERROR,
                            SCANLINE_WEBPWRITER,
                            "malloc()");
  }
  rgb_end_ = rgb_ + size_bytes;
  position_bytes_ = rgb_;
  init_ok_ = true;
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus WebpScanlineWriter::WriteNextScanlineWithStatus(
    const void* const scanline_bytes) {
  if ((position_bytes_ == NULL) ||
      (position_bytes_ + stride_bytes_ > rgb_end_)) {
    PS_DLOG_INFO(message_handler_, \
        "Attempting to write past allocated memory "
        "(rgb_ == %p; position_bytes_ == %p; stride_bytes_ == %d; "
        "rgb_end_ == %p)",
        static_cast<void*>(rgb_), static_cast<void*>(position_bytes_),
        stride_bytes_, static_cast<void*>(rgb_end_));
    return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler_,
                            SCANLINE_STATUS_INTERNAL_ERROR,
                            SCANLINE_WEBPWRITER,
                            "attempting to write past allocated memory");
  }

  const int kNumRgbChannels = 3;
  if (should_expand_gray_to_rgb_) {
    // Replicate the luminance to RGB.
    const uint8_t* const in_bytes =
        reinterpret_cast<const uint8_t*>(scanline_bytes);
    for (int idx_in = 0, idx_out = 0;
         idx_in < picture_.width;
         ++idx_in, idx_out += kNumRgbChannels) {
      memset(position_bytes_+idx_out, in_bytes[idx_in], kNumRgbChannels);
    }
  } else {
    memcpy(position_bytes_, scanline_bytes, stride_bytes_);
  }
  position_bytes_ += stride_bytes_;
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus WebpScanlineWriter::FinalizeWriteWithStatus() {
  if (!got_all_scanlines_) {
    if (position_bytes_ != rgb_end_) {
      return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler_,
                              SCANLINE_STATUS_INVOCATION_ERROR,
                              SCANLINE_WEBPWRITER, "unwritten scanlines");
    }
    got_all_scanlines_ = true;
    position_bytes_ = NULL;
  }

  bool ok = has_alpha_ ?
      (WebPPictureImportRGBA(&picture_, rgb_, stride_bytes_) != 0) :
      (WebPPictureImportRGB(&picture_, rgb_, stride_bytes_) != 0);

  if (!ok) {
    if (has_alpha_) {
      return PS_LOGGED_STATUS(PS_DLOG_ERROR, message_handler_,
                              SCANLINE_STATUS_INTERNAL_ERROR,
                              SCANLINE_WEBPWRITER,
                              "WebPPictureImportRGBA()");
    } else {
      return PS_LOGGED_STATUS(PS_DLOG_ERROR, message_handler_,
                              SCANLINE_STATUS_INTERNAL_ERROR,
                              SCANLINE_WEBPWRITER,
                              "WebPPictureImportRGB()");
    }
  }

  imported_ = true;

  picture_.writer = WriteWebpIncrementally;
  picture_.custom_ptr = webp_image_;
  picture_.user_data = this;
  if (progress_hook_) {
    picture_.progress_hook = ProgressHook;
  }
  if (!WebPEncode(config_, &picture_)) {
    if (picture_.error_code == kWebPErrorTimeout) {
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler_,
                              SCANLINE_STATUS_TIMEOUT_ERROR,
                              SCANLINE_WEBPWRITER,
                              "WebPEncode(): %s",
                              kWebPErrorMessages[picture_.error_code]);
    } else {
      return PS_LOGGED_STATUS(PS_LOG_ERROR, message_handler_,
                              SCANLINE_STATUS_INTERNAL_ERROR,
                              SCANLINE_WEBPWRITER,
                              "WebPEncode(): %s",
                              kWebPErrorMessages[picture_.error_code]);
    }
  }

  PS_DLOG_INFO(message_handler_, \
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
      return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
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

     // Clean up WebP decoder because it is not needed any more no matter
     // whether decoding was successful or not.
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
