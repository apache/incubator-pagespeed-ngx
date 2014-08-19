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

#include <cstddef>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/image/image_util.h"

namespace net_instaweb {
class MessageHandler;
}

namespace pagespeed {

namespace image_compression {

class ScanlineReaderInterface;
using net_instaweb::MessageHandler;

const int kNumColorHistogramBins = 256;

// Computes image gradient (i.e., image edge) from the luminance using Sobel
// operator: http://en.wikipedia.org/wiki/Sobel_operator.
// Supports GRAY_8, RGB_888, and RGBA_8888 formats. Alpha is ignored if it
// exists. The gradient has the same size as the input image.
//
// The following simplifications are used:
//   - Pixels on the image border (i.e., first and last rows, first and last
//     columns) have zero value for the gradient.
//   - The luminance of a pixel is the average of the red, green, and blue
//     channels.
bool SobelGradient(const uint8_t* image, int width, int height,
                   int bytes_per_line, PixelFormat pixel_format,
                   MessageHandler* handler, uint8_t* gradient);

// Returns histogram of a grayscale image. The histogram has 256 bins,
// and is normalized such that the sum is 1. Pixels at the following locations
// will be used to compute the histogram:
//   x0 <= x < x0 + width
//   y0 <= y < y0 + height.
void Histogram(const uint8_t* image, int width, int height, int bytes_per_line,
               int x0, int y0, float* hist);

// Returns the photographic metric. Photos will have large metric values, while
// computer generated graphics, especially those consisting of only a few colors
// or slowly changing colors will have small values. Graphics usually
// have tall and sharp peaks in their gradient histogram, while photo peaks are
// short and wide. Thus, the width of the widest peak in the color histogram
// can be used as the metric for photo-likeness. The metric can have values
// between 1 and 256, and the recommended threshold for separating graphics
// and photo is around 16.
float PhotoMetric(const uint8_t* image, int width, int height,
                  int bytes_per_line, PixelFormat pixel_format, float threshold,
                  MessageHandler* handler);

// Returns the width of the widest peak in the color histogram. A peak is a
// maximum set of contiguous bins such that all of them are greater than or
// equal to (max(hist) * threshold).
float WidestPeakWidth(const float* hist, float threshold);

// Returns true if the image looks like a photo, or false if it looks like
// computer generated graphics. The reader must be initialized with the image
// to be processed.
bool IsPhoto(ScanlineReaderInterface* reader, MessageHandler* handler);

// Return key information of the image. For the information which you do not
// need, set the arguments to NULL so they will not be computed.
//
// "is_progressive" is only valid for single frame images. For animated images
// it will always be set to "false" even if some frames were encoded in
// progressive format.
bool AnalyzeImage(ImageFormat image_type,
                  const void* image_buffer,
                  size_t buffer_length,
                  int* width,
                  int* height,
                  bool* is_progressive,
                  bool* is_animated,
                  bool* has_transparency,
                  bool* is_photo,
                  int* quality,
                  ScanlineReaderInterface** reader,
                  MessageHandler* handler);

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_IMAGE_ANALYSIS_H_
