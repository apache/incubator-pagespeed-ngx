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

// Author: Huibao Lin

#include "pagespeed/kernel/image/image_util.h"

#include "pagespeed/kernel/base/countdown_timer.h"
#include "pagespeed/kernel/base/message_handler.h"

namespace pagespeed {

namespace {

static const char kInvalidImageFormat[] = "Invalid image format";
static const char kInvalidPixelFormat[] = "Invalid pixel format";

// Magic number of the images.
const char kPngHeader[] = "\x89PNG\r\n\x1a\n";
const size_t kPngHeaderLength = arraysize(kPngHeader) - 1;
const char kGifHeader[] = "GIF8";
const size_t kGifHeaderLength = arraysize(kGifHeader) - 1;
const char kWebpIffHeader[] = "IFF";
const char kWebpWebpHeader[] = "WEBP";
const char kWebpLosslessHeader[] = "VP8L";

// char to int *without sign extension*.
inline int CharToInt(char c) {
  uint8 uc = static_cast<uint8>(c);
  return static_cast<int>(uc);
}

}  // namespace

namespace image_compression {

const char* ImageFormatToMimeTypeString(ImageFormat image_type) {
  switch (image_type) {
    case IMAGE_UNKNOWN: return "image/unknown";
    case IMAGE_JPEG:    return "image/jpeg";
    case IMAGE_PNG:     return "image/png";
    case IMAGE_GIF:     return "image/gif";
    case IMAGE_WEBP:    return "image/webp";
    // No default so compiler will complain if any enum is not processed.
  }
  return kInvalidImageFormat;
}

const char* ImageFormatToString(ImageFormat image_type) {
  switch (image_type) {
    case IMAGE_UNKNOWN: return "IMAGE_UNKNOWN";
    case IMAGE_JPEG:    return "IMAGE_JPEG";
    case IMAGE_PNG:     return "IMAGE_PNG";
    case IMAGE_GIF:     return "IMAGE_GIF";
    case IMAGE_WEBP:    return "IMAGE_WEBP";
    // No default so compiler will complain if any enum is not processed.
  }
  return kInvalidImageFormat;
}

const char* GetPixelFormatString(PixelFormat pixel_format) {
  switch (pixel_format) {
    case UNSUPPORTED: return "UNSUPPORTED";
    case RGB_888:     return "RGB_888";
    case RGBA_8888:   return "RGBA_8888";
    case GRAY_8:      return "GRAY_8";
    // No default so compiler will complain if any enum is not processed.
  }
  return kInvalidPixelFormat;
}

size_t GetBytesPerPixel(PixelFormat pixel_format) {
  switch (pixel_format) {
    case UNSUPPORTED: return 0;
    case RGB_888:     return 3;
    case RGBA_8888:   return 4;
    case GRAY_8:      return 1;
    // No default so compiler will complain if any enum is not processed.
  }
  return 0;
}

ImageFormat ComputeImageFormat(const StringPiece& buf,
                               bool* is_webp_lossless_alpha) {
  // Image classification based on buffer contents gakked from leptonica,
  // but based on well-documented headers (see Wikipedia etc.).
  // Note that we can be fooled if we're passed random binary data;
  // we make the call based on as few as two bytes (JPEG).
  ImageFormat image_format = IMAGE_UNKNOWN;
  if (buf.size() >= 8) {
    // Note that gcc rightly complains about constant ranges with the
    // negative char constants unless we cast.
    switch (CharToInt(buf[0])) {
      case 0xff:
        // Either jpeg or jpeg2
        // (the latter we don't handle yet, and don't bother looking for).
        if (CharToInt(buf[1]) == 0xd8) {
          image_format = IMAGE_JPEG;
        }
        break;
      case 0x89:
        // Possible png.
        if (StringPiece(buf.data(), kPngHeaderLength) ==
            StringPiece(kPngHeader, kPngHeaderLength)) {
          image_format = IMAGE_PNG;
        }
        break;
      case 'G':
        // Possible gif.
        if ((StringPiece(buf.data(), kGifHeaderLength) ==
             StringPiece(kGifHeader, kGifHeaderLength)) &&
            (buf[kGifHeaderLength] == '7' || buf[kGifHeaderLength] == '9') &&
            buf[kGifHeaderLength + 1] == 'a') {
          image_format = IMAGE_GIF;
        }
        break;
      case 'R':
        // Possible Webp
        // Detailed explanation on parsing webp format is available at
        // http://code.google.com/speed/webp/docs/riff_container.html
        if (buf.size() >= 20 && buf.substr(1, 3) == kWebpIffHeader &&
            buf.substr(8, 4) == kWebpWebpHeader) {
          image_format = IMAGE_WEBP;
          if (buf.substr(12, 4) == kWebpLosslessHeader) {
            *is_webp_lossless_alpha = true;
          } else {
            *is_webp_lossless_alpha = false;
          }
        }
        break;
      default:
        break;
    }
  }
  return image_format;
}

bool ConversionTimeoutHandler::Continue(int percent, void* user_data) {
  ConversionTimeoutHandler* timeout_handler =
    static_cast<ConversionTimeoutHandler*>(user_data);
  if (timeout_handler != NULL &&
      !timeout_handler->countdown_timer_.HaveTimeLeft()) {
    // We include the output_->empty() check after HaveTimeLeft()
    // for testing, in case there's a callback that writes to
    // output_ invoked at a time that triggers a timeout.
    if (!timeout_handler->output_->empty()) {
      return true;
    }
    PS_LOG_WARN(timeout_handler->handler_, "Image conversion timed out.");
    timeout_handler->was_timed_out_ = true;
    return false;
  }
  return true;
}

}  // namespace image_compression

}  // namespace pagespeed
