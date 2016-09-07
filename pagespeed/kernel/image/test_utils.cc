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
#include <cstdint>
#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/image/read_image.h"
#include "pagespeed/kernel/image/scanline_interface.h"
#include "pagespeed/kernel/image/scanline_utils.h"


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
        double alpha_scaling1 = 0, alpha_scaling2 = 0;
        if (ch != kIndexAlpha && num_channels > 3) {
          alpha_scaling1 = pixels1[kIndexAlpha] / 255.0;
          alpha_scaling2 = pixels2[kIndexAlpha] / 255.0;
        }
        int index = y * stride + (x * num_channels + ch);
        double dif = static_cast<double>(pixels1[index] * alpha_scaling1) -
                     static_cast<double>(pixels2[index] * alpha_scaling2);
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
  content->clear();
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
    bool ignore_transparent_rgb,
    MessageHandler* message_handler) {
  DecodeAndCompareImagesByPSNR(image_format1, image_buffer1, buffer_length1,
                               image_format2, image_buffer2, buffer_length2,
                               kMaxPSNR, ignore_transparent_rgb,
                               true /* expand colors */,
                               message_handler);
}

void DecodeAndCompareImagesByPSNR(
    pagespeed::image_compression::ImageFormat image_format1,
    const void* image_buffer1,
    size_t buffer_length1,
    pagespeed::image_compression::ImageFormat image_format2,
    const void* image_buffer2,
    size_t buffer_length2,
    double min_psnr,
    bool ignore_transparent_rgb,
    bool expand_colors,
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

  // Verify that the pixel format and sizes are the same.
  ASSERT_EQ(width1, width2);
  ASSERT_EQ(height1, height2);
  if (!expand_colors) {
    ASSERT_EQ(pixel_format1, pixel_format2);
    ASSERT_EQ(stride1, stride2);
  }

  CompareImageRegionsByPSNR(pixels1, pixel_format1, stride1, 0, 0,
                            pixels2, pixel_format2, stride2, 0, 0,
                            width1, height1, min_psnr, ignore_transparent_rgb,
                            expand_colors, message_handler);

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

void ComparePixelsByPSNR(const uint8_t* image1, const uint8_t* image2,
                         PixelFormat format, int num_rows, int num_cols,
                         double min_psnr, bool ignore_transparent_rgb,
                         MessageHandler* handler) {
  int num_channels = GetNumChannelsFromPixelFormat(format, handler);
  int bytes_per_line = num_channels * num_cols;
  int bytes_per_image = bytes_per_line * num_rows;
  if (min_psnr >= kMaxPSNR) {
    // Verify that all of the pixels are exactly the same.
    if (!ignore_transparent_rgb || format != RGBA_8888) {
      EXPECT_EQ(0, memcmp(image1, image2, bytes_per_image));
    } else {
      // To ignore transparent pixels, we have to check the pixels one by one.
      for (int row = 0; row < num_rows; ++row) {
        for (int col = 0; col < num_cols; ++col) {
          int ch = 0;
          if (image1[row * bytes_per_line + col * 4 + 3] == 0) {
            // Skip checking RGB when alpha is 0, still test alpha itself.
            ch = 3;  // Index of alpha channel in RGBA_8888.
          }

          for (; ch < 4; ++ch) {
            int index = row * bytes_per_line + col * 4 + ch;
            EXPECT_EQ(image1[index], image2[index])
                << "  row: " << row
                << "  col: " << col
                << "  ch: " << ch
                << "  index: " << index;
          }
        }
      }
    }
  } else {
    double psnr =
        ComputePSNR(image1, image2, num_cols, num_rows, num_channels,
                    bytes_per_line);
    EXPECT_LE(min_psnr, psnr);
  }
}

void CompareImageRegionsByPSNR(const uint8_t* image1, PixelFormat format1,
                               int bytes_per_row1, int col1, int row1,
                               const uint8_t* image2, PixelFormat format2,
                               int bytes_per_row2, int col2, int row2,
                               int num_cols, int num_rows, double min_psnr,
                               bool ignore_transparent_rgb,
                               bool expand_colors, MessageHandler* handler) {
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

  int bytes_per_image = bytes_per_line * num_rows;
  net_instaweb::scoped_array<uint8_t>
      image_buffer1(new uint8_t[bytes_per_image]);
  net_instaweb::scoped_array<uint8_t>
      image_buffer2(new uint8_t[bytes_per_image]);
  ASSERT_TRUE(image_buffer1 != NULL && image_buffer2 != NULL);

  memset(image_buffer1.get(), 0, bytes_per_image);
  memset(image_buffer2.get(), 0, bytes_per_image);

  bool should_expand_colors = (format1 != format2 && expand_colors);
  for (int row = 0; row < num_rows; ++row) {
    const uint8_t* src1 = image1 + (row + row1) * bytes_per_row1;
    uint8_t* dest1 = image_buffer1.get() + row * bytes_per_line;
    const uint8_t* src2 = image2 + (row + row2) * bytes_per_row2;
    uint8_t* dest2 = image_buffer2.get() + row * bytes_per_line;

    if (should_expand_colors) {
      // Expand and copy colors.
      ASSERT_TRUE(ExpandPixelFormat(num_cols, format1, col1, src1, format, 0,
                                    dest1, handler));
      ASSERT_TRUE(ExpandPixelFormat(num_cols, format2, col2, src2, format, 0,
                                    dest2, handler));
    } else {
      memcpy(dest1, src1 + col1 * num_channels1, bytes_per_line);
      memcpy(dest2, src2 + col2 * num_channels2, bytes_per_line);
    }
  }

  ComparePixelsByPSNR(image_buffer1.get(), image_buffer2.get(),
                      format, num_rows, num_cols, min_psnr,
                      ignore_transparent_rgb, handler);
}

void CompareImageRegions(const uint8_t* image1, PixelFormat format1,
                         int bytes_per_row1, int col1, int row1,
                         const uint8_t* image2, PixelFormat format2,
                         int bytes_per_row2, int col2, int row2,
                         int num_cols, int num_rows, MessageHandler* handler) {
  CompareImageRegionsByPSNR(image1, format1, bytes_per_row1, col1, row1,
                            image2, format2, bytes_per_row2, col2, row2,
                            num_cols, num_rows, kMaxPSNR,
                            false,  // ignore_transparent_rgb
                            true,  // Expand colors
                            handler);
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

// Returns true if 2 animated images are identical.
bool CompareAnimatedImages(const GoogleString& expected_image_filename,
                           const GoogleString& actual_image_content,
                           MessageHandler* message_handler) {
  // TODO(vchudnov): Fill this in.
  return true;
}

}  // namespace image_compression

}  // namespace pagespeed
