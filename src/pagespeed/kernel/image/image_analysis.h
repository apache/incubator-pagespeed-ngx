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

#ifndef PAGESPEED_KERNEL_IMAGE_IMAGE_ANALYSIS_H_
#define PAGESPEED_KERNEL_IMAGE_IMAGE_ANALYSIS_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/image/scanline_interface.h"

namespace net_instaweb {
class MessageHandler;
}

namespace pagespeed {

namespace image_compression {

using net_instaweb::MessageHandler;

const int kNumColorHistogramBins = 256;

// Quickly compute image gradient (i.e., image edge) from the luminance.
// Supports GRAY_8, RGB_888, and RGBA_8888 formats. Alpha is ignored if it
// exists. The gradient has the same size as the input image.
//
// To speed up computation, the following simplifications are used:
//   - The gradient of a pixel is the greater of (1) the absolute difference
//     between its top neighbor and bottom neighbor, and (2) the absolute
//     difference between its left neighbor and right neighbor.
//   - Pixels on the image border (i.e., first and last rows, first and last
//     columns) have zero value for the gradient.
//   - The luminance of a pixel is the average of the red, green, and blue
//     channels, rounded up to the next integer.
bool SimpleGradient(const uint8_t* image, int width, int height,
                    int bytes_per_line, PixelFormat pixel_format,
                    MessageHandler* handler, uint8_t* gradient);

// Return histogram of a grayscale image. The histogram has 256 bins,
// and is normalized such that the sum is 1. Pixels at the following locations
// will be used to compute the histogram:
//   x0 <= x < x0 + width
//   y0 <= y < y0 + height.
void Histogram(const uint8_t* image, int width, int height, int bytes_per_line,
               int x0, int y0, float* hist);

// Return the photographic metric. Photos will have large metric values, while
// computer generated graphics, especially those consists of only a few colors
// or slowly changing colors will have small values. Graphics usually
// have tall and sharp peaks in their gradient histogram, so the mean width
// of histogram peaks will be small. On the other hand, photos have flat peaks
// in their gradient histogram so the mean width of peaks will be larger.
// Metric can have values between 1 and 256, and the recommended threshold for
// separating graphics and photo is around 7.5.
float PhotoMetric(const uint8_t* image, int width, int height,
                  int bytes_per_line, PixelFormat pixel_format, float threshold,
                  MessageHandler* handler);

// Return the average width of all the peaks in the histogram. A peak is a
// maximum set of contiguous bins such that all of them are greater than or
// equal to (max(hist) * threshold).
float AveragePeakWidth(const float* hist, float threshold);

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_IMAGE_ANALYSIS_H_
