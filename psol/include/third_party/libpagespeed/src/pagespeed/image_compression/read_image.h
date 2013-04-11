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

#ifndef THIRD_PARTY_PAGESPEED_SRC_PAGESPEED_IMAGE_COMPRESSION_READ_IMAGE_H_
#define THIRD_PARTY_PAGESPEED_SRC_PAGESPEED_IMAGE_COMPRESSION_READ_IMAGE_H_

#include "base/basictypes.h"
#include "pagespeed/image_compression/scanline_interface.h"

namespace pagespeed {

namespace image_compression {

enum ImageFormat {
  IMAGE_UNKNOWN = 0,
  IMAGE_JPEG,
  IMAGE_PNG,
  IMAGE_GIF,
  IMAGE_WEBP
};

// Decode the image stream and return the image information. Use non-null
// pointers to retrieve the informatin you need, and use null pointers to
// ignore other information.
//
// If the input "pixels" is set to a null pointer, the function will finish
// quicker because the pixel data will not be decoded. If "pixel" is set to
// a non-null pointer, the function will return a buffer containning the pixel
// data. You are responsible for destroying the buffer using free().
//
// Arguments "width" and "height" indicate the number of pixels along the
// horizontal and vertical directions, respectively. Argument "stride" indicates
// the number of bytes between the starting points of adjacent rows. Garbage
// bytes may be padded to the end of rows in order to make "stride" a multiplier
// of 4.
//
bool ReadImage(ImageFormat image_type,
               const void* image_buffer,
               size_t buffer_length,
               void** pixels,
               PixelFormat* pixel_format,
               size_t* width,
               size_t* height,
               size_t* stride);

}  // namespace image_compression

}  // namespace pagespeed

#endif  // THIRD_PARTY_PAGESPEED_SRC_PAGESPEED_IMAGE_COMPRESSION_READ_IMAGE_H_
