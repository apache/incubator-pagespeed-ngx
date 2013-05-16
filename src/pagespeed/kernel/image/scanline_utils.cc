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

#include "base/logging.h"

namespace pagespeed {

namespace image_compression {

size_t GetNumChannelsFromPixelFormat(PixelFormat format) {
  int num_channels;
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
      LOG(INFO) << "Invalid pixel format.";
      num_channels = 0;
  }
  return num_channels;
}

}  // namespace image_compression

}  // namespace pagespeed
