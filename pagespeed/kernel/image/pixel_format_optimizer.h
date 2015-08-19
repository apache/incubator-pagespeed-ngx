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

#ifndef PAGESPEED_KERNEL_IMAGE_PIXEL_FORMAT_OPTIMIZER_H_
#define PAGESPEED_KERNEL_IMAGE_PIXEL_FORMAT_OPTIMIZER_H_

#include <cstddef>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/image/scanline_interface.h"
#include "pagespeed/kernel/image/scanline_status.h"

namespace pagespeed {

namespace image_compression {
// PixelFormatOptimizer removes an unused channel from the image. This
// corresponds to changing the pixel format to a more compact one.
// Currently it only removes opaque alpha channel and changes RGBA_8888
// to RGB_888.
//
// To determine if a channel is unused, PixelFormatOptimizer has to examine
// every pixel in the image. Thus, the entire image may be buffered before
// the first output scanline can be retrieved. However, as soon as
// PixelFormatOptimizer finds a pixel with all channels used, it will stop
// buffering and become ready to serve the first scanline.
//
// TODO(huibao): Check how often gray scale images are encoded as color. If it
// happens often, implement the conversion of RGBA_8888/RGB_888 to GRAY_8.
class PixelFormatOptimizer : public ScanlineReaderInterface {
 public:
  explicit PixelFormatOptimizer(net_instaweb::MessageHandler* handler);
  virtual ~PixelFormatOptimizer();

  // PixelFormatOptimizer acquires ownership of reader, even in case of failure.
  ScanlineStatus Initialize(ScanlineReaderInterface* reader);

  virtual ScanlineStatus ReadNextScanlineWithStatus(void** out_scanline_bytes);

  // Resets the resizer to its initial state. Always returns true.
  virtual bool Reset();

  // Returns number of bytes required to store a scanline.
  virtual size_t GetBytesPerScanline() {
    return bytes_per_row_;
  }

  // Returns true if there are more scanlines to read. Returns false if the
  // object has not been initialized or all of the scanlines have been read.
  virtual bool HasMoreScanLines() {
    return (output_row_ < GetImageHeight());
  }

  // Returns the height of the image.
  virtual size_t GetImageHeight() {
    return reader_->GetImageHeight();
  }

  // Returns the width of the image.
  virtual size_t GetImageWidth() {
    return reader_->GetImageWidth();
  }

  // Returns the pixel format of the image.
  virtual PixelFormat GetPixelFormat() {
    return pixel_format_;
  }

  // Returns true if the image is encoded in progressive / interlacing format.
  virtual bool IsProgressive() {
    return reader_->IsProgressive();
  }

  // This method should not be called. If it does get called, in DEBUG mode it
  // will throw a FATAL error and in RELEASE mode it does nothing.
  virtual ScanlineStatus InitializeWithStatus(const void* image_buffer,
                                              size_t buffer_length);

 private:
  net_instaweb::scoped_ptr<ScanlineReaderInterface> reader_;
  size_t bytes_per_row_;
  PixelFormat pixel_format_;
  size_t output_row_;
  bool strip_alpha_;
  bool was_initialized_;

  // Buffer for storing decoded scanlines.
  net_instaweb::scoped_array<uint8_t> input_lines_;

  // Number of rows which have been examined and buffered.
  size_t input_row_;

  // Buffer for storing a single converted scanline.
  net_instaweb::scoped_array<uint8_t> output_line_;

  net_instaweb::MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(PixelFormatOptimizer);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_PIXEL_FORMAT_OPTIMIZER_H_
