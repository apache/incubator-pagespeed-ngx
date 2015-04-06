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

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/jpeg_reader.h"
#include "pagespeed/kernel/image/read_image.h"
#include "pagespeed/kernel/image/test_utils.h"

// DO NOT INCLUDE LIBJPEG HEADERS HERE. Doing so causes build errors
// on Windows. If you need to call out to libjpeg, please add helper
// methods in jpeg_optimizer_test_helper.h.

namespace {

using net_instaweb::MockMessageHandler;
using net_instaweb::NullMutex;
using pagespeed::image_compression::IMAGE_JPEG;
using pagespeed::image_compression::IMAGE_PNG;
using pagespeed::image_compression::JpegScanlineReader;
using pagespeed::image_compression::kJpegTestDir;
using pagespeed::image_compression::kMessagePatternLibJpegFailure;
using pagespeed::image_compression::ReadImage;
using pagespeed::image_compression::ReadTestFile;
using pagespeed::image_compression::ReadTestFileWithExt;

const char* kValidJpegImages[] = {
  "test411",        // RGB color space with 4:1:1 chroma sub-sampling.
  "test420",        // RGB color space with 4:2:0 chroma sub-sampling.
  "test422",        // RGB color space with 4:2:2 chroma sub-sampling.
  "test444",        // RGB color space with full chroma information.
  "testgray",       // Grayscale color space.
};

const char *kInvalidFiles[] = {
  "notajpeg.png",   // A png.
  "notajpeg.gif",   // A gif.
  "emptyfile.jpg",  // A zero-byte file.
  "corrupt.jpg",    // Invalid huffman code in the image data section.
};

const size_t kValidJpegImageCount = arraysize(kValidJpegImages);
const size_t kInvalidFileCount = arraysize(kInvalidFiles);

// Verify that decoded image is accurate for each pixel.
TEST(JpegReaderTest, ValidJpegs) {
  MockMessageHandler message_handler(new NullMutex);
  for (size_t i = 0; i < kValidJpegImageCount; ++i) {
    GoogleString jpeg_image, png_image;
    ReadTestFile(kJpegTestDir, kValidJpegImages[i], "jpg", &jpeg_image);
    ReadTestFile(kJpegTestDir, kValidJpegImages[i], "png", &png_image);
    DecodeAndCompareImages(IMAGE_PNG, png_image.c_str(), png_image.length(),
                           IMAGE_JPEG, jpeg_image.c_str(), jpeg_image.length(),
                           &message_handler);
  }
}

// Verify that the reader exit gracefully when the input is an invalid JPEG.
TEST(JpegReaderTest, InvalidJpegs) {
  for (size_t i = 0; i < kInvalidFileCount; ++i) {
    GoogleString src_data;
    ReadTestFileWithExt(kJpegTestDir, kInvalidFiles[i], &src_data);
    MockMessageHandler message_handler(new NullMutex);
    JpegScanlineReader reader(&message_handler);
    message_handler.AddPatternToSkipPrinting(kMessagePatternLibJpegFailure);
    if (i < kInvalidFileCount-1) {
      ASSERT_FALSE(reader.Initialize(src_data.c_str(), src_data.length()));
    } else {
      ASSERT_TRUE(reader.Initialize(src_data.c_str(), src_data.length()));
      void* scanline = NULL;
      // The image data section of this image is corrupted. The first 89 rows
      // can be decoded correctly, but not for the 90th or later rows.
      for (int row = 0; row < 89; ++row) {
        ASSERT_TRUE(reader.ReadNextScanline(&scanline));
      }
      ASSERT_FALSE(reader.ReadNextScanline(&scanline));
    }
  }
}

// Verify that the reader work properly no matter how many scalines it reads.
TEST(JpegReaderTest, PartialRead) {
  GoogleString image1, image2;
  void* scanline = NULL;
  MockMessageHandler message_handler(new NullMutex);

  ReadTestFile(kJpegTestDir, kValidJpegImages[0], "jpg", &image1);
  ReadTestFile(kJpegTestDir, kValidJpegImages[1], "jpg", &image2);

  JpegScanlineReader reader1(&message_handler);
  ASSERT_TRUE(reader1.Initialize(image1.c_str(), image1.length()));

  JpegScanlineReader reader2(&message_handler);
  ASSERT_TRUE(reader2.Initialize(image1.c_str(), image1.length()));
  ASSERT_TRUE(reader2.ReadNextScanline(&scanline));

  JpegScanlineReader reader3(&message_handler);
  ASSERT_TRUE(reader3.Initialize(image1.c_str(), image1.length()));
  ASSERT_TRUE(reader3.ReadNextScanline(&scanline));
  ASSERT_TRUE(reader3.ReadNextScanline(&scanline));
  ASSERT_TRUE(reader3.Initialize(image2.c_str(), image2.length()));
  ASSERT_TRUE(reader3.ReadNextScanline(&scanline));

  JpegScanlineReader reader4(&message_handler);
  ASSERT_TRUE(reader4.Initialize(image1.c_str(), image1.length()));
  while (reader4.HasMoreScanLines()) {
    ASSERT_TRUE(reader4.ReadNextScanline(&scanline));
  }

  // After depleting the scanlines, any further call to
  // ReadNextScanline leads to death in debugging mode, or a
  // false in release mode.
#ifdef NDEBUG
  EXPECT_FALSE(reader4.ReadNextScanline(&scanline));
#else
  EXPECT_DEATH(reader4.ReadNextScanline(&scanline),
               "The reader was not initialized or does not "
               "have any more scanlines.");
#endif

  ASSERT_TRUE(reader4.Initialize(image2.c_str(), image2.length()));
  ASSERT_TRUE(reader4.ReadNextScanline(&scanline));
}

}  // namespace
