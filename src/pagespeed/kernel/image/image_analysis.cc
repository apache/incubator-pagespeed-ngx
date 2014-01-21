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

#include "pagespeed/kernel/image/image_analysis.h"

#include <algorithm>
#include "base/logging.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/scanline_utils.h"

namespace pagespeed {

namespace {

template <class T>
inline T AbsDif(T v1, T v2) {
  return (v1 >= v2 ? v1 - v2 : v2 - v1);
}

}  // namespace

namespace image_compression {

bool SimpleGradient(const uint8_t* image, int width, int height,
                    int bytes_per_line, PixelFormat pixel_format,
                    MessageHandler* handler, uint8_t* gradient) {
  if (width < 3 || height < 3 ||
      (pixel_format != GRAY_8 && pixel_format != RGB_888 &&
       pixel_format != RGBA_8888)) {
    return false;
  }

  const uint8_t* luminance = NULL;
  int width_luminance;
  net_instaweb::scoped_array<uint8_t> luminance_buffer;

  if (pixel_format == GRAY_8) {
    luminance = image;
    width_luminance = bytes_per_line;
  } else {
    luminance_buffer.reset(new uint8_t[width * height]);
    if (luminance_buffer == NULL) {
      return false;
    }
    luminance = luminance_buffer.get();
    width_luminance = width;

    const int num_channels =
      GetNumChannelsFromPixelFormat(pixel_format, handler);

    // Compute the luminance which is simply the average of R, G, and B.
    uint8_t* out_pixel = luminance_buffer.get();
    for (int y = 0; y < height; ++y) {
      const uint8_t* in_channel = image + y * bytes_per_line;
      for (int x = 0; x < width; ++x) {
        *out_pixel = static_cast<uint8_t>((static_cast<int>(in_channel[0]) +
                                           static_cast<int>(in_channel[1]) +
                                           static_cast<int>(in_channel[2]) +
                                           2) / 3);
        ++out_pixel;
        in_channel += num_channels;
      }
    }
  }

  // Compute the gradient which is simply the maximum of the convolution of
  // the image with the following 2 kernels:
  //   [ 0 -1 0 ]        [  0 0 0 ]
  //   [ 0  0 0 ]        [ -1 0 1 ]
  //   [ 0  1 0 ]        [  0 0 0 ]
  memset(gradient, 0, width * height * sizeof(gradient[0]));
  for (int y = 1; y < height-1; ++y) {
    for (int x = 1; x < width-1; ++x) {
      int idx_luminance = y * width_luminance + x;
      int idx_gradient = y * width + x;
      uint8_t dif_y = AbsDif(luminance[idx_luminance - width_luminance],
                             luminance[idx_luminance + width_luminance]);
      uint8_t dif_x = AbsDif(luminance[idx_luminance - 1],
                             luminance[idx_luminance + 1]);
      gradient[idx_gradient] = std::max(dif_x, dif_y);
    }
  }

  return true;
}

void Histogram(const uint8_t* image, int width, int height, int bytes_per_line,
               int x0, int y0, float* hist) {
  DCHECK(bytes_per_line >= width);

  uint32_t hist_int[kNumColorHistogramBins];
  memset(hist_int, 0, kNumColorHistogramBins * sizeof(hist_int[0]));

  // Aggregate the histogram.
  for (int y = y0; y < y0 + height; ++y) {
    int i = y * bytes_per_line + x0;
    for (int x = 0; x < width; ++x, ++i) {
      ++hist_int[image[i]];
    }
  }

  const float norm = 1.0f / static_cast<float>(height * width);
  for (int i = 0; i < kNumColorHistogramBins; ++i) {
    hist[i] = static_cast<float>(hist_int[i]) * norm;
  }
}

}  // namespace image_compression

}  // namespace pagespeed
