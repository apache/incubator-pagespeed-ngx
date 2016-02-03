/*
 * Copyright 2012 Google Inc.
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

#ifndef PAGESPEED_KERNEL_IMAGE_SCANLINE_UTILS_H_
#define PAGESPEED_KERNEL_IMAGE_SCANLINE_UTILS_H_

#include <cstddef>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/image_util.h"

namespace net_instaweb {
class MessageHandler;
}

namespace pagespeed {

namespace image_compression {

using net_instaweb::MessageHandler;

// Return the number of channels, including color channels and alpha channel,
// for the input pixel format.
//   GRAY_8:    1
//   RGB_888:   3
//   RGBA_8888: 4
//
size_t GetNumChannelsFromPixelFormat(PixelFormat format,
                                     MessageHandler* handler);

// Palette for RGBA_8888.
//
struct PaletteRGBA {
  uint8 red_;
  uint8 green_;
  uint8 blue_;
  uint8 alpha_;
};

// ScanlineStreamInput stores the data stream that will be used by a scanline
// reader. It also stores the position of the stream that the scanline reader
// should start to read.
//
class ScanlineStreamInput {
 public:
  explicit ScanlineStreamInput(MessageHandler* handler)
    : data_(NULL), length_(0), offset_(0),
      message_handler_(handler) {
  }

  void Reset() {
    data_ = NULL;
    length_ = 0;
    offset_ = 0;
  }

  void Initialize(const void* image_buffer, size_t buffer_length) {
    data_ = static_cast<const char*>(image_buffer);
    length_ = buffer_length;
    offset_ = 0;
  }

  void Initialize(const GoogleString& image_string) {
    data_ = static_cast<const char*>(image_string.data());
    length_ = image_string.length();
    offset_ = 0;
  }

  const char* data() {
    return data_;
  }
  size_t length() {
    return length_;
  }
  size_t offset() {
    return offset_;
  }
  void set_offset(size_t val) {
    offset_ = val;
  }
  MessageHandler* message_handler() {
    return message_handler_;
  }

 private:
  const char* data_;
  size_t length_;
  size_t offset_;
  MessageHandler* message_handler_;
};

// Expand pixel format for a scanline and change its offset in the memory.
// Supported expansions:
//   - GRAY_8    -> RGB_888
//   - RGB_888   -> RGB_888
//   - GRAY_8    -> RGBA_8888
//   - RGB_888   -> RGBA_8888
//   - RGBA_8888 -> RGBA_8888
bool ExpandPixelFormat(size_t num_pixels, PixelFormat src_format,
                       int src_offset, const uint8_t* src_data,
                       PixelFormat dst_format, int dst_offset,
                       uint8_t* dst_data, MessageHandler* handler);

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_SCANLINE_UTILS_H_
