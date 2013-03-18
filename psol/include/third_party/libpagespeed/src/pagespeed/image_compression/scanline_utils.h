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

#ifndef THIRD_PARTY_PAGESPEED_SRC_PAGESPEED_IMAGE_COMPRESSION_SCANLINE_UTILS_H_
#define THIRD_PARTY_PAGESPEED_SRC_PAGESPEED_IMAGE_COMPRESSION_SCANLINE_UTILS_H_

#include "base/basictypes.h"
#include "pagespeed/image_compression/scanline_interface.h"

namespace pagespeed {

namespace image_compression {

// Return the number of channels, including color channels and alpha channel,
// for the input pixel format.
//   GRAY_8:    1
//   RGB_888:   3
//   RGBA_8888: 4
//
size_t GetNumChannelsFromPixelFormat(PixelFormat format);

}  // namespace image_compression

}  // namespace pagespeed

#endif  // THIRD_PARTY_PAGESPEED_SRC_PAGESPEED_IMAGE_COMPRESSION_SCANLINE_UTILS_H_
