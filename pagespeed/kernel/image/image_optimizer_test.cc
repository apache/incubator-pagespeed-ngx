/*
 * Copyright 2016 Google Inc.
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

// Author: huibao@google.com (Huibao Lin)

#include "pagespeed/kernel/image/image_optimizer.h"

#include <cstddef>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/image/gif_reader.h"
#include "pagespeed/kernel/image/image_analysis.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/image/test_utils.h"

namespace {

using net_instaweb::MockMessageHandler;
using net_instaweb::NullMutex;
using net_instaweb::Timer;
using pagespeed::image_compression::kGifTestDir;
using pagespeed::image_compression::kJpegTestDir;
using pagespeed::image_compression::kPngSuiteTestDir;
using pagespeed::image_compression::kPngSuiteGifTestDir;
using pagespeed::image_compression::kPngTestDir;
using pagespeed::image_compression::GifReader;
using pagespeed::image_compression::ImageDimensions;
using pagespeed::image_compression::ImageFormat;
using pagespeed::image_compression::ImageOptions;
using pagespeed::image_compression::IMAGE_GIF;
using pagespeed::image_compression::IMAGE_JPEG;
using pagespeed::image_compression::IMAGE_PNG;
using pagespeed::image_compression::IMAGE_UNKNOWN;
using pagespeed::image_compression::IMAGE_WEBP;
using pagespeed::image_compression::kMessagePatternLibpngError;
using pagespeed::image_compression::kMessagePatternLibpngWarning;
using pagespeed::image_compression::kMessagePatternPixelFormat;
using pagespeed::image_compression::kMessagePatternStats;
using pagespeed::image_compression::kMessagePatternUnexpectedEOF;
using pagespeed::image_compression::kMessagePatternWritingToWebp;
using pagespeed::image_compression::kTestRootDir;
using pagespeed::image_compression::ImageOptimizer;
using pagespeed::image_compression::ReadTestFileWithExt;

struct TestingImageInfo {
  const char* file_name;
  int width;
  int height;
  int rewritten_width;
  int rewritten_height;
  int original_size;
  int rewritten_png_or_jpeg_size;
};

// In kGifImages[], rewritten_width === 7 and rewritten_height === 8.
const TestingImageInfo kGifImages[] = {
  {"basi0g01.gif", 32, 32, 7, 8, 153, 166},
  {"basi0g02.gif", 32, 32, 7, 8, 185, 112},
  {"basi0g04.gif", 32, 32, 7, 8, 344, 144},
  {"basi0g08.gif", 32, 32, 7, 8, 1736, 116},
  {"basn0g01.gif", 32, 32, 7, 8, 153, 166},
  {"basn0g02.gif", 32, 32, 7, 8, 185, 112},
  {"basn0g04.gif", 32, 32, 7, 8, 344, 144},
  {"basn0g08.gif", 32, 32, 7, 8, 1736, 116},
  {"basi3p01.gif", 32, 32, 7, 8, 138, 96},
  {"basi3p02.gif", 32, 32, 7, 8, 186, 114},
  {"basi3p04.gif", 32, 32, 7, 8, 344, 155},
  {"basi3p08.gif", 32, 32, 7, 8, 1737, 495},
  {"basn3p01.gif", 32, 32, 7, 8, 138, 96},
  {"basn3p02.gif", 32, 32, 7, 8, 186, 114},
  {"basn3p04.gif", 32, 32, 7, 8, 344, 155},
  {"basn3p08.gif", 32, 32, 7, 8, 1737, 495},
};

// In kJpegImages[], the images were requested to be resized to height = 8.
const TestingImageInfo kJpegImages[] = {
  {"sjpeg1.jpg", 120, 90, 11, 8, 1552, 1165},
  {"sjpeg3.jpg", 512, 384, 11, 8, 44084, 25814},
  {"sjpeg6.jpg", 512, 512, 8, 8, 149600, 84641},
  {"testgray.jpg", 130, 97, 11, 8, 5014, 3060},
  {"sjpeg2.jpg", 130, 97, 11, 8, 3612, 3272},
  {"sjpeg4.jpg", 512, 384, 11, 8, 168895, 48717},
  {"test411.jpg", 130, 97, 11, 8, 6883, 3705},
  {"test420.jpg", 130, 97, 11, 8, 6173, 3653},
  {"test422.jpg", 130, 97, 11, 8, 6501, 3709},
};

const TestingImageInfo kAnimatedGifImages[] = {
  {"animated.gif", 120, 50, 120, 50, 4382, 0},
  {"animated_interlaced.gif", 120, 50, 120, 50, 3855, 0},
  {"full2loop.gif", 100, 200, 100, 200, 648, 0},
};

// In kPngImages[], the images were requested to be resized to width = 7.
const TestingImageInfo kPngImages[] = {
  {"basi0g01.png", 32, 32, 7, 7, 217, 166},
  {"basi0g02.png", 32, 32, 7, 7, 154, 105},
  {"basi0g04.png", 32, 32, 7, 7, 247, 137},
  {"basi0g08.png", 32, 32, 7, 7, 254, 116},
  {"basi0g16.png", 32, 32, 7, 7, 299, 122},
  {"basi2c08.png", 32, 32, 7, 7, 315, 129},
  {"basi2c16.png", 32, 32, 7, 7, 595, 216},
  {"basi3p01.png", 32, 32, 7, 7, 132, 96},
  {"basi3p02.png", 32, 32, 7, 7, 193, 114},
  {"basi3p04.png", 32, 32, 7, 7, 327, 155},
  {"basi4a08.png", 32, 32, 7, 7, 214, 105},
  {"basi4a16.png", 32, 32, 7, 7, 2855, 778},
  {"basi6a08.png", 32, 32, 7, 7, 361, 160},
  {"basi6a16.png", 32, 32, 7, 7, 4180, 1213},
  {"basn0g01.png", 32, 32, 7, 7, 164, 166},
  {"basn0g02.png", 32, 32, 7, 7, 104, 105},
  {"basn0g04.png", 32, 32, 7, 7, 145, 137},
  {"basn0g08.png", 32, 32, 7, 7, 138, 116},
  {"basn0g16.png", 32, 32, 7, 7, 167, 122},
  {"basn2c08.png", 32, 32, 7, 7, 145, 129},
  {"basn2c16.png", 32, 32, 7, 7, 302, 216},
  {"basn3p01.png", 32, 32, 7, 7, 112, 96},
  {"basn3p02.png", 32, 32, 7, 7, 146, 114},
  {"basn3p04.png", 32, 32, 7, 7, 216, 155},
  {"basn4a08.png", 32, 32, 7, 7, 126, 105},
  {"basn4a16.png", 32, 32, 7, 7, 2206, 778},
  {"basn6a08.png", 32, 32, 7, 7, 184, 160},
  {"basn6a16.png", 32, 32, 7, 7, 3435, 1213},
  {"bgai4a08.png", 32, 32, 7, 7, 214, 105},
  {"bgai4a16.png", 32, 32, 7, 7, 2855, 778},
  {"bgan6a08.png", 32, 32, 7, 7, 184, 160},
  {"bgan6a16.png", 32, 32, 7, 7, 3435, 1213},
  {"bgbn4a08.png", 32, 32, 7, 7, 140, 105},
  {"bggn4a16.png", 32, 32, 7, 7, 2220, 778},
  {"bgwn6a08.png", 32, 32, 7, 7, 202, 160},
  {"bgyn6a16.png", 32, 32, 7, 7, 3453, 1213},
  {"cdfn2c08.png", 8, 32, 7, 28, 404, 336},
  {"cdhn2c08.png", 32, 8, 7, 2, 344, 308},
  {"cdsn2c08.png", 8, 8, 7, 7, 232, 177},
  {"cdun2c08.png", 32, 32, 7, 7, 724, 666},
  {"ch1n3p04.png", 32, 32, 7, 7, 258, 155},
  {"cm0n0g04.png", 32, 32, 7, 7, 292, 272},
  {"cm7n0g04.png", 32, 32, 7, 7, 292, 272},
  {"cm9n0g04.png", 32, 32, 7, 7, 292, 272},
  {"cs3n2c16.png", 32, 32, 7, 7, 214, 142},
  {"cs3n3p08.png", 32, 32, 7, 7, 259, 142},
  {"cs5n2c08.png", 32, 32, 7, 7, 186, 148},
  {"cs5n3p08.png", 32, 32, 7, 7, 271, 148},
  {"cs8n2c08.png", 32, 32, 7, 7, 149, 142},
  {"cs8n3p08.png", 32, 32, 7, 7, 256, 142},
  {"ct0n0g04.png", 32, 32, 7, 7, 273, 272},
  {"ct1n0g04.png", 32, 32, 7, 7, 792, 272},
  {"ctzn0g04.png", 32, 32, 7, 7, 753, 272},
  {"f00n0g08.png", 32, 32, 7, 7, 319, 312},
  {"f01n0g08.png", 32, 32, 7, 7, 321, 246},
  {"f02n0g08.png", 32, 32, 7, 7, 355, 289},
  {"f03n0g08.png", 32, 32, 7, 7, 389, 292},
  {"f04n0g08.png", 32, 32, 7, 7, 269, 273},
  {"g03n0g16.png", 32, 32, 7, 7, 345, 257},
  {"g03n2c08.png", 32, 32, 7, 7, 370, 352},
  {"g03n3p04.png", 32, 32, 7, 7, 214, 189},
  {"g04n0g16.png", 32, 32, 7, 7, 363, 271},
  {"g04n2c08.png", 32, 32, 7, 7, 377, 358},
  {"g04n3p04.png", 32, 32, 7, 7, 219, 190},
  {"g05n0g16.png", 32, 32, 7, 7, 339, 259},
  {"g05n2c08.png", 32, 32, 7, 7, 350, 348},
  {"g05n3p04.png", 32, 32, 7, 7, 206, 181},
  {"g07n0g16.png", 32, 32, 7, 7, 321, 245},
  {"g07n2c08.png", 32, 32, 7, 7, 340, 352},
  {"g07n3p04.png", 32, 32, 7, 7, 207, 177},
  {"g10n0g16.png", 32, 32, 7, 7, 262, 194},
  {"g10n2c08.png", 32, 32, 7, 7, 285, 351},
  {"g10n3p04.png", 32, 32, 7, 7, 214, 188},
  {"g25n0g16.png", 32, 32, 7, 7, 383, 289},
  {"g25n2c08.png", 32, 32, 7, 7, 405, 350},
  {"g25n3p04.png", 32, 32, 7, 7, 215, 192},
  {"oi1n0g16.png", 32, 32, 7, 7, 167, 122},
  {"oi1n2c16.png", 32, 32, 7, 7, 302, 216},
  {"oi2n0g16.png", 32, 32, 7, 7, 179, 122},
  {"oi2n2c16.png", 32, 32, 7, 7, 314, 216},
  {"oi4n0g16.png", 32, 32, 7, 7, 203, 122},
  {"oi4n2c16.png", 32, 32, 7, 7, 338, 216},
  {"oi9n0g16.png", 32, 32, 7, 7, 1283, 122},
  {"oi9n2c16.png", 32, 32, 7, 7, 3038, 216},
  {"pp0n2c16.png", 32, 32, 7, 7, 962, 216},
  {"pp0n6a08.png", 32, 32, 7, 7, 818, 142},
  {"ps1n0g08.png", 32, 32, 7, 7, 1477, 116},
  {"ps1n2c16.png", 32, 32, 7, 7, 1641, 216},
  {"ps2n0g08.png", 32, 32, 7, 7, 2341, 116},
  {"ps2n2c16.png", 32, 32, 7, 7, 2505, 216},
  {"s01i3p01.png", 1, 1, 1, 1, 113, 69},
  {"s01n3p01.png", 1, 1, 1, 1, 113, 69},
  {"s02i3p01.png", 2, 2, 2, 2, 114, 72},
  {"s02n3p01.png", 2, 2, 2, 2, 115, 72},
  {"s03i3p01.png", 3, 3, 3, 3, 118, 77},
  {"s03n3p01.png", 3, 3, 3, 3, 120, 77},
  {"s04i3p01.png", 4, 4, 4, 4, 126, 79},
  {"s04n3p01.png", 4, 4, 4, 4, 121, 79},
  {"s05i3p02.png", 5, 5, 5, 5, 134, 86},
  {"s05n3p02.png", 5, 5, 5, 5, 129, 86},
  {"s06i3p02.png", 6, 6, 6, 6, 143, 86},
  {"s06n3p02.png", 6, 6, 6, 6, 131, 86},
  {"s07i3p02.png", 7, 7, 7, 7, 149, 94},
  {"s07n3p02.png", 7, 7, 7, 7, 138, 94},
  {"s08i3p02.png", 8, 8, 7, 7, 149, 99},
  {"s08n3p02.png", 8, 8, 7, 7, 139, 99},
  {"s09i3p02.png", 9, 9, 7, 7, 147, 102},
  {"s09n3p02.png", 9, 9, 7, 7, 143, 102},
  {"s32i3p04.png", 32, 32, 7, 7, 355, 213},
  {"s32n3p04.png", 32, 32, 7, 7, 263, 213},
  {"s33i3p04.png", 33, 33, 7, 7, 385, 250},
  {"s33n3p04.png", 33, 33, 7, 7, 329, 250},
  {"s34i3p04.png", 34, 34, 7, 7, 349, 205},
  {"s34n3p04.png", 34, 34, 7, 7, 248, 205},
  {"s35i3p04.png", 35, 35, 7, 7, 399, 257},
  {"s35n3p04.png", 35, 35, 7, 7, 338, 257},
  {"s36i3p04.png", 36, 36, 7, 7, 356, 205},
  {"s36n3p04.png", 36, 36, 7, 7, 258, 205},
  {"s37i3p04.png", 37, 37, 7, 7, 393, 246},
  {"s37n3p04.png", 37, 37, 7, 7, 336, 246},
  {"s38i3p04.png", 38, 38, 7, 7, 357, 200},
  {"s38n3p04.png", 38, 38, 7, 7, 245, 200},
  {"s39i3p04.png", 39, 39, 7, 7, 420, 269},
  {"s39n3p04.png", 39, 39, 7, 7, 352, 269},
  {"s40i3p04.png", 40, 40, 7, 7, 357, 220},
  {"s40n3p04.png", 40, 40, 7, 7, 256, 220},
  {"tbbn1g04.png", 32, 32, 7, 7, 419, 439},
  {"tbbn2c16.png", 32, 32, 7, 7, 1994, 1016},
  {"tbbn3p08.png", 32, 32, 7, 7, 1128, 1016},
  {"tbgn2c16.png", 32, 32, 7, 7, 1994, 1016},
  {"tbgn3p08.png", 32, 32, 7, 7, 1128, 1016},
  {"tbrn2c08.png", 32, 32, 7, 7, 1347, 1016},
  {"tbwn1g16.png", 32, 32, 7, 7, 1146, 890},
  {"tbwn3p08.png", 32, 32, 7, 7, 1131, 1016},
  {"tbyn3p08.png", 32, 32, 7, 7, 1131, 1016},
  {"tp0n1g08.png", 32, 32, 7, 7, 689, 552},
  {"tp1n3p08.png", 32, 32, 7, 7, 1115, 1016},
  {"z00n2c08.png", 32, 32, 7, 7, 3172, 224},
  {"z03n2c08.png", 32, 32, 7, 7, 232, 224},
  {"z06n2c08.png", 32, 32, 7, 7, 224, 224},
  {"z09n2c08.png", 32, 32, 7, 7, 224, 224},
  {"basi3p08.png", 32, 32, 7, 7, 1527, 495},
  {"basn3p08.png", 32, 32, 7, 7, 1286, 495},
  {"ccwn2c08.png", 32, 32, 7, 7, 1514, 1440},
  {"ccwn3p08.png", 32, 32, 7, 7, 1554, 1226},
  {"ch2n3p08.png", 32, 32, 7, 7, 1810, 495},
  {"f00n2c08.png", 32, 32, 7, 7, 2475, 1070},
  {"f01n2c08.png", 32, 32, 7, 7, 1180, 965},
  {"f02n2c08.png", 32, 32, 7, 7, 1729, 1024},
  {"f03n2c08.png", 32, 32, 7, 7, 1291, 1062},
  {"f04n2c08.png", 32, 32, 7, 7, 985, 985},
  {"tp0n2c08.png", 32, 32, 7, 7, 1311, 919},
  {"tp0n3p08.png", 32, 32, 7, 7, 1120, 919},
};

const TestingImageInfo kInvalidImages[] = {
  {"x00n0g01.png", 0, 0, 0, 0, 0, 0},
  {"xcrn0g04.png", 0, 0, 0, 0, 0, 0},
  {"xlfn0g04.png", 0, 0, 0, 0, 0, 0},
};

const size_t kGifImageCount = arraysize(kGifImages);
const size_t kPngImageCount = arraysize(kPngImages);
const size_t kJpegImageCount = arraysize(kJpegImages);
const size_t kAnimatedGifImageCount = arraysize(kAnimatedGifImages);
const size_t kInvalidImageCount = arraysize(kInvalidImages);

class ImageOptimizerTest : public testing::Test {
 public:
  ImageOptimizerTest()
      : message_handler_(new NullMutex) {
  }

 protected:
  void SetUp() override {
    message_handler_.AddPatternToSkipPrinting(kMessagePatternLibpngError);
    message_handler_.AddPatternToSkipPrinting(kMessagePatternLibpngWarning);
    message_handler_.AddPatternToSkipPrinting(kMessagePatternPixelFormat);
    message_handler_.AddPatternToSkipPrinting(kMessagePatternStats);
    message_handler_.AddPatternToSkipPrinting(kMessagePatternUnexpectedEOF);
    message_handler_.AddPatternToSkipPrinting(kMessagePatternWritingToWebp);
  }

  void RewriteAndVerifyImages(
      const char* dir, const TestingImageInfo* images, int num_images,
      bool is_animated, const ImageOptions& options,
      const ImageDimensions& requested_dimension,
      const ImageFormat original_format,
      bool expected_success,
      const ImageFormat expected_format,
      bool compare_to_rewritten_dimensions,
      bool expected_uses_lossy_format) {
    for (size_t i = 0; i < num_images; ++i) {
      // Load test image.
      const TestingImageInfo& image = images[i];
      GoogleString original_image;
      ASSERT_TRUE(ReadTestFileWithExt(dir, image.file_name, &original_image));

      // Try to optimize the image.
      GoogleString rewritten_image;
      ImageFormat rewritten_format;

      ImageOptimizer optimizer(&message_handler_);
      optimizer.set_options(options);
      optimizer.set_requested_dimension(requested_dimension);
      bool result = optimizer.Optimize(StringPiece(original_image),
                                       &rewritten_image, &rewritten_format);

      if (!expected_success) {
        EXPECT_FALSE(result);
        return;
      }

      ASSERT_TRUE(result) << " file: " << image.file_name;
      EXPECT_EQ(expected_format, rewritten_format)
          << " file: " << image.file_name;
      EXPECT_EQ(expected_uses_lossy_format, optimizer.UsesLossyFormat())
          << " file: " << image.file_name;

      if (compare_to_rewritten_dimensions) {
        EXPECT_EQ(image.rewritten_width, optimizer.optimized_width())
            << " file: " << image.file_name;
        EXPECT_EQ(image.rewritten_height, optimizer.optimized_height())
            << " file: " << image.file_name;
      } else {
        EXPECT_EQ(image.width, optimizer.optimized_width())
            << " file: " << image.file_name;
        EXPECT_EQ(image.height, optimizer.optimized_height())
            << " file: " << image.file_name;

        if (expected_format != IMAGE_WEBP) {
          int expected_rewritten_size = image.rewritten_png_or_jpeg_size;
          // If the image was not resized, we expect the rewritten
          // image to have the predicted size. In considering version
          // difference of encoder, the rewritten size is compared with
          // a window.
          const int threshold = 20;
          EXPECT_LE(expected_rewritten_size - threshold,
                    rewritten_image.length())
              << " file: " << image.file_name;
          EXPECT_GE(expected_rewritten_size + threshold,
                    rewritten_image.length())
              << " file: " << image.file_name;
        }
      }

      if (!is_animated) {
        int rewritten_width = -1;
        int rewritten_height = -1;
        EXPECT_TRUE(
            AnalyzeImage(rewritten_format, rewritten_image.data(),
                         rewritten_image.length(), &rewritten_width,
                         &rewritten_height, nullptr,  nullptr,
                         nullptr,  nullptr,  nullptr,  nullptr,
                         &message_handler_));
        EXPECT_EQ(optimizer.optimized_width(), rewritten_width);
        EXPECT_EQ(optimizer.optimized_height(), rewritten_height);

        // If the original and rewritten images have the same dimensions,
        // we verify that they match pixel by pixel. If they have different
        // dimension, we only verify the rewritten dimension. Pixel accuracy
        // of the resized image is tested in image_resizer_test.cc.
        if (!compare_to_rewritten_dimensions) {
          if (original_format == IMAGE_JPEG) {
            double min_psnr = 32.0;
            DecodeAndCompareImagesByPSNR(
                original_format, original_image.data(), original_image.length(),
                rewritten_format, rewritten_image.data(),
                rewritten_image.length(), min_psnr,
                true,  // expand colors
                true,  // ignore transparent pixels
                &message_handler_);
          } else {
            // For lossless formats (GIF and PNG) the pixels must match
            // bit-by-bit.
            DecodeAndCompareImages(
                original_format, original_image.data(), original_image.length(),
                rewritten_format, rewritten_image.data(),
                rewritten_image.length(),
                true,  // ignore transparent pixels
                &message_handler_);
          }
        }
      } else {
        EXPECT_TRUE(
            pagespeed::image_compression::CompareAnimatedImages(
                net_instaweb::StrCat(dir, images[i].file_name),
                rewritten_image, &message_handler_));
      }
    }
  }

 protected:
  MockMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageOptimizerTest);
};

// Test converting PNG, JPEG, and GIF (single-frame and animated) to WebP.
TEST_F(ImageOptimizerTest, AllFormats) {
  ImageOptions options;
  options.set_allow_webp_lossless_or_alpha(true);
  options.set_allow_webp_animated(true);
  options.set_must_reduce_bytes(false);
  ImageDimensions requested_dimension;
  bool is_animated = false;
  bool expected_success = true;
  bool compare_to_rewritten_dimensions = false;
  bool expected_uses_lossy_format = false;

  RewriteAndVerifyImages(kPngSuiteGifTestDir, kGifImages, kGifImageCount,
                         is_animated, options, requested_dimension,
                         IMAGE_GIF, expected_success, IMAGE_WEBP,
                         compare_to_rewritten_dimensions,
                         expected_uses_lossy_format);

  RewriteAndVerifyImages(kPngSuiteTestDir, kPngImages, kPngImageCount,
                         is_animated, options, requested_dimension,
                         IMAGE_PNG, expected_success, IMAGE_WEBP,
                         compare_to_rewritten_dimensions,
                         expected_uses_lossy_format);

  expected_uses_lossy_format = true;
  RewriteAndVerifyImages(kJpegTestDir, kJpegImages, kJpegImageCount,
                         is_animated, options, requested_dimension,
                         IMAGE_JPEG, expected_success, IMAGE_WEBP,
                         compare_to_rewritten_dimensions,
                         expected_uses_lossy_format);

  is_animated = true;
  expected_uses_lossy_format = false;
  RewriteAndVerifyImages(kGifTestDir, kAnimatedGifImages,
                         kAnimatedGifImageCount, is_animated, options,
                         requested_dimension, IMAGE_GIF, expected_success,
                         IMAGE_WEBP, compare_to_rewritten_dimensions,
                         expected_uses_lossy_format);
}

// Test converting single-frame GIF to PNG.
TEST_F(ImageOptimizerTest, GifToPng) {
  ImageOptions options;
  options.set_must_reduce_bytes(false);
  ImageDimensions requested_dimension;
  bool is_animated = false;
  bool expected_success = true;
  bool compare_to_rewritten_dimensions = false;
  bool expected_uses_lossy_format = false;

  RewriteAndVerifyImages(kPngSuiteGifTestDir, kGifImages, kGifImageCount,
                         is_animated, options, requested_dimension,
                         IMAGE_GIF, expected_success, IMAGE_PNG,
                         compare_to_rewritten_dimensions,
                         expected_uses_lossy_format);
}

// Test recompressing PNG.
TEST_F(ImageOptimizerTest, PngToPng) {
  ImageOptions options;
  options.set_must_reduce_bytes(false);
  ImageDimensions requested_dimension;
  bool is_animated = false;
  bool expected_success = true;
  bool compare_to_rewritten_dimensions = false;
  bool expected_uses_lossy_format = false;

  RewriteAndVerifyImages(kPngSuiteTestDir, kPngImages, kPngImageCount,
                         is_animated, options, requested_dimension,
                         IMAGE_PNG, expected_success, IMAGE_PNG,
                         compare_to_rewritten_dimensions,
                         expected_uses_lossy_format);
}

// Test recompressing JPEG.
TEST_F(ImageOptimizerTest, JpegToJpeg) {
  ImageOptions options;
  options.set_allow_webp_lossy(false);
  options.set_must_reduce_bytes(false);
  ImageDimensions requested_dimension;
  bool is_animated = false;
  bool expected_success = true;
  bool compare_to_rewritten_dimensions = false;
  bool expected_uses_lossy_format = true;

  RewriteAndVerifyImages(kJpegTestDir, kJpegImages, kJpegImageCount,
                         is_animated, options, requested_dimension,
                         IMAGE_JPEG, expected_success, IMAGE_JPEG,
                         compare_to_rewritten_dimensions,
                         expected_uses_lossy_format);
}

// Test resizing and compressing PNG, JPEG, and single-frame GIF.
TEST_F(ImageOptimizerTest, Resize) {
  ImageOptions options;
  options.set_allow_webp_lossy(false);
  options.set_must_reduce_bytes(false);
  ImageDimensions requested_dimension;
  bool is_animated = false;
  bool expected_success = true;
  bool compare_to_rewritten_dimensions = true;
  bool expected_uses_lossy_format = false;

  requested_dimension.set_width(7);
  requested_dimension.set_height(8);
  RewriteAndVerifyImages(kPngSuiteGifTestDir, kGifImages, kGifImageCount,
                         is_animated, options, requested_dimension,
                         IMAGE_GIF, expected_success, IMAGE_PNG,
                         compare_to_rewritten_dimensions,
                         expected_uses_lossy_format);

  requested_dimension.set_width(7);
  requested_dimension.clear_height();
  RewriteAndVerifyImages(kPngSuiteTestDir, kPngImages, kPngImageCount,
                         is_animated, options, requested_dimension,
                         IMAGE_PNG, expected_success, IMAGE_PNG,
                         compare_to_rewritten_dimensions,
                         expected_uses_lossy_format);

  requested_dimension.clear_width();
  requested_dimension.set_height(8);
  expected_uses_lossy_format = true;
  RewriteAndVerifyImages(kJpegTestDir, kJpegImages, kJpegImageCount,
                         is_animated, options, requested_dimension,
                         IMAGE_JPEG, expected_success, IMAGE_JPEG,
                         compare_to_rewritten_dimensions,
                         expected_uses_lossy_format);
}

// Test un-resizeable images, e.g., requested dimension is larger than the
// original or unsupported format (i.e., animated image).
TEST_F(ImageOptimizerTest, NotResize) {
  ImageOptions options;
  options.set_allow_webp_lossless_or_alpha(true);
  options.set_allow_webp_animated(true);
  options.set_must_reduce_bytes(false);
  ImageDimensions requested_dimension;
  bool is_animated = false;
  bool expected_success = true;
  bool compare_to_rewritten_dimensions = false;
  bool expected_uses_lossy_format = false;

  // Both dimensions are too large.
  requested_dimension.set_width(1000000);
  requested_dimension.set_height(1000000);
  RewriteAndVerifyImages(kPngSuiteGifTestDir, kGifImages, kGifImageCount,
                         is_animated, options, requested_dimension,
                         IMAGE_GIF, expected_success, IMAGE_WEBP,
                         compare_to_rewritten_dimensions,
                         expected_uses_lossy_format);

  // Width too large.
  requested_dimension.set_width(1000000);
  requested_dimension.clear_height();
  RewriteAndVerifyImages(kPngSuiteTestDir, kPngImages, kPngImageCount,
                         is_animated, options, requested_dimension,
                         IMAGE_PNG, expected_success, IMAGE_WEBP,
                         compare_to_rewritten_dimensions,
                         expected_uses_lossy_format);

  // Height too large.
  requested_dimension.clear_width();
  requested_dimension.set_height(1000000);
  expected_uses_lossy_format = true;
  RewriteAndVerifyImages(kJpegTestDir, kJpegImages, kJpegImageCount,
                         is_animated, options, requested_dimension,
                         IMAGE_JPEG, expected_success, IMAGE_WEBP,
                         compare_to_rewritten_dimensions,
                         expected_uses_lossy_format);

  // Animated images can't be resized currently.
  requested_dimension.set_width(7);
  requested_dimension.set_height(8);
  expected_uses_lossy_format = false;
  is_animated = true;
  RewriteAndVerifyImages(kGifTestDir, kAnimatedGifImages,
                         kAnimatedGifImageCount, is_animated, options,
                         requested_dimension, IMAGE_GIF, expected_success,
                         IMAGE_WEBP, compare_to_rewritten_dimensions,
                         expected_uses_lossy_format);
}

// Test invalid images.
TEST_F(ImageOptimizerTest, InvalidImages) {
  ImageOptions options;
  ImageDimensions requested_dimension;
  bool is_animated = false;
  bool expected_success = false;
  bool compare_to_rewritten_dimensions = false;
  bool expected_uses_lossy_format = false;

  RewriteAndVerifyImages(kPngSuiteTestDir, kInvalidImages, kInvalidImageCount,
                         is_animated, options, requested_dimension,
                         IMAGE_PNG, expected_success, IMAGE_UNKNOWN,
                         compare_to_rewritten_dimensions,
                         expected_uses_lossy_format);
}

// Make sure that an ImageOptimizer object can only be used once.
TEST_F(ImageOptimizerTest, SingleUse) {
  GoogleString original_image;
  ASSERT_TRUE(ReadTestFileWithExt(kPngSuiteTestDir, "basi0g01.png",
                                  &original_image));

  // Try to optimize the image.
  GoogleString rewritten_image;
  ImageFormat rewritten_format;
  ImageOptimizer optimizer(&message_handler_);
  ASSERT_TRUE(optimizer.Optimize(StringPiece(original_image),
                                 &rewritten_image, &rewritten_format));
  EXPECT_DEATH(optimizer.Optimize(StringPiece(original_image),
                                  &rewritten_image, &rewritten_format),
               "Check failed: is_valid_");
}

// Make sure that we return false if "must_reduce_bytes" was set to true and
// we can't make the image smaller.
TEST_F(ImageOptimizerTest, MustReduceBytes) {
  GoogleString original_image;
  GoogleString rewritten_image;
  ImageFormat rewritten_format;
  ImageOptimizer optimizer(&message_handler_);

  // o.gif is a well-optimized GIF image with only 43 bytes. We can't improve
  // this image.
  ASSERT_TRUE(ReadTestFileWithExt(kGifTestDir, "o.gif", &original_image));
  ASSERT_FALSE(optimizer.Optimize(StringPiece(original_image),
                                  &rewritten_image, &rewritten_format));
}

}  // namespace
