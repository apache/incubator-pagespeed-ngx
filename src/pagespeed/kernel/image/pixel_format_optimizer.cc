/*
 * Copyright 2013 Google Inc.
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

// Author: Huibao Lin

#include "pagespeed/kernel/image/pixel_format_optimizer.h"

#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/scanline_utils.h"

namespace pagespeed {

namespace {

using net_instaweb::MessageHandler;
const uint8_t OPAQUE_ALPHA = 0xFF;

}  // namespace

namespace image_compression {

PixelFormatOptimizer::PixelFormatOptimizer(MessageHandler* handler) :
    message_handler_(handler) {
  Reset();
}

PixelFormatOptimizer::~PixelFormatOptimizer() {
}

// Reset the scanline reader to its initial state.
bool PixelFormatOptimizer::Reset() {
  bytes_per_row_ = 0;
  pixel_format_ = UNSUPPORTED;
  output_row_ = 0;
  strip_alpha_ = false;
  was_initialized_ = false;
  input_lines_.reset();
  input_row_ = 0;
  output_line_.reset();
  return true;
}

ScanlineStatus PixelFormatOptimizer::InitializeWithStatus(
    const void* /* image_buffer */,
    size_t /* buffer_length */) {
  return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler_,
                          SCANLINE_STATUS_INVOCATION_ERROR,
                          SCANLINE_PIXEL_FORMAT_OPTIMIZER,
                          "Unexpected call to InitializeWithStatus()");
}

ScanlineStatus PixelFormatOptimizer::Initialize(
    ScanlineReaderInterface* reader) {
  Reset();
  reader_.reset(reader);

  if (reader == NULL ||
      reader->GetPixelFormat() == UNSUPPORTED ||
      reader->GetImageWidth() == 0 ||
      reader->GetImageHeight() == 0) {
    return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                            SCANLINE_STATUS_UNINITIALIZED,
                            SCANLINE_PIXEL_FORMAT_OPTIMIZER,
                            "Invalid input image.");
  }

  pixel_format_ = reader->GetPixelFormat();
  bytes_per_row_ = reader_->GetBytesPerScanline();

  // Only strip alpha for RGBA_8888 format.
  if (pixel_format_ != RGBA_8888) {
    strip_alpha_ = false;
    was_initialized_ = true;
    return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
  }

  const size_t image_width = reader_->GetImageWidth();
  const size_t image_height = reader_->GetImageHeight();

  // Now check if the alpha channel is opaque. To avoid decoding the image
  // twice, the decoded scanlines will be stored in 'input_lines_'.
  input_lines_.reset(new uint8_t[image_height * bytes_per_row_]);
  uint8_t* current_scanline = input_lines_.get();
  const size_t num_channels =
      GetNumChannelsFromPixelFormat(pixel_format_, message_handler_);

  input_row_ = 0;
  while (input_row_ < image_height) {
    void* in_scanline = NULL;
    ScanlineStatus status = reader_->ReadNextScanlineWithStatus(&in_scanline);
    if (!status.Success()) {
      Reset();
      return status;
    }

    // Buffer the scanline.
    memcpy(current_scanline, in_scanline, bytes_per_row_);
    ++input_row_;

    // Check if the current scanline is opaque or not. Alpha is the last
    // channel.
    for (size_t ch = num_channels - 1;
         ch < image_width * num_channels;
         ch += num_channels) {
      if (current_scanline[ch] != OPAQUE_ALPHA) {
        strip_alpha_ = false;
        was_initialized_ = true;
        return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
      }
    }
    current_scanline += bytes_per_row_;
  }

  // Now we know that the alpha channel is opaque. We will modify the pixel
  // format and allocate memory for the stripped scanlines.
  strip_alpha_ = true;
  pixel_format_ = RGB_888;
  bytes_per_row_ = image_width *
      GetNumChannelsFromPixelFormat(pixel_format_, message_handler_);
  output_line_.reset(new uint8_t[bytes_per_row_]);
  was_initialized_ = true;
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

// Reads the next available scanline.
ScanlineStatus PixelFormatOptimizer::ReadNextScanlineWithStatus(
    void** out_scanline_bytes) {
  if (!was_initialized_) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler_,
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            SCANLINE_PIXEL_FORMAT_OPTIMIZER,
                            "Uninitialized");
  }

  if (!HasMoreScanLines()) {
    return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            SCANLINE_PIXEL_FORMAT_OPTIMIZER,
                            "No more scanlines");
  }

  const int bytes_per_in_pixel = GetNumChannelsFromPixelFormat(RGBA_8888,
      message_handler_);
  const int bytes_per_out_pixel = GetNumChannelsFromPixelFormat(RGB_888,
      message_handler_);

  if (strip_alpha_) {
    // If we have decided to strip the alpha channel, the entire input image
    // should have already been copied to 'input_lines_'. We will grab the
    // corresponding line in 'input_lines_', filter the alpha, and store the
    // results in the 'output_line_'.
    uint8_t* in_pixel = input_lines_.get() +
        output_row_ * reader_->GetBytesPerScanline();
    uint8_t* out_pixel = output_line_.get();

    const size_t image_width = reader_->GetImageWidth();
    for (size_t pixel = 0; pixel < image_width; ++pixel) {
      memcpy(out_pixel, in_pixel, bytes_per_out_pixel);
      in_pixel += bytes_per_in_pixel;
      out_pixel += bytes_per_out_pixel;
    }
    *out_scanline_bytes = output_line_.get();
  } else {
    // If we have decided NOT to strip the alpha channel, we may have decoded
    // a portion of the input image. We will grab the decoded lines from
    // 'input_lines_', and then decode the rest of the image.
    if (output_row_ < input_row_) {
      *out_scanline_bytes = input_lines_.get()
          + output_row_ * reader_->GetBytesPerScanline();
    } else {
      if (!reader_->ReadNextScanline(out_scanline_bytes)) {
        Reset();
        return PS_LOGGED_STATUS(PS_LOG_INFO, message_handler_,
                                SCANLINE_STATUS_INTERNAL_ERROR,
                                SCANLINE_PIXEL_FORMAT_OPTIMIZER,
                                "Failed to read a scanline.");
      }
    }
  }
  ++output_row_;
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

}  // namespace image_compression

}  // namespace pagespeed
