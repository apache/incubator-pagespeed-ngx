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

#ifndef PAGESPEED_KERNEL_IMAGE_IMAGE_UTIL_H_
#define PAGESPEED_KERNEL_IMAGE_IMAGE_UTIL_H_

#include <cstddef>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"

namespace pagespeed {

namespace image_compression {

enum ImageFormat {
  IMAGE_UNKNOWN,
  IMAGE_JPEG,
  IMAGE_PNG,
  IMAGE_GIF,
  IMAGE_WEBP
};

enum PixelFormat {
  UNSUPPORTED,  // Not supported.
  RGB_888,      // RGB triplets, 24 bits per pixel.
  RGBA_8888,    // RGB triplet plus alpha channel, 32 bits per pixel.
  GRAY_8        // Grayscale, 8 bits per pixel.
};

enum RgbaChannels {
  RGBA_RED = 0,
  RGBA_GREEN,
  RGBA_BLUE,
  RGBA_ALPHA,

  RGBA_NUM_CHANNELS
};

const uint8_t kAlphaOpaque = 255;
const uint8_t kAlphaTransparent = 0;
typedef uint8_t PixelRgbaChannels[RGBA_NUM_CHANNELS];

// Packs the given A, R, G, B values into a single uint32.
inline uint32_t PackAsArgb(uint8_t alpha,
                           uint8_t red,
                           uint8_t green,
                           uint8_t blue) {
  return
      (static_cast<uint32_t>(alpha) << 24) |
      (red << 16) |
      (green << 8) |
      (blue);
}

// Packs a pixel's color channel data in RGBA format to a single
// uint32_t in ARGB format.
inline uint32_t RgbaToPackedArgb(const PixelRgbaChannels rgba) {
  return PackAsArgb(rgba[RGBA_ALPHA],
                    rgba[RGBA_RED],
                    rgba[RGBA_GREEN],
                    rgba[RGBA_BLUE]);
}

// Packs a pixel's color channel data in RGB format to a single
// uint32_t in ARGB format.
inline uint32_t RgbToPackedArgb(const PixelRgbaChannels rgba) {
  return PackAsArgb(kAlphaOpaque,
                    rgba[RGBA_RED],
                    rgba[RGBA_GREEN],
                    rgba[RGBA_BLUE]);
}

// Converts a pixel's grayscale data into a single uint32_t in ARGB
// format.
inline uint32_t GrayscaleToPackedArgb(const uint8_t luminance) {
  return PackAsArgb(kAlphaOpaque,
                    luminance,
                    luminance,
                    luminance);
}

// Sizes that can be measured in units of pixels: width, height,
// number of frames (a third dimension of the image), and indices into
// the same.
typedef uint32 size_px;

// Returns the MIME-type string corresponding to the given ImageFormat.
const char* ImageFormatToMimeTypeString(ImageFormat image_type);

// Returns a string representation of the given ImageFormat.
const char* ImageFormatToString(ImageFormat image_type);

// Returns a string representation of the given PixelFormat.
const char* GetPixelFormatString(PixelFormat pixel_format);

// Returns the number of bytes needed to encode each pixel in the
// given format.
size_t GetBytesPerPixel(PixelFormat pixel_format);

// Returns format of the image by inspecting magic numbers (cetain values at
// cetain bytes) in the file content. This method is super fast, but if a
// random binary file happens to have the magic numbers, it will incorrectly
// reports a format for the file. The problem will be corrected when the binary
// file is decoded.
ImageFormat ComputeImageFormat(const StringPiece& buf,
                               bool* is_webp_lossless_alpha);

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_IMAGE_UTIL_H_
