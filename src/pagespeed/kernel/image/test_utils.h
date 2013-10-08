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

#ifndef PAGESPEED_KERNEL_IMAGE_TEST_UTILS_H_
#define PAGESPEED_KERNEL_IMAGE_TEST_UTILS_H_

#include <cstddef>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/scanline_interface.h"

namespace net_instaweb {
class MessageHandler;
}

namespace pagespeed {

namespace image_compression {

using net_instaweb::MessageHandler;

  const char kTestRootDir[] = "/pagespeed/kernel/image/testdata/";

// Directory for test data.
const char kGifTestDir[] = "gif/";
const char kJpegTestDir[] = "jpeg/";
const char kPngSuiteGifTestDir[] = "pngsuite/gif/";
const char kPngSuiteTestDir[] = "pngsuite/";
const char kPngTestDir[] = "png/";
const char kWebpTestDir[] = "webp/";
const char kResizedTestDir[] = "resized/";

// Message to ignore.
const char kMessagePatternAnimatedGif[] =
    "Unable to optimize image with * frames.";
const char kMessagePatternFailedToOpen[] = "Failed to open*";
const char kMessagePatternFailedToRead[] = "Failed to read*";
const char kMessagePatternLibpngError[] = "libpng error:*";
const char kMessagePatternLibpngWarning[] = "libpng warning:*";
const char kMessagePatternPixelFormat[] = "Pixel format:*";
const char kMessagePatternStats[] = "Stats:*";
const char kMessagePatternUnexpectedEOF[] = "Unexpected EOF.";
const char kMessagePatternWritingToWebp[] = "Writing to webp:*";

struct ImageCompressionInfo {
 public:
  ImageCompressionInfo(const char* a_filename,
                       size_t a_original_size,
                       size_t a_compressed_size_best,
                       size_t a_compressed_size_default,
                       int a_width,
                       int a_height,
                       int a_original_bit_depth,
                       int a_original_color_type,
                       int a_compressed_bit_depth,
                       int a_compressed_color_type) :
      filename(a_filename),
      original_size(a_original_size),
      compressed_size_best(a_compressed_size_best),
      compressed_size_default(a_compressed_size_default),
      width(a_width),
      height(a_height),
      original_bit_depth(a_original_bit_depth),
      original_color_type(a_original_color_type),
      compressed_bit_depth(a_compressed_bit_depth),
      compressed_color_type(a_compressed_color_type) {
    }

 public:
  const char* filename;
  size_t original_size;
  size_t compressed_size_best;
  size_t compressed_size_default;
  int width;
  int height;
  int original_bit_depth;
  int original_color_type;
  int compressed_bit_depth;
  int compressed_color_type;
};

struct GoldImageCompressionInfo : public ImageCompressionInfo {
 public:
  GoldImageCompressionInfo(const char* a_filename,
                           size_t a_original_size,
                           size_t a_compressed_size_best,
                           size_t a_compressed_size_default,
                           int a_width,
                           int a_height,
                           int a_original_bit_depth,
                           int a_original_color_type,
                           int a_compressed_bit_depth,
                           int a_compressed_color_type,
                           bool a_transparency) :
      ImageCompressionInfo(a_filename,
                           a_original_size,
                           a_compressed_size_best,
                           a_compressed_size_default,
                           a_width,
                           a_height,
                           a_original_bit_depth,
                           a_original_color_type,
                           a_compressed_bit_depth,
                           a_compressed_color_type),
      transparency(a_transparency) {
  }

 public:
  bool transparency;
};

const GoldImageCompressionInfo kValidGifImages[] = {
  GoldImageCompressionInfo("basi0g01", 153, 166, 166, 32, 32, 8, 3, 1, 3,
                           false),
  GoldImageCompressionInfo("basi0g02", 185, 112, 112, 32, 32, 8, 3, 2, 3,
                           false),
  GoldImageCompressionInfo("basi0g04", 344, 144, 186, 32, 32, 8, 3, 4, 3,
                           false),
  GoldImageCompressionInfo("basi0g08", 1736, 116, 714, 32, 32, 8, 3, 8, 0,
                           false),
  GoldImageCompressionInfo("basi3p01", 138, 96, 96, 32, 32, 8, 3, 1, 3,
                           false),
  GoldImageCompressionInfo("basi3p02", 186, 115, 115, 32, 32, 8, 3, 2, 3,
                           false),
  GoldImageCompressionInfo("basi3p04", 344, 185, 185, 32, 32, 8, 3, 4, 3,
                           false),
  GoldImageCompressionInfo("basi3p08", 1737, 1270, 1270, 32, 32, 8, 3, 8, 3,
                           false),
  GoldImageCompressionInfo("basn0g01", 153, 166, 166, 32, 32, 8, 3, 1, 3,
                           false),
  GoldImageCompressionInfo("basn0g02", 185, 112, 112, 32, 32, 8, 3, 2, 3,
                           false),
  GoldImageCompressionInfo("basn0g04", 344, 144, 186, 32, 32, 8, 3, 4, 3,
                           false),
  GoldImageCompressionInfo("basn0g08", 1736, 116, 714, 32, 32, 8, 3, 8, 0,
                           false),
  GoldImageCompressionInfo("basn3p01", 138, 96, 96, 32, 32, 8, 3, 1, 3,
                           false),
  GoldImageCompressionInfo("basn3p02", 186, 115, 115, 32, 32, 8, 3, 2, 3,
                           false),
  GoldImageCompressionInfo("basn3p04", 344, 185, 185, 32, 32, 8, 3, 4, 3,
                           false),
  GoldImageCompressionInfo("basn3p08", 1737, 1270, 1270, 32, 32, 8, 3, 8, 3,
                           false),

  // These files have been transformed by rounding the original png
  // 8-bit alpha channel into a 1-bit alpha channel for gif.
  GoldImageCompressionInfo("tr-basi4a08", 467, 239, 316, 32, 32, 8, 3, 8, 3,
                           true),
  GoldImageCompressionInfo("tr-basn4a08", 467, 239, 316, 32, 32, 8, 3, 8, 3,
                           true),
};
const size_t kValidGifImageCount = arraysize(kValidGifImages);

bool ReadFile(const GoogleString& file_name,
              GoogleString* content);

bool ReadTestFile(const GoogleString& path,
                  const char* name,
                  const char* extension,
                  GoogleString* content);

bool ReadTestFileWithExt(const GoogleString& path,
                         const char* name_with_extension,
                         GoogleString* content);

void DecodeAndCompareImages(
    pagespeed::image_compression::ImageFormat image_format1,
    const void* image_buffer1,
    size_t buffer_length1,
    pagespeed::image_compression::ImageFormat image_format2,
    const void* image_buffer2,
    size_t buffer_length2,
    MessageHandler* message_handler);

void DecodeAndCompareImagesByPSNR(
    pagespeed::image_compression::ImageFormat image_format1,
    const void* image_buffer1,
    size_t buffer_length1,
    pagespeed::image_compression::ImageFormat image_format2,
    const void* image_buffer2,
    size_t buffer_length2,
    double min_psnr,
    MessageHandler* message_handler);

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_TEST_UTILS_H_
