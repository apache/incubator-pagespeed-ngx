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

#ifndef PAGESPEED_KERNEL_IMAGE_FRAME_INTERFACE_OPTIMIZER_H_
#define PAGESPEED_KERNEL_IMAGE_FRAME_INTERFACE_OPTIMIZER_H_

#include <cstddef>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/image/image_frame_interface.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/image/scanline_status.h"

namespace net_instaweb {
class MessageHandler;
}

namespace pagespeed {

namespace image_compression {

// This class is an adapter that makes the frame size be the same as
// the image size, padding the frame with background color pixels.
class MultipleFramePaddingReader : public MultipleFrameReader {
 public:
  // Takes ownership of reader.
  explicit MultipleFramePaddingReader(MultipleFrameReader* reader);
  virtual ~MultipleFramePaddingReader();

  virtual ScanlineStatus Reset();
  virtual ScanlineStatus Initialize();
  virtual bool HasMoreFrames() const;
  virtual bool HasMoreScanlines() const;
  virtual ScanlineStatus PrepareNextFrame();
  virtual ScanlineStatus ReadNextScanline(const void** out_scanline_bytes);
  virtual ScanlineStatus GetFrameSpec(FrameSpec* frame_spec) const;
  virtual ScanlineStatus GetImageSpec(ImageSpec* image_spec) const;
  MessageHandler* message_handler() const;
  virtual ScanlineStatus set_quirks_mode(QuirksMode quirks_mode);
  virtual QuirksMode quirks_mode() const;

 private:
  net_instaweb::scoped_ptr<MultipleFrameReader> impl_;

  // The ImageSpec as fetched from impl_.
  ImageSpec image_spec_;

  // The FrameSpec returned by impl_ for the current frame.
  FrameSpec impl_frame_spec_;

  // The padded FrameSpec we return for the current frame. Its
  // dimensions are those of the image.
  FrameSpec padded_frame_spec_;

  // Whether the frame is as tall as the image.
  bool frame_is_full_height_;

  // Whether the frame is as wide as the image.
  bool frame_is_full_width_;

  // Whether the frame has exactly the same dimensions as the
  // image. This is simply and frame_is_full_width_ &&
  // frame_is_full_height_, and is used to shortcut the expensive
  // operations in ReadNextScanline.
  bool frame_needs_no_padding_;

  // The index of the current scanline being read in the current
  // (padded) frame.
  size_px current_scanline_idx_;

  // The current scanline being read in the current (padded) frame.
  net_instaweb::scoped_array<uint8_t> current_scanline_;

  // A template scanline consisting of purely the padding background
  // color. We copy this to current_scanline_ and overwrite the
  // appropriate locations with the contents of the non-padded frame.
  net_instaweb::scoped_array<uint8_t> scanline_template_;

  // The number of bytes per pixel in the current frame.
  size_t bytes_per_pixel_;

  // Pointer to the byte in current_scanline_ that marks the start
  // location of where the non-padded frame will be copied, in rows
  // which contain the frame.
  uint8_t* foreground_scanline_start_byte_;

  DISALLOW_COPY_AND_ASSIGN(MultipleFramePaddingReader);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_FRAME_INTERFACE_OPTIMIZER_H_
