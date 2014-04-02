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

namespace pagespeed {

namespace {

static const char kInvalidImageFormat[] = "Invalid image format";
static const char kInvalidPixelFormat[] = "Invalid pixel format";

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

int GetBytesPerPixel(PixelFormat pixel_format) {
  switch (pixel_format) {
    case UNSUPPORTED: return 0;
    case RGB_888:     return 3;
    case RGBA_8888:   return 4;
    case GRAY_8:      return 1;
    // No default so compiler will complain if any enum is not processed.
  }
  return 0;
}

}  // namespace image_compression

}  // namespace pagespeed
