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
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/image/read_image.h"
#include "pagespeed/kernel/image/scanline_interface.h"
#include "pagespeed/kernel/image/scanline_utils.h"

namespace net_instaweb {
class MessageHandler;
}

namespace pagespeed {

namespace {

const double kMaxPSNR = 99.0;
const int kIndexAlpha = 3;
const uint8_t kAlphaTransparent = 0;

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

using net_instaweb::MessageHandler;

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
    size_t buffer_length2,
    MessageHandler* message_handler) {
  DecodeAndCompareImagesByPSNR(image_format1, image_buffer1, buffer_length1,
                               image_format2, image_buffer2, buffer_length2,
                               kMaxPSNR, message_handler);
}

void DecodeAndCompareImagesByPSNR(
    pagespeed::image_compression::ImageFormat image_format1,
    const void* image_buffer1,
    size_t buffer_length1,
    pagespeed::image_compression::ImageFormat image_format2,
    const void* image_buffer2,
    size_t buffer_length2,
    double min_psnr,
    MessageHandler* message_handler) {
  uint8_t* pixels1 = NULL;
  uint8_t* pixels2 = NULL;
  PixelFormat pixel_format1, pixel_format2;
  size_t width1, height1, stride1, width2, height2, stride2;

  // Decode the images.
  ASSERT_TRUE(ReadImage(image_format1, image_buffer1, buffer_length1,
                        reinterpret_cast<void**>(&pixels1),
                        &pixel_format1, &width1, &height1, &stride1,
                        message_handler));
  ASSERT_TRUE(ReadImage(image_format2, image_buffer2, buffer_length2,
                        reinterpret_cast<void**>(&pixels2),
                        &pixel_format2, &width2, &height2, &stride2,
                        message_handler));
  int num_channels = GetNumChannelsFromPixelFormat(pixel_format1,
                                                   message_handler);

  // Verify that the pixel format and sizes are the same.
  EXPECT_EQ(pixel_format1, pixel_format2);
  EXPECT_EQ(width1, width2);
  EXPECT_EQ(height1, height2);
  EXPECT_EQ(stride1, stride2);

  if (min_psnr >= kMaxPSNR) {
    // Verify that all of the pixels are exactly the same.
    for (size_t y = 0; y < height1; ++y) {
      for (size_t x = 0; x < width1; ++x) {
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

void CompareImageReaders(ScanlineReaderInterface* reader1,
                         ScanlineReaderInterface* reader2) {
  ASSERT_NE(reinterpret_cast<ScanlineReaderInterface*>(NULL), reader1);
  ASSERT_NE(reinterpret_cast<ScanlineReaderInterface*>(NULL), reader2);
  ASSERT_EQ(reader1->GetPixelFormat(), reader2->GetPixelFormat());
  ASSERT_EQ(reader1->GetImageHeight(), reader2->GetImageHeight());
  ASSERT_EQ(reader1->GetImageWidth(), reader2->GetImageWidth());
  ASSERT_EQ(reader1->GetBytesPerScanline(), reader2->GetBytesPerScanline());

  while (reader1->HasMoreScanLines() && reader2->HasMoreScanLines()) {
    uint8_t* scanline1 = NULL;
    uint8_t* scanline2 = NULL;
    ASSERT_TRUE(reader1->ReadNextScanline(
        reinterpret_cast<void**>(&scanline1)));
    ASSERT_TRUE(reader2->ReadNextScanline(
        reinterpret_cast<void**>(&scanline2)));
    EXPECT_EQ(0, memcmp(scanline1, scanline2, reader1->GetBytesPerScanline()));
  }

  // Make sure both readers have exhausted all of the scanlines.
  EXPECT_FALSE(reader1->HasMoreScanLines());
  EXPECT_FALSE(reader2->HasMoreScanLines());
}

void CompareImageRegions(const uint8_t* image1, PixelFormat format1,
                         int bytes_per_row1, int col1, int row1,
                         const uint8_t* image2, PixelFormat format2,
                         int bytes_per_row2, int col2, int row2,
                         int num_cols, int num_rows, MessageHandler* handler) {
  ASSERT_TRUE(format1 != UNSUPPORTED && format2 != UNSUPPORTED);
  const int num_channels1 =
    GetNumChannelsFromPixelFormat(format1, handler);
  const int num_channels2 =
    GetNumChannelsFromPixelFormat(format2, handler);

  PixelFormat format;
  int num_channels;
  if (num_channels1 >= num_channels2) {
    format = format1;
    num_channels = num_channels1;
  } else {
    format = format2;
    num_channels = num_channels2;
  }
  int bytes_per_line = num_cols * num_channels;

  net_instaweb::scoped_array<uint8_t> line1(new uint8_t[bytes_per_line]);
  net_instaweb::scoped_array<uint8_t> line2(new uint8_t[bytes_per_line]);
  ASSERT_TRUE(line1 != NULL && line2 != NULL);

  image1 += row1 * bytes_per_row1;
  image2 += row2 * bytes_per_row2;
  for (int row = 0; row < num_rows; ++row) {
    ASSERT_TRUE(ExpandPixelFormat(num_cols, format1, col1, image1, format, 0,
                                  line1.get(), handler));
    ASSERT_TRUE(ExpandPixelFormat(num_cols, format2, col2, image2, format, 0,
                                  line2.get(), handler));
    if (format != RGBA_8888) {
      EXPECT_EQ(0, memcmp(line1.get(), line2.get(), bytes_per_line));
    } else {
      uint8_t* pixel1 = line1.get();
      uint8_t* pixel2 = line2.get();
      for (int col = 0; col < num_cols; ++col) {
        if (pixel1[kIndexAlpha] != kAlphaTransparent ||
            pixel2[kIndexAlpha] != kAlphaTransparent) {
          EXPECT_EQ(0, memcmp(pixel1, pixel2, num_channels));
        }
        pixel1 += num_channels;
        pixel2 += num_channels;
      }
    }

    image1 += bytes_per_row1;
    image2 += bytes_per_row2;
  }
}

void SynthesizeImage(int width, int height, int bytes_per_line,
                     int num_channels, const uint8_t* seed_value,
                     const int* delta_x, const int* delta_y, uint8_t* image) {
  ASSERT_TRUE(image != NULL);
  ASSERT_GT(width, 0);
  ASSERT_GT(height, 0);
  ASSERT_GE(bytes_per_line, width);

  net_instaweb::scoped_array<uint8_t> current_value(new uint8_t[num_channels]);
  memcpy(current_value.get(), seed_value,
         num_channels * sizeof(seed_value[0]));

  for (int y = 0; y < height; ++y) {
    uint8_t* pixel = image + y * bytes_per_line;
    for (int x = 0; x < width; ++x) {
      for (int ch = 0; ch < num_channels; ++ch) {
        pixel[ch] = current_value[ch];
        current_value[ch] += delta_x[ch];
      }
      pixel += num_channels;
    }
    // Compute the value for the first pixel in the next line. The next line
    // has values increased from those of the current line by delta_y.
    for (int ch = 0; ch < num_channels; ++ch) {
      current_value[ch] = image[y * bytes_per_line + ch] + delta_y[ch];
    }
  }
}

}  // namespace image_compression

}  // namespace pagespeed
