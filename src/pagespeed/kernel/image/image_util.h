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

// Returns the MIME-type string corresponding to the given ImageFormat.
const char* ImageFormatToMimeTypeString(ImageFormat image_type);

// Returns a string representation of the given ImageFormat.
const char* ImageFormatToString(ImageFormat image_type);

// Returns a string representation of the given PixelFormat.
const char* GetPixelFormatString(PixelFormat pixel_format);

// Returns the number of bytes needed to encode each pixel in the
// given format.
int GetBytesPerPixel(PixelFormat pixel_format);

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_IMAGE_UTIL_H_
