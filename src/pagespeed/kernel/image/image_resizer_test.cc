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

#include <setjmp.h>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/image/image_resizer.h"
#include "pagespeed/kernel/image/jpeg_optimizer.h"
#include "pagespeed/kernel/image/png_optimizer.h"
#include "pagespeed/kernel/image/test_utils.h"
#include "pagespeed/kernel/image/webp_optimizer.h"

namespace {

// Pixel formats
using pagespeed::image_compression::PixelFormat;
using pagespeed::image_compression::GRAY_8;
using pagespeed::image_compression::RGB_888;
using pagespeed::image_compression::RGBA_8888;
// Readers and writers
using pagespeed::image_compression::JpegCompressionOptions;
using pagespeed::image_compression::JpegScanlineWriter;
using pagespeed::image_compression::kPngSuiteTestDir;
using pagespeed::image_compression::kResizedTestDir;
using pagespeed::image_compression::PngScanlineReaderRaw;
using pagespeed::image_compression::ReadTestFile;
using pagespeed::image_compression::ScanlineResizer;
using pagespeed::image_compression::ScanlineWriterInterface;
using pagespeed::image_compression::WebpConfiguration;
using pagespeed::image_compression::WebpScanlineWriter;

const size_t kPreserveAspectRatio =
    pagespeed::image_compression::ScanlineResizer::kPreserveAspectRatio;

// Three testing images: GRAY_8, RGB_888, and RGBA_8888.
// Size of these images is 32-by-32 pixels.
const char* kValidImages[] = {
  "basi0g04",
  "basi3p02",
  "basn6a16",
};

// Size of the output image [width, height]. The size of the input image
// is 32-by-32. We would like to test resizing ratioes of both integers
// and non-integers.
const size_t kOutputSize[][2] = {
  {16, kPreserveAspectRatio},   // Shrink image by 2 times in both directions.
  {kPreserveAspectRatio, 8},    // Shrink image by 4 times in both directions.
  {3, 3},    // Shrink image by 32/3 times in both directions.
  {16, 25},  // Shrink image by [2, 32/25] times.
  {32, 5},   // Shrink image by [1, 32/5] times.
  {32, 32},  // Although the image is not shrinked, the algorithm is exercised.
};

const size_t kValidImageCount = arraysize(kValidImages);
const size_t KOutputSizeCount = arraysize(kOutputSize);

class ScanlineResizerTest : public testing::Test {
 public:
  ScanlineResizerTest() :
      scanline_(NULL) {
  }

 protected:
  void InitializeReader(const char* file_name) {
    ASSERT_TRUE(ReadTestFile(kPngSuiteTestDir, file_name, "png",
                             &input_image_));
    ASSERT_TRUE(reader_.Initialize(input_image_.data(),
                                   input_image_.length()));
  }

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
  GoogleString gold_file_name = StringPrintf("%s_w%ld_h%ld", file_name,
                                             width, height);
  return ReadTestFile(kResizedTestDir, gold_file_name.c_str(), "png",
                      image_data);
}

// Return JPEG writer for Gray_8, or WebP writer for RGB_888 or RGBA_8888.
ScanlineWriterInterface* CreateWriter(PixelFormat pixel_format,
                                      size_t width,
                                      size_t height,
                                      GoogleString* image_data,
                                      GoogleString* file_ext) {
  if (pixel_format == GRAY_8) {
    JpegScanlineWriter* jpeg_writer = new JpegScanlineWriter();
    if (jpeg_writer != NULL) {
      JpegCompressionOptions jpeg_options;
      jpeg_options.lossy = true;
      jpeg_options.lossy_options.quality = 100;

      jmp_buf env;
      if (setjmp(env)) {
        // This code is run only when libjpeg hit an error, and called
        // longjmp(env).
        jpeg_writer->AbortWrite();
      } else {
        jpeg_writer->SetJmpBufEnv(&env);
        jpeg_writer->Init(width, height, pixel_format);
        jpeg_writer->SetJpegCompressParams(jpeg_options);
        jpeg_writer->InitializeWrite(image_data);
      }
      *file_ext = "jpg";
    }
    return reinterpret_cast<ScanlineWriterInterface*>(jpeg_writer);
  } else {
    WebpScanlineWriter* webp_writer = new WebpScanlineWriter();
    if (webp_writer != NULL) {
      WebpConfiguration webp_config;  // Use lossless by default
      webp_writer->Init(width, height, pixel_format);
      webp_writer->InitializeWrite(webp_config, image_data);
      *file_ext = "webp";
    }
    return reinterpret_cast<ScanlineWriterInterface*>(webp_writer);
  }
}

// Make sure the resized results, include image size, pixel format, and pixel
// values, match the gold data. The gold data has the image size coded in the
// file name. For example, an image resized to 16-by-16 is
// third_party/pagespeed/kernel/image/testdata/resized/basi0g04_w16_h16.png
TEST_F(ScanlineResizerTest, Accuracy) {
  PngScanlineReaderRaw gold_reader;

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
                      &file_ext));

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
  ASSERT_FALSE(resizer_.ReadNextScanline(&scanline_));
}

// The resizer has only one scanline, so ReadNextScanline returns false
// at the second call.
TEST_F(ScanlineResizerTest, ReadNextScanline) {
  InitializeReader(kValidImages[1]);
  ASSERT_TRUE(resizer_.Initialize(&reader_, 10, 1));
  ASSERT_TRUE(resizer_.ReadNextScanline(&scanline_));
  ASSERT_FALSE(resizer_.ReadNextScanline(&scanline_));
}

// The original image is truncated. Only 100 bytes are passed to the reader.
// The reader is able decode the image header, but not the pixels.
// The resizer should return false when the reader failes.
TEST_F(ScanlineResizerTest, BadReader) {
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

}  // namespace
