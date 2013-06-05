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

#include "pagespeed/kernel/image/test_utils.h"

#include <math.h>
#include <cstdlib>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/image/read_image.h"
#include "pagespeed/kernel/image/scanline_interface.h"
#include "pagespeed/kernel/image/scanline_utils.h"

namespace pagespeed {

namespace {

const double kMaxPSNR = 99.0;

// Definition of Peak-Signal-to-Noise-Ratio (PSNR):
// http://en.wikipedia.org/wiki/Peak_signal-to-noise_ratio
//
// The implementation is similar to
// third_party/libwebp/tests/check_psnr.cc.
// However, this implementation supports image with different number of
// channels. It also allows padding at the end of scanlines.
double ComputePSNR(const uint8_t* pixels1, const uint8_t* pixels2,
                   int width, int height, int num_channels, int stride) {
  double error = 0.0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      for (int ch = 0; ch < num_channels; ++ch) {
        int index = y * stride + (x * num_channels + ch);
        double dif = static_cast<double>(pixels1[index]) -
                     static_cast<double>(pixels2[index]);
        error += dif * dif;
      }
    }
  }
  error /= (height * width * num_channels);
  return (error > 0.0) ? 10.0 * log10(255.0 * 255.0 / error) : kMaxPSNR;
}

}  // namespace

namespace image_compression {

bool ReadFile(const GoogleString& file_name,
              GoogleString* content) {
  net_instaweb::StdioFileSystem file_system;
  net_instaweb::MockMessageHandler message_handler(new net_instaweb::NullMutex);
  net_instaweb::StringWriter writer(content);
  return(file_system.ReadFile(file_name.c_str(), &writer, &message_handler));
}

bool ReadTestFile(const GoogleString& path,
                  const char* name,
                  const char* extension,
                  GoogleString* content) {
  content->clear();
  GoogleString file_name = net_instaweb::StrCat(net_instaweb::GTestSrcDir(),
       kTestRootDir, path, name, ".", extension);
  return ReadFile(file_name, content);
}

bool ReadTestFileWithExt(const GoogleString& path,
                         const char* name_with_extension,
                         GoogleString* content) {
  GoogleString file_name = net_instaweb::StrCat(net_instaweb::GTestSrcDir(),
       kTestRootDir, path, name_with_extension);
  return ReadFile(file_name, content);
}

void DecodeAndCompareImages(
    pagespeed::image_compression::ImageFormat image_format1,
    const void* image_buffer1,
    size_t buffer_length1,
    pagespeed::image_compression::ImageFormat image_format2,
    const void* image_buffer2,
    size_t buffer_length2) {
  DecodeAndCompareImagesByPSNR(image_format1, image_buffer1, buffer_length1,
                               image_format2, image_buffer2, buffer_length2,
                               kMaxPSNR);
}

void DecodeAndCompareImagesByPSNR(
    pagespeed::image_compression::ImageFormat image_format1,
    const void* image_buffer1,
    size_t buffer_length1,
    pagespeed::image_compression::ImageFormat image_format2,
    const void* image_buffer2,
    size_t buffer_length2,
    double min_psnr) {
  uint8_t* pixels1 = NULL;
  uint8_t* pixels2 = NULL;
  PixelFormat pixel_format1, pixel_format2;
  size_t width1, height1, stride1, width2, height2, stride2;

  // Decode the images.
  ASSERT_TRUE(ReadImage(image_format1, image_buffer1, buffer_length1,
                        reinterpret_cast<void**>(&pixels1),
                        &pixel_format1, &width1, &height1, &stride1));
  ASSERT_TRUE(ReadImage(image_format2, image_buffer2, buffer_length2,
                        reinterpret_cast<void**>(&pixels2),
                        &pixel_format2, &width2, &height2, &stride2));
  int num_channels = GetNumChannelsFromPixelFormat(pixel_format1);

  // Verify that the pixel format and sizes are the same.
  EXPECT_EQ(pixel_format1, pixel_format2);
  EXPECT_EQ(width1, width2);
  EXPECT_EQ(height1, height2);
  EXPECT_EQ(stride1, stride2);

  if (min_psnr >= kMaxPSNR) {
    // Verify that all of the pixels are exactly the same.
    for (int y = 0; y < height1; ++y) {
      for (int x = 0; x < width1; ++x) {
        for (int ch = 0; ch < num_channels; ++ch) {
          int index = y * stride1 + (x * num_channels + ch);
          EXPECT_EQ(pixels1[index], pixels2[index]);
        }
      }
    }
  } else {
    double psnr = ComputePSNR(pixels1, pixels2, width1, height1, num_channels,
                  stride1);
    EXPECT_LE(min_psnr, psnr);
  }

  free(pixels1);
  free(pixels2);
}

}  // namespace image_compression

}  // namespace pagespeed
