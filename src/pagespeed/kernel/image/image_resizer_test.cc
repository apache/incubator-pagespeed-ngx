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

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/image/image_resizer.h"
#include "pagespeed/kernel/image/jpeg_optimizer.h"
#include "pagespeed/kernel/image/png_optimizer.h"
#include "pagespeed/kernel/image/read_image.h"
#include "pagespeed/kernel/image/test_utils.h"
#include "pagespeed/kernel/image/webp_optimizer.h"

namespace {

// Pixel formats
using net_instaweb::MessageHandler;
using net_instaweb::MockMessageHandler;
using net_instaweb::NullMutex;
using pagespeed::image_compression::PixelFormat;
using pagespeed::image_compression::GRAY_8;
using pagespeed::image_compression::RGB_888;
using pagespeed::image_compression::RGBA_8888;
// Readers and writers
using pagespeed::image_compression::JpegCompressionOptions;
using pagespeed::image_compression::JpegScanlineWriter;
using pagespeed::image_compression::kPngSuiteTestDir;
using pagespeed::image_compression::kPngTestDir;
using pagespeed::image_compression::kResizedTestDir;
using pagespeed::image_compression::PngScanlineReaderRaw;
using pagespeed::image_compression::ReadTestFile;
using pagespeed::image_compression::ScanlineResizer;
using pagespeed::image_compression::ScanlineWriterInterface;
using pagespeed::image_compression::WebpConfiguration;
using pagespeed::image_compression::kMessagePatternPixelFormat;
using pagespeed::image_compression::kMessagePatternStats;
using pagespeed::image_compression::kMessagePatternUnexpectedEOF;
using pagespeed::image_compression::kMessagePatternWritingToWebp;

const size_t kPreserveAspectRatio =
    pagespeed::image_compression::ScanlineResizer::kPreserveAspectRatio;

// Three testing images: GRAY_8, RGB_888, and RGBA_8888.
// Size of these images is 32-by-32 pixels.
const char* kValidImages[] = {
  "basi0g04",
  "basi3p02",
  "basn6a16",
};

const char kImagePagespeed[] = "pagespeed-128";

// Size of the output image [width, height]. The size of the input image
// is 32-by-32. We would like to test resizing ratios of both integers
// and non-integers.
const size_t kOutputSize[][2] = {
  {16, kPreserveAspectRatio},   // Shrink image by 2 times in both directions.
  {kPreserveAspectRatio, 8},    // Shrink image by 4 times in both directions.
  {3, 3},    // Shrink image by 32/3 times in both directions.
  {16, 25},  // Shrink image by [2, 32/25] times.
  {32, 5},   // Shrink image by [1, 32/5] times.
  {3, 32},   // Shrink image by [32/3, 1] times.
  {31, 31},  // Shrink image by [32/31, 32/31] times.
  {32, 32},  // Although the image did not shrink, the algorithm is exercised.
};

const size_t kValidImageCount = arraysize(kValidImages);
const size_t KOutputSizeCount = arraysize(kOutputSize);

class ScanlineResizerTest : public testing::Test {
 public:
  ScanlineResizerTest() :
      message_handler_(new NullMutex),
      reader_(&message_handler_),
      resizer_(&message_handler_),
      scanline_(NULL) {
  }

 protected:
  void InitializeReader(const char* file_name) {
    ASSERT_TRUE(ReadTestFile(kPngSuiteTestDir, file_name, "png",
                             &input_image_));
    ASSERT_TRUE(reader_.Initialize(input_image_.data(),
                                   input_image_.length()));
  }

  MockMessageHandler message_handler_;
  PngScanlineReaderRaw reader_;
  ScanlineResizer resizer_;
  GoogleString input_image_;
  void* scanline_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScanlineResizerTest);
};

// Read the gold file. Size of the gold file is embedded in its name.
// For example 'testdata/resized/basi0g04_w16_h16.png'.
bool ReadGoldImageToString(const char* file_name,
                           size_t width,
                           size_t height,
                           GoogleString* image_data) {
  GoogleString gold_file_name = StringPrintf("%s_w%d_h%d", file_name,
      static_cast<int>(width), static_cast<int>(height));
  return ReadTestFile(kResizedTestDir, gold_file_name.c_str(), "png",
                      image_data);
}

// Return JPEG writer for Gray_8, or WebP writer for RGB_888 or RGBA_8888.
ScanlineWriterInterface* CreateWriter(PixelFormat pixel_format,
                                      size_t width,
                                      size_t height,
                                      GoogleString* image_data,
                                      GoogleString* file_ext,
                                      MessageHandler* handler) {
  if (pixel_format == GRAY_8) {
    *file_ext = "jpg";
    JpegCompressionOptions jpeg_config;
    jpeg_config.lossy = true;
    jpeg_config.lossy_options.quality = 100;
    return reinterpret_cast<ScanlineWriterInterface*>(
        CreateScanlineWriter(pagespeed::image_compression::IMAGE_JPEG,
                             pixel_format, width, height, &jpeg_config,
                             image_data, handler));
  } else {
    *file_ext = "webp";
    WebpConfiguration webp_config;  // Use lossless by default
    return reinterpret_cast<ScanlineWriterInterface*>(
        CreateScanlineWriter(pagespeed::image_compression::IMAGE_WEBP,
                             pixel_format, width, height, &webp_config,
                             image_data, handler));
  }
}

// Make sure the resized results, include image size, pixel format, and pixel
// values, match the gold data. The gold data has the image size coded in the
// file name. For example, an image resized to 16-by-16 is
// third_party/pagespeed/kernel/image/testdata/resized/basi0g04_w16_h16.png
TEST_F(ScanlineResizerTest, Accuracy) {
  PngScanlineReaderRaw gold_reader(&message_handler_);

  for (size_t index_image = 0;
       index_image < kValidImageCount;
       ++index_image) {
    const char* file_name = kValidImages[index_image];
    ASSERT_TRUE(ReadTestFile(kPngSuiteTestDir, file_name, "png",
                             &input_image_));

    for (size_t index_size = 0; index_size < KOutputSizeCount; ++index_size) {
      size_t width = kOutputSize[index_size][0];
      size_t height = kOutputSize[index_size][1];
      ASSERT_TRUE(reader_.Initialize(input_image_.data(),
                                     input_image_.length()));
      ASSERT_TRUE(resizer_.Initialize(&reader_, width, height));

      if (width == 0) width = height;
      if (height == 0) height = width;
      GoogleString gold_image;
      ASSERT_TRUE(ReadGoldImageToString(file_name, width, height, &gold_image));
      ASSERT_TRUE(gold_reader.Initialize(gold_image.data(),
                                         gold_image.length()));

      // Make sure the images sizes and the pixel formats are the same.
      ASSERT_EQ(gold_reader.GetImageWidth(), resizer_.GetImageWidth());
      ASSERT_EQ(gold_reader.GetImageHeight(), resizer_.GetImageHeight());
      ASSERT_EQ(gold_reader.GetPixelFormat(), resizer_.GetPixelFormat());

      while (resizer_.HasMoreScanLines() && gold_reader.HasMoreScanLines()) {
        uint8* resized_scanline = NULL;
        uint8* gold_scanline = NULL;
        ASSERT_TRUE(resizer_.ReadNextScanline(
            reinterpret_cast<void**>(&resized_scanline)));
        ASSERT_TRUE(gold_reader.ReadNextScanline(
            reinterpret_cast<void**>(&gold_scanline)));

        for (size_t i = 0; i < resizer_.GetBytesPerScanline(); ++i) {
          ASSERT_EQ(gold_scanline[i], resized_scanline[i]);
        }
      }

      // Make sure both the resizer and the reader have exhausted scanlines.
      ASSERT_FALSE(resizer_.HasMoreScanLines());
      ASSERT_FALSE(gold_reader.HasMoreScanLines());
    }
  }
}

// Resize the image and write the result to a JPEG or a WebP image.
TEST_F(ScanlineResizerTest, ResizeAndWrite) {
  message_handler_.AddPatternToSkipPrinting(kMessagePatternPixelFormat);
  message_handler_.AddPatternToSkipPrinting(kMessagePatternStats);
  message_handler_.AddPatternToSkipPrinting(kMessagePatternWritingToWebp);
  for (size_t index_image = 0; index_image < kValidImageCount; ++index_image) {
    const char* file_name = kValidImages[index_image];
    ASSERT_TRUE(ReadTestFile(kPngSuiteTestDir, file_name, "png",
                             &input_image_));

    for (size_t index_size = 0; index_size < KOutputSizeCount; ++index_size) {
      const size_t width = kOutputSize[index_size][0];
      const size_t height = kOutputSize[index_size][1];
      ASSERT_TRUE(reader_.Initialize(input_image_.data(),
                                     input_image_.length()));
      ASSERT_TRUE(resizer_.Initialize(&reader_, width, height));

      GoogleString output_image;
      GoogleString file_ext;
      scoped_ptr<ScanlineWriterInterface> writer(
         CreateWriter(resizer_.GetPixelFormat(),
                      resizer_.GetImageWidth(),
                      resizer_.GetImageHeight(),
                      &output_image,
                      &file_ext,
                      &message_handler_));

      while (resizer_.HasMoreScanLines()) {
        ASSERT_TRUE(resizer_.ReadNextScanline(&scanline_));
        ASSERT_TRUE(writer->WriteNextScanline(scanline_));
      }

      ASSERT_TRUE(writer->FinalizeWrite());
    }
  }
}

// Both width and height are specified.
TEST_F(ScanlineResizerTest, InitializeWidthHeight) {
  size_t width = 20;
  size_t height = 10;
  InitializeReader(kValidImages[0]);
  ASSERT_TRUE(resizer_.Initialize(&reader_, width, height));
  EXPECT_EQ(width, resizer_.GetImageWidth());
  EXPECT_EQ(height, resizer_.GetImageHeight());
  EXPECT_EQ(reader_.GetPixelFormat(), resizer_.GetPixelFormat());
}

// Only height is specified.
TEST_F(ScanlineResizerTest, InitializeHeight) {
  size_t width = kPreserveAspectRatio;
  size_t height = 10;
  InitializeReader(kValidImages[0]);
  ASSERT_TRUE(resizer_.Initialize(&reader_, width, height));
  EXPECT_EQ(height, resizer_.GetImageWidth());
  EXPECT_EQ(height, resizer_.GetImageHeight());
}

// Only width is specified.
TEST_F(ScanlineResizerTest, InitializeWidth) {
  size_t width = 12;
  size_t height = kPreserveAspectRatio;
  InitializeReader(kValidImages[0]);
  ASSERT_TRUE(resizer_.Initialize(&reader_, width, height));
  EXPECT_EQ(width, resizer_.GetImageWidth());
  EXPECT_EQ(width, resizer_.GetImageHeight());
}

// The resizer is not initialized, so ReadNextScanline returns false.
TEST_F(ScanlineResizerTest, ReadNullScanline) {
#ifndef NDEBUG
  ASSERT_DEATH(resizer_.ReadNextScanline(&scanline_),
               "SCANLINE_RESIZER/SCANLINE_STATUS_INVOCATION_ERROR "
               "null reader or no more scanlines");
#else
  ASSERT_FALSE(resizer_.ReadNextScanline(&scanline_));
#endif
}

// The resizer has only one scanline, so ReadNextScanline returns false
// at the second call.
TEST_F(ScanlineResizerTest, ReadNextScanline) {
  InitializeReader(kValidImages[1]);
  ASSERT_TRUE(resizer_.Initialize(&reader_, 10, 1));
  ASSERT_TRUE(resizer_.ReadNextScanline(&scanline_));
#ifndef NDEBUG
  ASSERT_DEATH(resizer_.ReadNextScanline(&scanline_),
               "SCANLINE_RESIZER/SCANLINE_STATUS_INVOCATION_ERROR "
               "null reader or no more scanlines");
#else
  ASSERT_FALSE(resizer_.ReadNextScanline(&scanline_));
#endif
}

// The original image is truncated. Only 100 bytes are passed to the reader.
// The reader is able decode the image header, but not the pixels.
// The resizer should return false when the reader failes.
TEST_F(ScanlineResizerTest, BadReader) {
  message_handler_.AddPatternToSkipPrinting(kMessagePatternUnexpectedEOF);
  ASSERT_TRUE(ReadTestFile(kPngSuiteTestDir, kValidImages[0], "png",
                           &input_image_));
  ASSERT_TRUE(reader_.Initialize(input_image_.data(), 100));
  ASSERT_TRUE(resizer_.Initialize(&reader_, 10, 20));
  ASSERT_FALSE(resizer_.ReadNextScanline(&scanline_));
}

// The resizer is initialized twice and only a portion of scanlines are readed.
// The resizer should not have any error.
TEST_F(ScanlineResizerTest, PartialRead) {
  InitializeReader(kValidImages[0]);

  ASSERT_TRUE(resizer_.Initialize(&reader_, 10, 20));
  // Read only 1 scanline, although there are 20.
  EXPECT_TRUE(resizer_.ReadNextScanline(&scanline_));

  ASSERT_TRUE(resizer_.Initialize(&reader_, 10, 20));
  // Read only 2 scanlines, although there are 20.
  EXPECT_TRUE(resizer_.ReadNextScanline(&scanline_));
  EXPECT_TRUE(resizer_.ReadNextScanline(&scanline_));
}

// Resize the image by non-integer ratios.
TEST_F(ScanlineResizerTest, ResizeFractionalRatio) {
  const int new_width = 11;
  const int new_height = 19;
  ASSERT_TRUE(ReadTestFile(kPngTestDir, kImagePagespeed, "png",
                           &input_image_));

  ASSERT_TRUE(reader_.Initialize(input_image_.data(),
                                 input_image_.length()));
  ASSERT_TRUE(resizer_.Initialize(&reader_, new_width, new_height));

  int num_rows = 0;
  while (resizer_.HasMoreScanLines()) {
    ASSERT_TRUE(resizer_.ReadNextScanline(&scanline_));
    ++num_rows;
  }
  EXPECT_EQ(new_height, num_rows);
}

}  // namespace
