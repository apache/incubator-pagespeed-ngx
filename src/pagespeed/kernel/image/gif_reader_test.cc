/*
 * Copyright 2009 Google Inc.
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

// Test that basic GifReader operations succeed or fail as expected.
// Note that read-in file contents are tested against golden RGBA
// files in png_optimizer_test.cc, not here.

// Author: Victor Chudnovsky

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/gif_reader.h"
#include "pagespeed/kernel/image/png_optimizer.h"
#include "pagespeed/kernel/image/scanline_utils.h"
#include "pagespeed/kernel/image/test_utils.h"

namespace {

using net_instaweb::MockMessageHandler;
using net_instaweb::NullMutex;
using pagespeed::image_compression::kGifTestDir;
using pagespeed::image_compression::kPngSuiteGifTestDir;
using pagespeed::image_compression::kPngSuiteTestDir;
using pagespeed::image_compression::kPngTestDir;
using pagespeed::image_compression::kValidGifImageCount;
using pagespeed::image_compression::kValidGifImages;
using pagespeed::image_compression::GifReader;
using pagespeed::image_compression::GifScanlineReaderRaw;
using pagespeed::image_compression::ImageFormat;
using pagespeed::image_compression::IMAGE_GIF;
using pagespeed::image_compression::IMAGE_PNG;
using pagespeed::image_compression::PixelFormat;
using pagespeed::image_compression::PngReaderInterface;
using pagespeed::image_compression::ReadTestFile;
using pagespeed::image_compression::ScopedPngStruct;
using pagespeed::image_compression::kMessagePatternFailedToOpen;
using pagespeed::image_compression::kMessagePatternFailedToRead;
using pagespeed::image_compression::kMessagePatternLibpngError;
using pagespeed::image_compression::kMessagePatternLibpngWarning;
using pagespeed::image_compression::kMessagePatternUnexpectedEOF;

const char *kValidOpaqueGifImages[] = {
  "basi0g01",
  "basi0g02",
  "basi0g04",
  "basi0g08",
  "basi3p01",
  "basi3p02",
  "basi3p04",
  "basi3p08",
  "basn0g01",
  "basn0g02",
  "basn0g04",
  "basn0g08",
  "basn3p01",
  "basn3p02",
  "basn3p04",
  "basn3p08",
};

const char *kValidTransparentGifImages[] = {
  "tr-basi4a08",
  "tr-basn4a08"
};

const size_t kValidOpaqueGifImageCount = arraysize(kValidOpaqueGifImages);
const size_t kValidTransparentGifImageCount =
    arraysize(kValidTransparentGifImages);

const char kAnimatedGif[] = "animated";
const char kBadGif[] = "bad";
const char kCompletelyTransparentImage[] = "completely_transparent";
const char kInterlacedImage[] = "interlaced";
const char kRedConforming[] = "red_conforming";
const char kRedEmptyScreen[] = "red_empty_screen";
const char kRedUnusedBackground[] = "red_unused_invalid_background";
const char kTransparentGif[] = "transparent";
const char kZeroSizeAnimatedGif[] = "zero_size_animation";

// Message to ignore.
const char kMessagePatternMultipleFrameGif[] =
    "Multiple frame GIF is not supported.";

class GifReaderTest : public testing::Test {
 public:
  GifReaderTest()
    : message_handler_(new NullMutex),
      gif_reader_(new GifReader(&message_handler_)),
      read_(ScopedPngStruct::READ, &message_handler_) {
  }

 protected:
  virtual void SetUp() {
    message_handler_.AddPatternToSkipPrinting(kMessagePatternLibpngError);
    message_handler_.AddPatternToSkipPrinting(kMessagePatternLibpngWarning);
  }

 protected:
  MockMessageHandler message_handler_;
  scoped_ptr<PngReaderInterface> gif_reader_;
  ScopedPngStruct read_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GifReaderTest);
};

TEST_F(GifReaderTest, LoadValidGifsWithoutTransforms) {
  GoogleString in, out;
  for (size_t i = 0; i < kValidOpaqueGifImageCount; i++) {
    ReadTestFile(
        kPngSuiteGifTestDir, kValidOpaqueGifImages[i], "gif", &in);
    ASSERT_NE(static_cast<size_t>(0), in.length());
    ASSERT_TRUE(gif_reader_->ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                                     PNG_TRANSFORM_IDENTITY))
        << kValidOpaqueGifImages[i];
    ASSERT_TRUE(read_.reset());
  }

  for (size_t i = 0; i < kValidTransparentGifImageCount; i++) {
    ReadTestFile(
        kPngSuiteGifTestDir, kValidTransparentGifImages[i], "gif", &in);
    ASSERT_NE(static_cast<size_t>(0), in.length());
    ASSERT_TRUE(gif_reader_->ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                                     PNG_TRANSFORM_IDENTITY))
        << kValidTransparentGifImages[i];
    ASSERT_TRUE(read_.reset());
  }

  ReadTestFile(kGifTestDir, "transparent", "gif", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  ASSERT_TRUE(gif_reader_->ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                                   PNG_TRANSFORM_IDENTITY));
}

TEST_F(GifReaderTest, ExpandColorMapForValidGifs) {
  GoogleString in, out;
  for (size_t i = 0; i < kValidOpaqueGifImageCount; i++) {
    ReadTestFile(
        kPngSuiteGifTestDir, kValidOpaqueGifImages[i], "gif", &in);
    ASSERT_NE(static_cast<size_t>(0), in.length());
    ASSERT_TRUE(gif_reader_->ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                                     PNG_TRANSFORM_EXPAND))
        << kValidOpaqueGifImages[i];
    ASSERT_TRUE(read_.reset());
  }

  for (size_t i = 0; i < kValidTransparentGifImageCount; i++) {
    ReadTestFile(
        kPngSuiteGifTestDir, kValidTransparentGifImages[i], "gif", &in);
    ASSERT_NE(static_cast<size_t>(0), in.length());
    ASSERT_TRUE(gif_reader_->ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                                     PNG_TRANSFORM_EXPAND))
        << kValidTransparentGifImages[i];
    ASSERT_TRUE(read_.reset());
  }

  ReadTestFile(kGifTestDir, "transparent", "gif", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  ASSERT_TRUE(gif_reader_->ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                                   PNG_TRANSFORM_EXPAND));
}

TEST_F(GifReaderTest, RequireOpaqueForValidGifs) {
  GoogleString in;
  for (size_t i = 0; i < kValidOpaqueGifImageCount; i++) {
    ReadTestFile(
        kPngSuiteGifTestDir, kValidOpaqueGifImages[i], "gif", &in);
    ASSERT_NE(static_cast<size_t>(0), in.length());
    ASSERT_TRUE(gif_reader_->ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                                     PNG_TRANSFORM_IDENTITY, true))
        << kValidOpaqueGifImages[i];
    ASSERT_TRUE(read_.reset());
  }

  for (size_t i = 0; i < kValidTransparentGifImageCount; i++) {
    ReadTestFile(
        kPngSuiteGifTestDir, kValidTransparentGifImages[i], "gif", &in);
    ASSERT_NE(static_cast<size_t>(0), in.length());
    ASSERT_FALSE(gif_reader_->ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                                      PNG_TRANSFORM_IDENTITY, true))
        << kValidTransparentGifImages[i];
    ASSERT_TRUE(read_.reset());
  }

  ReadTestFile(kGifTestDir, "transparent", "gif", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  ASSERT_FALSE(gif_reader_->ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                                    PNG_TRANSFORM_IDENTITY, true));
}

TEST_F(GifReaderTest, ExpandColormapAndRequireOpaqueForValidGifs) {
  GoogleString in;
  for (size_t i = 0; i < kValidOpaqueGifImageCount; i++) {
    ReadTestFile(
        kPngSuiteGifTestDir, kValidOpaqueGifImages[i], "gif", &in);
    ASSERT_NE(static_cast<size_t>(0), in.length());
    ASSERT_TRUE(gif_reader_->ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                                     PNG_TRANSFORM_EXPAND, true))
        << kValidOpaqueGifImages[i];
    ASSERT_TRUE(read_.reset());
  }

  for (size_t i = 0; i < kValidTransparentGifImageCount; i++) {
    ReadTestFile(
        kPngSuiteGifTestDir, kValidTransparentGifImages[i], "gif", &in);
    ASSERT_NE(static_cast<size_t>(0), in.length());
    ASSERT_FALSE(gif_reader_->ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                                      PNG_TRANSFORM_EXPAND, true))
        << kValidTransparentGifImages[i];
    ASSERT_TRUE(read_.reset());
  }

  ReadTestFile(kGifTestDir, "transparent", "gif", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  ASSERT_FALSE(gif_reader_->ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                                    PNG_TRANSFORM_EXPAND, true));
}

TEST_F(GifReaderTest, StripAlpha) {
  GoogleString in;
  png_uint_32 height;
  png_uint_32 width;
  int bit_depth;
  int color_type;
  png_bytep trans;
  int num_trans;
  png_color_16p trans_values;

  ReadTestFile(kGifTestDir, "transparent", "gif", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  ASSERT_TRUE(gif_reader_->ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                                   PNG_TRANSFORM_STRIP_ALPHA, false));
  png_get_IHDR(read_.png_ptr(), read_.info_ptr(),
               &width, &height, &bit_depth, &color_type,
               NULL, NULL, NULL);
  ASSERT_TRUE((color_type & PNG_COLOR_MASK_ALPHA) == 0);  // NOLINT
  ASSERT_EQ(static_cast<unsigned int>(0),
            png_get_tRNS(read_.png_ptr(),
                         read_.info_ptr(),
                         &trans,
                         &num_trans,
                         &trans_values));

  read_.reset();

  ASSERT_TRUE(gif_reader_->ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                                   PNG_TRANSFORM_STRIP_ALPHA |
                                   PNG_TRANSFORM_EXPAND, false));
  png_get_IHDR(read_.png_ptr(), read_.info_ptr(),
               &width, &height, &bit_depth, &color_type,
               NULL, NULL, NULL);
  ASSERT_TRUE((color_type & PNG_COLOR_MASK_ALPHA) == 0);  // NOLINT

  ASSERT_EQ(static_cast<unsigned int>(0),
            png_get_tRNS(read_.png_ptr(),
                         read_.info_ptr(),
                         &trans,
                         &num_trans,
                         &trans_values));
}

TEST_F(GifReaderTest, ExpandColormapOnZeroSizeCanvasAndCatchLibPngError) {
  GoogleString in;
  // This is a free image from
  // <http://www.gifs.net/subcategory/40/0/20/Email>, with the canvas
  // size information manually set to zero in order to trigger a
  // libpng error.
  ReadTestFile(kGifTestDir, "zero_size_animation", "gif", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  ASSERT_FALSE(gif_reader_->ReadPng(in, read_.png_ptr(), read_.info_ptr(),
                                    PNG_TRANSFORM_EXPAND, true));
}

class GifScanlineReaderRawTest : public testing::Test {
 public:
  GifScanlineReaderRawTest()
    : scanline_(NULL),
      message_handler_(new NullMutex),
      reader_(&message_handler_) {
  }

  bool Initialize(const char* file_name) {
    if (!ReadTestFile(kGifTestDir, file_name, "gif", &input_image_)) {
      PS_LOG_DFATAL((&message_handler_), "Failed to read file: %s", file_name);
      return false;
    }
    return reader_.Initialize(input_image_.c_str(), input_image_.length());
  }

 protected:
  virtual void SetUp() {
    message_handler_.AddPatternToSkipPrinting(kMessagePatternFailedToOpen);
    message_handler_.AddPatternToSkipPrinting(kMessagePatternFailedToRead);
    message_handler_.AddPatternToSkipPrinting(kMessagePatternMultipleFrameGif);
    message_handler_.AddPatternToSkipPrinting(kMessagePatternUnexpectedEOF);
  }

 protected:
  void* scanline_;
  MockMessageHandler message_handler_;
  GifScanlineReaderRaw reader_;
  GoogleString input_image_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GifScanlineReaderRawTest);
};

TEST_F(GifScanlineReaderRawTest, CorruptHeader) {
  ReadTestFile(kGifTestDir, kTransparentGif, "gif", &input_image_);
  // Make GifRecordType invalid.
  input_image_[781] = 0;
  ASSERT_FALSE(reader_.Initialize(input_image_.c_str(),
      input_image_.length()));
}

TEST_F(GifScanlineReaderRawTest, InitializeWithoutRead) {
  ASSERT_TRUE(Initialize(kTransparentGif));
}

TEST_F(GifScanlineReaderRawTest, ReadOneRow) {
  ASSERT_TRUE(Initialize(kTransparentGif));
  EXPECT_TRUE(reader_.ReadNextScanline(&scanline_));
}

TEST_F(GifScanlineReaderRawTest, ReinitializeAfterOneRow) {
  ASSERT_TRUE(Initialize(kTransparentGif));
  EXPECT_TRUE(reader_.ReadNextScanline(&scanline_));
  ASSERT_TRUE(Initialize(kInterlacedImage));
  EXPECT_TRUE(reader_.ReadNextScanline(&scanline_));
}

TEST_F(GifScanlineReaderRawTest, ReInitializeAfterLastRow) {
  ASSERT_TRUE(Initialize(kTransparentGif));
  while (reader_.HasMoreScanLines()) {
    EXPECT_TRUE(reader_.ReadNextScanline(&scanline_));
  }
  EXPECT_FALSE(reader_.ReadNextScanline(&scanline_));
  ASSERT_TRUE(Initialize(kInterlacedImage));
  EXPECT_TRUE(reader_.ReadNextScanline(&scanline_));
}

// Animated GIF is not supported. Make sure GIF reader exits gracefully.
TEST_F(GifScanlineReaderRawTest, AnimatedGif) {
  ASSERT_FALSE(Initialize(kAnimatedGif));
}

TEST_F(GifScanlineReaderRawTest, BadGif) {
  ASSERT_FALSE(Initialize(kBadGif));
}

TEST_F(GifScanlineReaderRawTest, ZeroSizeGif) {
  ASSERT_FALSE(Initialize(kZeroSizeAnimatedGif));
}

// Check the accuracy of the reader. Compare the decoded results with the gold
// data (".gif.rgba"). The test images include transparent and opaque ones.
TEST_F(GifScanlineReaderRawTest, ValidGifs) {
  for (size_t i = 0; i < kValidGifImageCount; i++) {
    GoogleString rgba_image, gif_image;
    const char* file_name = kValidGifImages[i].filename;
    ReadTestFile(kPngSuiteGifTestDir, file_name, "gif.rgba", &rgba_image);
    ReadTestFile(kPngSuiteGifTestDir, file_name, "gif", &gif_image);

    const uint8_t* reference_rgba =
        reinterpret_cast<const uint8*>(rgba_image.data());
    uint8* decoded_pixels = NULL;

    ASSERT_TRUE(reader_.Initialize(gif_image.data(), gif_image.length()));

    PixelFormat pixel_format = reader_.GetPixelFormat();
    int width = reader_.GetImageWidth();
    int height = reader_.GetImageHeight();
    int bytes_per_row = reader_.GetBytesPerScanline();
    int num_channels = GetNumChannelsFromPixelFormat(pixel_format,
                                                     &message_handler_);

    EXPECT_EQ(kValidGifImages[i].width, width);
    EXPECT_EQ(kValidGifImages[i].height, height);
    if (kValidGifImages[i].transparency) {
      EXPECT_EQ(pagespeed::image_compression::RGBA_8888, pixel_format);
      EXPECT_EQ(4, num_channels);
    } else {
      EXPECT_EQ(pagespeed::image_compression::RGB_888, pixel_format);
      EXPECT_EQ(3, num_channels);
    }
    EXPECT_EQ(width*num_channels, bytes_per_row);

    // Decode and check the image a row at a time.
    int row = 0;
    while (reader_.HasMoreScanLines()) {
      EXPECT_TRUE(reader_.ReadNextScanline(
        reinterpret_cast<void**>(&decoded_pixels)));

      for (int x = 0; x < width; ++x) {
        int index_dec = x * num_channels;
        int index_ref = (row * width + x) * 4;
        // ASSERT_EQ is used in stead of EXPECT_EQ. Otherwise, the log will be
        // spammed when an error happens.
        ASSERT_EQ(0, memcmp(reference_rgba+index_ref, decoded_pixels+index_dec,
                            num_channels));
      }
      ++row;
    }

    // Make sure both readers have exhausted all image rows.
    EXPECT_EQ(height, row);
    EXPECT_EQ(rgba_image.length(),
              static_cast<size_t>(4 * height * width));
  }
}

TEST_F(GifScanlineReaderRawTest, Interlaced) {
  GoogleString png_image, gif_image;
  ReadTestFile(kGifTestDir, kInterlacedImage, "png", &png_image);
  ReadTestFile(kGifTestDir, kInterlacedImage, "gif", &gif_image);
  DecodeAndCompareImages(IMAGE_PNG, png_image.c_str(), png_image.length(),
                         IMAGE_GIF, gif_image.c_str(), gif_image.length(),
                         &message_handler_);
}

TEST_F(GifScanlineReaderRawTest, CompletelyTransparent) {
  GoogleString png_image, gif_image;
  ReadTestFile(kGifTestDir, kCompletelyTransparentImage, "png", &png_image);
  ReadTestFile(kGifTestDir, kCompletelyTransparentImage, "gif", &gif_image);
  DecodeAndCompareImages(IMAGE_PNG, png_image.c_str(), png_image.length(),
                         IMAGE_GIF, gif_image.c_str(), gif_image.length(),
                         &message_handler_);
}

TEST_F(GifScanlineReaderRawTest, EmptyScreen) {
  GoogleString png_image, gif_image;
  ReadTestFile(kGifTestDir, kRedConforming, "png", &png_image);
  ReadTestFile(kGifTestDir, kRedEmptyScreen, "gif", &gif_image);
  DecodeAndCompareImages(IMAGE_PNG, png_image.c_str(), png_image.length(),
                         IMAGE_GIF, gif_image.c_str(), gif_image.length(),
                         &message_handler_);
}

TEST_F(GifScanlineReaderRawTest, UnusedBackground) {
  GoogleString png_image, gif_image;
  ReadTestFile(kGifTestDir, kRedConforming, "png", &png_image);
  ReadTestFile(kGifTestDir, kRedUnusedBackground, "gif", &gif_image);
  DecodeAndCompareImages(IMAGE_PNG, png_image.c_str(), png_image.length(),
                         IMAGE_GIF, gif_image.c_str(), gif_image.length(),
                         &message_handler_);
}

}  // namespace
