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

#include "pagespeed/kernel/image/scanline_utils.h"

#include "pagespeed/kernel/base/message_handler.h"

namespace pagespeed {

namespace {

const uint8_t kAlphaOpaque = 255;

}  // namespace

namespace image_compression {

size_t GetNumChannelsFromPixelFormat(PixelFormat format,
                                     MessageHandler* handler) {
  int num_channels = 0;
  switch (format) {
    case GRAY_8:
      num_channels = 1;
      break;
    case RGB_888:
      num_channels = 3;
      break;
    case RGBA_8888:
      num_channels = 4;
      break;
    default:
      PS_LOG_DFATAL(handler, "Invalid pixel format.");
  }
  return num_channels;
}

bool ExpandPixelFormat(size_t num_pixels, PixelFormat src_format,
                       int src_offset, const uint8_t* src_data,
                       PixelFormat dst_format, int dst_offset,
                       uint8_t* dst_data, MessageHandler* handler) {
  const int src_num_channels =
    GetNumChannelsFromPixelFormat(src_format, handler);
  const int dst_num_channels =
    GetNumChannelsFromPixelFormat(dst_format, handler);
  const int rgb_num_channels =
    GetNumChannelsFromPixelFormat(RGB_888, handler);
  const int opaque_channel = rgb_num_channels;
  src_data += src_offset * src_num_channels;
  dst_data += dst_offset * dst_num_channels;

  bool is_ok = true;
  switch (dst_format) {
    case RGB_888:
      switch (src_format) {
        case GRAY_8:
          for (size_t i = 0; i < num_pixels; ++i) {
            memset(dst_data, *src_data, dst_num_channels);
            ++src_data;
            dst_data += dst_num_channels;
          }
          break;
        case RGB_888:
          memcpy(dst_data, src_data, num_pixels * src_num_channels);
          break;
        default:
          is_ok = false;
          PS_LOG_DFATAL(handler, "Unsupported pixel format conversion.");
          break;
      }
      break;

    case RGBA_8888:
      switch (src_format) {
        case GRAY_8:
          for (size_t i = 0; i < num_pixels; ++i) {
            memset(dst_data, *src_data, rgb_num_channels);
            dst_data[opaque_channel] = kAlphaOpaque;
            ++src_data;
            dst_data += dst_num_channels;
          }
          break;
        case RGB_888:
          for (size_t i = 0; i < num_pixels; ++i) {
            memcpy(dst_data, src_data, src_num_channels);
            dst_data[opaque_channel] = kAlphaOpaque;
            src_data += src_num_channels;
            dst_data += dst_num_channels;
          }
          break;
        case RGBA_8888:
          memcpy(dst_data, src_data, num_pixels * src_num_channels);
          break;
        default:
          is_ok = false;
          PS_LOG_DFATAL(handler, "Unsupported pixel format conversion.");
          break;
      }
      break;
    default:
      is_ok = false;
      PS_LOG_DFATAL(handler, "Unsupported pixel format conversion.");
      break;
  }
  return is_ok;
}

}  // namespace image_compression

}  // namespace pagespeed
