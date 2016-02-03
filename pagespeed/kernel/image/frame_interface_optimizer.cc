/*
 * Copyright 2014 Google Inc.
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

#include "pagespeed/kernel/image/frame_interface_optimizer.h"

#include <cstdint>

#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"

namespace pagespeed {

namespace image_compression {

// Takes ownership of reader.
MultipleFramePaddingReader::MultipleFramePaddingReader(
    MultipleFrameReader* reader) :
    MultipleFrameReader(reader->message_handler()),
    impl_(reader) {
}

MultipleFramePaddingReader::~MultipleFramePaddingReader() {}

ScanlineStatus MultipleFramePaddingReader::Reset() {
  return impl_->Reset();
}

ScanlineStatus MultipleFramePaddingReader::Initialize() {
  ScanlineStatus status = impl_->Initialize(image_buffer_, buffer_length_);
  if (status.Success()) {
    status = impl_->GetImageSpec(&image_spec_);
  }
  return status;
}

bool MultipleFramePaddingReader::HasMoreFrames() const {
  return impl_->HasMoreFrames();
}

bool MultipleFramePaddingReader::HasMoreScanlines() const {
  return current_scanline_idx_ < padded_frame_spec_.height;
}

ScanlineStatus MultipleFramePaddingReader::PrepareNextFrame() {
  frame_needs_no_padding_ = false;
  frame_is_full_height_ = false;
  frame_is_full_width_ = false;

  // If image_spec_.use_bg_color == false, then we pad the frame with
  // the transparent color defined in kTransparent.
  static const PixelRgbaChannels kTransparent = {0, 0, 0, kAlphaTransparent};

  ScanlineStatus status;
  if (impl_->PrepareNextFrame(&status) &&
      impl_->GetFrameSpec(&impl_frame_spec_, &status)) {
    // Bounds-check the FrameSpec.
    impl_frame_spec_.left = image_spec_.TruncateXIndex(impl_frame_spec_.left);
    impl_frame_spec_.width =
        image_spec_.TruncateXIndex(impl_frame_spec_.left +
                                   impl_frame_spec_.width) -
        impl_frame_spec_.left;

    padded_frame_spec_ = impl_frame_spec_;
    padded_frame_spec_.width = image_spec_.width;
    padded_frame_spec_.height = image_spec_.height;
    padded_frame_spec_.top = 0;
    padded_frame_spec_.left = 0;

    bytes_per_pixel_ = GetBytesPerPixel(padded_frame_spec_.pixel_format);
    size_px scanline_num_bytes = padded_frame_spec_.width * bytes_per_pixel_;

    current_scanline_.reset(new uint8_t[scanline_num_bytes]);
    scanline_template_.reset(new uint8_t[scanline_num_bytes]);

    uint8_t* template_ptr_end = scanline_template_.get() + scanline_num_bytes;
    const void* bg_color = (image_spec_.use_bg_color ?
                            image_spec_.bg_color : kTransparent);
    for (uint8_t* template_ptr = scanline_template_.get();
         template_ptr < template_ptr_end;
         template_ptr += bytes_per_pixel_) {
      memcpy(template_ptr, bg_color, bytes_per_pixel_);
    }

    current_scanline_idx_ = 0;
    // These are guaranteed to be in range because impl_frame_spec_
    // was itself bounds-checked above.
    size_px foreground_scanline_start_idx = impl_frame_spec_.left;
    size_px foreground_scanline_end_idx = (impl_frame_spec_.left +
                                           impl_frame_spec_.width);
    foreground_scanline_start_byte_ =
        (current_scanline_.get() +
         bytes_per_pixel_ * foreground_scanline_start_idx);

    frame_is_full_width_ = ((foreground_scanline_start_idx == 0) &&
                            (foreground_scanline_end_idx == image_spec_.width));
    frame_is_full_height_ = ((impl_frame_spec_.top == 0) &&
                             (impl_frame_spec_.height == image_spec_.height));
    frame_needs_no_padding_ = frame_is_full_width_ && frame_is_full_height_;

    // Set the background color for all the scanlines to follow. Note
    // that since the foreground is rectangular, the same foreground
    // pixels will get overwritten in each scanline, while the
    // background pixels remain untouched.
    memcpy(current_scanline_.get(),
           scanline_template_.get(),
           scanline_num_bytes);
  }
  return status;
}

ScanlineStatus MultipleFramePaddingReader::ReadNextScanline(
    const void** out_scanline_bytes) {
  if (frame_needs_no_padding_) {
    // Short-circuit any additional computations.
    ++current_scanline_idx_;
    return impl_->ReadNextScanline(out_scanline_bytes);
  }

  if (!HasMoreScanlines()) {
    return PS_LOGGED_STATUS(PS_LOG_DFATAL, message_handler(),
                            SCANLINE_STATUS_INVOCATION_ERROR,
                            FRAME_PADDING_READER,
                            "no more scanlines in the current frame");
  }

  const void* impl_scanline = NULL;
  ScanlineStatus status;

  if (frame_is_full_height_ ||
      ((current_scanline_idx_ >= impl_frame_spec_.top) &&
       (current_scanline_idx_ < (impl_frame_spec_.top +
                                 impl_frame_spec_.height)))) {
    // This scanline contains foreground pixels.

    // If a full-width row, we can short-circuit the remaining
    // computations.
    if (frame_is_full_width_) {
      ++current_scanline_idx_;
      return impl_->ReadNextScanline(out_scanline_bytes);
    }

    // Read the foreground row for use below.
    if (!impl_->ReadNextScanline(&impl_scanline, &status)) {
      return status;
    }
  }

  if (impl_scanline == NULL) {
    // This scanline contains only background pixels.
    *out_scanline_bytes = scanline_template_.get();
  } else {
    // Overwrite the foreground pixels appropriately. Note that the
    // background pixels were already set in PrepareNextFrame.
    memcpy(foreground_scanline_start_byte_,
           impl_scanline,
           bytes_per_pixel_ * impl_frame_spec_.width);
    *out_scanline_bytes = current_scanline_.get();
  }

  ++current_scanline_idx_;
  return status;
}

ScanlineStatus MultipleFramePaddingReader::GetFrameSpec(
    FrameSpec* frame_spec) const {
  *frame_spec = padded_frame_spec_;
  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus MultipleFramePaddingReader::GetImageSpec(
    ImageSpec* image_spec) const {
  ScanlineStatus status = impl_->GetImageSpec(image_spec);
  if (status.Success() && !image_spec->Equals(image_spec_)) {
    return ScanlineStatus(SCANLINE_STATUS_INTERNAL_ERROR,
                          FRAME_PADDING_READER,
                          "ImageSpec changed during image processing");
  }
  return status;
}

MessageHandler* MultipleFramePaddingReader::message_handler() const {
  return impl_->message_handler();
}

ScanlineStatus MultipleFramePaddingReader::set_quirks_mode(
    QuirksMode quirks_mode) {
  return impl_->set_quirks_mode(quirks_mode);
}

QuirksMode MultipleFramePaddingReader::quirks_mode() const {
  return impl_->quirks_mode();
}


}  // namespace image_compression

}  // namespace pagespeed
