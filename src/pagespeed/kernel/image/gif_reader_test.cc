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

#include <vector>

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/image/gif_reader.h"
#include "pagespeed/kernel/image/gif_square.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/image/png_optimizer.h"
#include "pagespeed/kernel/image/scanline_interface_frame_adapter.h"
#include "pagespeed/kernel/image/scanline_utils.h"
#include "pagespeed/kernel/image/test_utils.h"

namespace {

extern "C" {
#ifdef USE_SYSTEM_LIBPNG
#include "png.h"  // NOLINT
#else
#include "third_party/libpng/png.h"
#endif
}  // extern "C"

using net_instaweb::MockMessageHandler;
using net_instaweb::NullMutex;
using pagespeed::image_compression::kAlphaOpaque;
using pagespeed::image_compression::kAlphaTransparent;
using pagespeed::image_compression::kGifTestDir;
using pagespeed::image_compression::kPngSuiteGifTestDir;
using pagespeed::image_compression::kPngSuiteTestDir;
using pagespeed::image_compression::kPngTestDir;
using pagespeed::image_compression::kValidGifImageCount;
using pagespeed::image_compression::kValidGifImages;
using pagespeed::image_compression::size_px;
using pagespeed::image_compression::FrameSpec;
using pagespeed::image_compression::FrameToScanlineReaderAdapter;
using pagespeed::image_compression::GifDisposalToFrameSpecDisposal;
using pagespeed::image_compression::GifFrameReader;
using pagespeed::image_compression::GifReader;
using pagespeed::image_compression::GifSquare;
using pagespeed::image_compression::ImageFormat;
using pagespeed::image_compression::ImageSpec;
using pagespeed::image_compression::IMAGE_GIF;
using pagespeed::image_compression::IMAGE_PNG;
using pagespeed::image_compression::MultipleFrameReader;
using pagespeed::image_compression::PixelFormat;
using pagespeed::image_compression::PixelFormat;
using pagespeed::image_compression::PngReaderInterface;
using pagespeed::image_compression::ReadTestFile;
using pagespeed::image_compression::ReadFile;
using pagespeed::image_compression::QUIRKS_CHROME;
using pagespeed::image_compression::QUIRKS_FIREFOX;
using pagespeed::image_compression::QUIRKS_NONE;
using pagespeed::image_compression::RGB_888;
using pagespeed::image_compression::RGBA_8888;
using pagespeed::image_compression::RGBA_ALPHA;
using pagespeed::image_compression::RGBA_BLUE;
using pagespeed::image_compression::RGBA_GREEN;
using pagespeed::image_compression::RGBA_RED;
using pagespeed::image_compression::ScanlineStatus;
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
const char kFrameSmallerThanScreen[] = "frame_smaller_than_screen";
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
      reader_(new GifFrameReader(&message_handler_)) {
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
  FrameToScanlineReaderAdapter reader_;
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

  // After depleting the scanlines, any further call to
  // ReadNextScanline leads to death in debugging mode, or a
  // false in release mode.
#ifdef NDEBUG
  EXPECT_FALSE(reader_.ReadNextScanline(&scanline_));
#else
  EXPECT_DEATH(reader_.ReadNextScanline(&scanline_),
               "The GIF image was not initialized or does not "
               "have more scanlines.");
#endif

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

TEST_F(GifScanlineReaderRawTest, FrameSmallerThanImage) {
  ASSERT_FALSE(Initialize(kFrameSmallerThanScreen));
  EXPECT_EQ(1, message_handler_.MessagesOfType(net_instaweb::kInfo));
  EXPECT_EQ(0, message_handler_.MessagesOfType(net_instaweb::kWarning));
  EXPECT_EQ(0, message_handler_.MessagesOfType(net_instaweb::kError));
  EXPECT_EQ(0, message_handler_.MessagesOfType(net_instaweb::kFatal));
}

TEST(GifReaderUtil, DisposalMethod) {
  for (int i = -1; i < 4; ++i) {
    FrameSpec::DisposalMethod actual_disposal =
        GifDisposalToFrameSpecDisposal(i);
    FrameSpec::DisposalMethod expected_disposal = FrameSpec::DISPOSAL_NONE;
    switch (i) {
      case 0:
        // intentional fall-through
      case 1:
        expected_disposal = FrameSpec::DISPOSAL_NONE;
        break;
      case 2:
        expected_disposal = FrameSpec::DISPOSAL_BACKGROUND;
        break;
      case 3:
        expected_disposal = FrameSpec::DISPOSAL_RESTORE;
        break;
    }
    EXPECT_EQ(expected_disposal, actual_disposal);
  }
}

void CheckQuirksModeChangesToImageSpec(const FrameSpec& frame_spec,
                                       const ImageSpec& original_spec,
                                       const bool has_loop_count,
                                       const ImageSpec& expected_noquirks_spec,
                                       const ImageSpec& expected_firefox_spec,
                                       const ImageSpec& expected_chrome_spec) {
  ImageSpec noquirks_spec = original_spec;
  ImageSpec firefox_spec = original_spec;
  ImageSpec chrome_spec = original_spec;

  GifFrameReader::ApplyQuirksModeToImage(QUIRKS_NONE, has_loop_count,
                                         frame_spec, &noquirks_spec);
  EXPECT_TRUE(noquirks_spec.Equals(expected_noquirks_spec));

  GifFrameReader::ApplyQuirksModeToImage(QUIRKS_FIREFOX, has_loop_count,
                                         frame_spec, &firefox_spec);
  EXPECT_TRUE(firefox_spec.Equals(expected_firefox_spec));

  GifFrameReader::ApplyQuirksModeToImage(QUIRKS_CHROME, has_loop_count,
                                         frame_spec, &chrome_spec);
  EXPECT_TRUE(chrome_spec.Equals(expected_chrome_spec));
}

TEST(ApplyQuirksModeToImage, TestWidth) {
  ImageSpec image_spec;
  FrameSpec frame_spec;
  image_spec.width = 100;
  image_spec.height = 100;
  frame_spec.width = 200;
  frame_spec.height = 50;
  frame_spec.top= 10;
  frame_spec.left = 2;

  ImageSpec expected_noquirks_spec = image_spec;
  ImageSpec expected_firefox_spec = image_spec;
  ImageSpec expected_chrome_spec = image_spec;

  expected_chrome_spec.width = frame_spec.width;
  expected_chrome_spec.height = frame_spec.height;

  CheckQuirksModeChangesToImageSpec(frame_spec, image_spec, false,
                                    expected_noquirks_spec,
                                    expected_firefox_spec,
                                    expected_chrome_spec);
}

TEST(ApplyQuirksModeToImage, TestHeight) {
  ImageSpec image_spec;
  FrameSpec frame_spec;
  image_spec.width = 100;
  image_spec.height = 100;
  frame_spec.width = 50;
  frame_spec.height = 200;
  frame_spec.top= 10;
  frame_spec.left = 2;

  ImageSpec expected_noquirks_spec = image_spec;
  ImageSpec expected_firefox_spec = image_spec;
  ImageSpec expected_chrome_spec = image_spec;

  expected_chrome_spec.width = frame_spec.width;
  expected_chrome_spec.height = frame_spec.height;

  CheckQuirksModeChangesToImageSpec(frame_spec, image_spec, false,
                                    expected_noquirks_spec,
                                    expected_firefox_spec,
                                    expected_chrome_spec);
}

TEST(ApplyQuirksModeToImage, TestLoopCount) {
  ImageSpec image_spec;
  FrameSpec frame_spec;
  image_spec.width = 100;
  image_spec.height = 100;
  image_spec.loop_count = 3;
  frame_spec.width = 100;
  frame_spec.height = 100;
  frame_spec.top= 0;
  frame_spec.left = 0;

  ImageSpec expected_noquirks_spec = image_spec;
  ImageSpec expected_firefox_spec = image_spec;
  ImageSpec expected_chrome_spec = image_spec;

  CheckQuirksModeChangesToImageSpec(frame_spec, image_spec, false,
                                    expected_noquirks_spec,
                                    expected_firefox_spec,
                                    expected_chrome_spec);

  expected_chrome_spec.loop_count = image_spec.loop_count + 1;

  CheckQuirksModeChangesToImageSpec(frame_spec, image_spec, true,
                                    expected_noquirks_spec,
                                    expected_firefox_spec,
                                    expected_chrome_spec);
}

TEST(ApplyQuirksModeToImage, TestNoop) {
  ImageSpec image_spec;
  FrameSpec frame_spec;
  image_spec.width = 100;
  image_spec.height = 100;
  frame_spec.width = 50;
  frame_spec.height = 50;
  frame_spec.top= 10;
  frame_spec.left = 2;

  ImageSpec expected_noquirks_spec = image_spec;
  ImageSpec expected_firefox_spec = image_spec;
  ImageSpec expected_chrome_spec = image_spec;

  CheckQuirksModeChangesToImageSpec(frame_spec, image_spec, false,
                                    expected_noquirks_spec,
                                    expected_firefox_spec,
                                    expected_chrome_spec);
}

void CheckQuirksModeChangesToFirstFrameSpec(
    const ImageSpec& image_spec,
    const FrameSpec& original_spec,
    const FrameSpec& expected_noquirks_spec,
    const FrameSpec& expected_firefox_spec,
    const FrameSpec& expected_chrome_spec) {
  FrameSpec noquirks_spec = original_spec;
  FrameSpec firefox_spec = original_spec;
  FrameSpec chrome_spec = original_spec;

  GifFrameReader::ApplyQuirksModeToFirstFrame(QUIRKS_NONE,
                                              image_spec, &noquirks_spec);
  EXPECT_TRUE(noquirks_spec.Equals(expected_noquirks_spec));

  GifFrameReader::ApplyQuirksModeToFirstFrame(QUIRKS_FIREFOX,
                                              image_spec, &firefox_spec);
  EXPECT_TRUE(firefox_spec.Equals(expected_firefox_spec));

  GifFrameReader::ApplyQuirksModeToFirstFrame(QUIRKS_CHROME,
                                              image_spec, &chrome_spec);
  EXPECT_TRUE(chrome_spec.Equals(expected_chrome_spec));
}

TEST(ApplyQuirksModeToFirstFrame, TestWidth) {
  ImageSpec image_spec;
  FrameSpec frame_spec;
  image_spec.width = 100;
  image_spec.height = 100;
  frame_spec.width = 200;
  frame_spec.height = 50;
  frame_spec.top= 10;
  frame_spec.left = 2;

  FrameSpec expected_noquirks_spec = frame_spec;
  FrameSpec expected_firefox_spec = frame_spec;
  FrameSpec expected_chrome_spec = frame_spec;

  expected_firefox_spec.top = 0;
  expected_firefox_spec.left = 0;

  CheckQuirksModeChangesToFirstFrameSpec(image_spec, frame_spec,
                                         expected_noquirks_spec,
                                         expected_firefox_spec,
                                         expected_chrome_spec);
}

TEST(ApplyQuirksModeToFirstFrame, TestHeight) {
  ImageSpec image_spec;
  FrameSpec frame_spec;
  image_spec.width = 100;
  image_spec.height = 100;
  frame_spec.width = 50;
  frame_spec.height = 200;
  frame_spec.top= 10;
  frame_spec.left = 2;

  FrameSpec expected_noquirks_spec = frame_spec;
  FrameSpec expected_firefox_spec = frame_spec;
  FrameSpec expected_chrome_spec = frame_spec;

  expected_firefox_spec.top = 0;
  expected_firefox_spec.left = 0;

  CheckQuirksModeChangesToFirstFrameSpec(image_spec, frame_spec,
                                         expected_noquirks_spec,
                                         expected_firefox_spec,
                                         expected_chrome_spec);
}

TEST(ApplyQuirksModeToFirstFrame, TestNoop) {
  ImageSpec image_spec;
  FrameSpec frame_spec;
  image_spec.width = 100;
  image_spec.height = 100;
  frame_spec.width = 50;
  frame_spec.height = 50;
  frame_spec.top= 10;
  frame_spec.left = 2;

  FrameSpec expected_noquirks_spec = frame_spec;
  FrameSpec expected_firefox_spec = frame_spec;
  FrameSpec expected_chrome_spec = frame_spec;

  CheckQuirksModeChangesToFirstFrameSpec(image_spec, frame_spec,
                                         expected_noquirks_spec,
                                         expected_firefox_spec,
                                         expected_chrome_spec);
}

class GifAnimationTest : public testing::Test {
 public:
  struct Image {
    size_px width;
    size_px height;
    int bg_color_idx;
    size_t loop_count;
  };

  struct Frame {
    size_px width;
    size_px height;
    bool interlace;
    int delay_cs;
    int disposal;
    const GifColorType* colormap;
    int num_colors;
    int transparent_idx;
    int square_color_idx;
    size_px top;
    size_px left;
  };

  GifAnimationTest()
      : scanline_(NULL),
        message_handler_(new NullMutex),
        gif_(true, &message_handler_),
        reader_(new GifFrameReader(&message_handler_)),
        read_all_scanlines_(true) {
  }

  void SynthesizeImage(const char* filename, const Image& image) {
    // Note that these images are synthesized with QUIRKS_NONE.
    filename_ = net_instaweb::StrCat(net_instaweb::GTestTempDir(),
                                     "/", filename, ".gif");
    EXPECT_TRUE(gif_.Open(filename_));
    PS_LOG_INFO((&message_handler_), "Generating image: %s", filename_.c_str());
    gif_.PrepareScreen(true, image.width, image.height, kColorMap, kNumColors,
                       image.bg_color_idx, image.loop_count);
    for (std::vector<Frame>::iterator frame = synth_frames_.begin();
         frame != synth_frames_.end();
         ++frame) {
      EXPECT_TRUE(gif_.PutImage(
          frame->left, frame->top, frame->width, frame->height,
          frame->colormap, frame->num_colors,
          frame->square_color_idx, frame->transparent_idx,
          frame->interlace, frame->delay_cs, frame->disposal));
    }
    EXPECT_TRUE(gif_.Close());
  }

  void ReadImage(const Image& image) {
    if (!ReadFile(filename_, &input_image_)) {
      PS_LOG_DFATAL((&message_handler_),
                    "Failed to read file: %s", filename_.c_str());
      return;
    }

    EXPECT_TRUE(reader_->Initialize(input_image_.c_str(),
                                    input_image_.length()).Success());
    ScanlineStatus status;
    ImageSpec image_spec;
    EXPECT_TRUE(reader_->GetImageSpec(&image_spec, &status));
    EXPECT_EQ(image.width, image_spec.width);
    EXPECT_EQ(image.height, image_spec.height);
    EXPECT_EQ((image.loop_count == GifSquare::kNoLoopCountSpecified ?
               1 : image.loop_count),
              image_spec.loop_count);
    EXPECT_EQ(synth_frames_.size(), image_spec.num_frames);
    EXPECT_FALSE(image_spec.use_bg_color);
    EXPECT_EQ(kColorMap[image.bg_color_idx].Red,
              image_spec.bg_color[RGBA_RED]);
    EXPECT_EQ(kColorMap[image.bg_color_idx].Green,
              image_spec.bg_color[RGBA_GREEN]);
    EXPECT_EQ(kColorMap[image.bg_color_idx].Blue,
              image_spec.bg_color[RGBA_BLUE]);
    EXPECT_EQ(kAlphaOpaque, image_spec.bg_color[RGBA_ALPHA]);
    for (std::vector<Frame>::iterator set_frame = synth_frames_.begin();
         set_frame != synth_frames_.end();
         ++set_frame) {
      FrameSpec frame_spec;
      EXPECT_TRUE(reader_->HasMoreFrames());
      EXPECT_TRUE(reader_->PrepareNextFrame(&status));
      EXPECT_TRUE(reader_->GetFrameSpec(&frame_spec, &status));

      EXPECT_EQ(set_frame->delay_cs < 0 ?
                0 : set_frame->delay_cs * 10, frame_spec.duration_ms);
      EXPECT_EQ(set_frame->width, frame_spec.width);
      EXPECT_EQ(set_frame->height, frame_spec.height);
      EXPECT_EQ(set_frame->top, frame_spec.top);
      EXPECT_EQ(set_frame->left, frame_spec.left);
      EXPECT_EQ(set_frame->interlace, frame_spec.hint_progressive);
      EXPECT_EQ(set_frame->transparent_idx >= 0 ?
                RGBA_8888 : RGB_888, frame_spec.pixel_format);
      FrameSpec::DisposalMethod frame_disposal =
          GifDisposalToFrameSpecDisposal(set_frame->disposal);
      EXPECT_EQ((frame_disposal == FrameSpec::DISPOSAL_UNKNOWN ?
                 FrameSpec::DISPOSAL_NONE : frame_disposal),
                frame_spec.disposal);

      const GifColorType* cmap = (set_frame->colormap == NULL ?
                                  kColorMap : set_frame->colormap);
      const int bytes_per_pixel = GetBytesPerPixel(frame_spec.pixel_format);
      static const int kRgbBytes = 3;

      if (read_all_scanlines_) {
        for (int row = 0; row < frame_spec.height; ++row) {
          EXPECT_TRUE(reader_->HasMoreScanlines());
          const uint8_t* scanline;
          ScanlineStatus status =
              reader_->ReadNextScanline(
                  reinterpret_cast<const void **>(&scanline));
          EXPECT_TRUE(status.Success());

          for (int col = 0; col < frame_spec.width; ++col) {
            // Since we're comparing pixel by pixel, correlated errors
            // would generate huge output if we use EXPECT rather than
            // ASSERT below.
            ASSERT_EQ(0, memcmp(scanline + col * bytes_per_pixel,
                                cmap + set_frame->square_color_idx, kRgbBytes));
            if (frame_spec.pixel_format == RGBA_8888) {
              ASSERT_EQ(*(scanline + col * bytes_per_pixel + kRgbBytes),
                        ((set_frame->transparent_idx ==
                          set_frame->square_color_idx) ?
                         kAlphaTransparent : kAlphaOpaque));
            }
          }
        }
        EXPECT_FALSE(reader_->HasMoreScanlines());
      }
    }
    EXPECT_FALSE(reader_->HasMoreFrames());
  }

  Image DefineImage() {
    Image image;
    image.width = 100;
    image.height = 100;
    image.bg_color_idx = 3;
    image.loop_count = GifSquare::kNoLoopCountSpecified;
    return image;
  }

  void SynthesizeAndRead(const char* name, const Image& image) {
    SynthesizeImage(name, image);
    ReadImage(image);
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
  GifSquare gif_;
  scoped_ptr<MultipleFrameReader> reader_;
  bool read_all_scanlines_;
  GoogleString filename_;
  GoogleString input_image_;
  std::vector<Frame> synth_frames_;

  static const int kNumColors;
  static const GifColorType kColorMap[];
  static const GifColorType kAlternateColorMap[];

 private:
  DISALLOW_COPY_AND_ASSIGN(GifAnimationTest);
};

const int GifAnimationTest::kNumColors = 8;
const GifColorType GifAnimationTest::kColorMap[kNumColors] = {
  GifSquare::kGifWhite,
  GifSquare::kGifBlack,
  GifSquare::kGifRed,
  GifSquare::kGifGreen,
  GifSquare::kGifBlue,
  GifSquare::kGifYellow,
  GifSquare::kGifGray,
  GifSquare::kGifGray,
};

const GifColorType GifAnimationTest::kAlternateColorMap[kNumColors] = {
  GifSquare::kGifBlue,
  GifSquare::kGifRed,
  GifSquare::kGifYellow,
  GifSquare::kGifGreen,
  GifSquare::kGifWhite,
  GifSquare::kGifBlack,
  GifSquare::kGifGray,
  GifSquare::kGifGray,
};

// Note that the various test cases with "FallingOffImage" generate
// images that, when viewed on Chrome and Firefox, exhibit the quirky
// behavior that is captured in ImageSpec and FrameSpec when the
// appropriate QuirksMode is used. The test cases below use
// QUIRKS_NONE to test behavior to the spec.

// Non-animated, non-interlaced, only global colormap, varying disposals.

TEST_F(GifAnimationTest, ReadSingleFrameOpaque) {
  const Frame frame = {10, 10, false, 0, 0, NULL, 0, -1, 2, 10, 10};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_opaque", DefineImage());
}

TEST_F(GifAnimationTest, ReadSingleFrameTransparency) {
  const Frame frame = {10, 10, false, 0, 1, NULL, 0, 4, 2, 10, 10};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_transparency", DefineImage());
}

TEST_F(GifAnimationTest, ReadSingleFrameOpaqueFallingOffImage) {
  const Frame frame = {10, 10, false, 0, 2, NULL, 0, -1, 2, 95, 95};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_opaque_falling_off_image", DefineImage());
}

TEST_F(GifAnimationTest, ReadSingleFrameOpaqueLargeFallingOffImage) {
  const Frame frame = {250, 250, false, 0, 2, NULL, 0, -1, 2, 95, 95};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_opaque_large_falling_off_image",
                    DefineImage());
}

TEST_F(GifAnimationTest, ReadSingleFrameTransparencyFallingOffImage) {
  const Frame frame = {10, 10, false, 0, 3, NULL, 0, 4, 2, 95, 95};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_transparent_falling_off_image",
                    DefineImage());
}

TEST_F(GifAnimationTest, ReadSingleFrameTransparencyFallingOffImageAtOrigin) {
  const Frame frame = {250, 250, false, 0, 3, NULL, 0, 4, 2, 0, 0};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_transparent_falling_off_image_at_origin",
                    DefineImage());
}

TEST_F(GifAnimationTest, ReadSingleFrameOpaqueInZeroSizeImage) {
  Image image = DefineImage();
  image.width = 0;
  image.height = 0;

  const Frame frame = {10, 10, false, 0, 1, NULL, 0, 4, 2, 10, 10};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_opaque_in_zero_size_image", image);
}

TEST_F(GifAnimationTest, ReadSingleFrameOpaqueInZeroSizeImageAtOrigin) {
  Image image = DefineImage();
  image.width = 0;
  image.height = 0;

  const Frame frame = {10, 10, false, 0, 1, NULL, 0, 4, 2, 0, 0};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_opaque_in_zero_size_image_at_origin", image);
}

// Non-animated, interlaced, only global colormap, varying disposals.

TEST_F(GifAnimationTest, ReadSingleFrameInterlacedOpaque) {
  const Frame frame = {10, 10, true, 0, 4, NULL, 0, -1, 2, 10, 10};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_interlaced_opaque", DefineImage());
}

TEST_F(GifAnimationTest, ReadSingleFrameInterlacedTransparency) {
  const Frame frame = {10, 10, true, 0, 0, NULL, 0, 4, 2, 10, 10};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_interlaced_transparency", DefineImage());
}

TEST_F(GifAnimationTest,
       ReadSingleFrameInterlacedOpaqueFallingOffImage) {
  const Frame frame = {10, 10, true, 0, 1, NULL, 0, -1, 2, 95, 95};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_interlaced_opaque_falling_off_image",
                    DefineImage());
}

TEST_F(GifAnimationTest,
       ReadSingleFrameInterlacedTransparencyFallingOffImage) {
  const Frame frame = {10, 10, true, 0, 2, NULL, 0, 4, 2, 95, 95};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_interlaced_transparent_falling_off_image",
                    DefineImage());
}

// Non-animated, non-interlaced, both global and per-frame colormap,
// varying disposals.

TEST_F(GifAnimationTest, ReadSingleFrameDualColormapsOpaque) {
  const Frame frame =
      {10, 10, false, 0, 3, kAlternateColorMap, kNumColors, -1, 2, 10, 10};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_colormaps_opaque", DefineImage());
}

TEST_F(GifAnimationTest, ReadSingleFrameDualColormapsTransparency) {
  const Frame frame =
      {10, 10, false, 0, 4, kAlternateColorMap, kNumColors, 4, 2, 10, 10};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_colormaps_transparency", DefineImage());
}

TEST_F(GifAnimationTest,
       ReadSingleFrameDualColormapsOpaqueFallingOffImage) {
  const Frame frame =
      {10, 10, false, 0, 0, kAlternateColorMap, kNumColors, -1, 2, 95, 95};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_colormaps_opaque_falling_off_image",
                    DefineImage());
}

TEST_F(GifAnimationTest,
       ReadSingleFrameDualColormapsTransparencyFallingOffImage) {
  const Frame frame =
      {10, 10, false, 0, 1, kAlternateColorMap, kNumColors, 4, 2, 95, 95};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_colormaps_transparent_falling_off_image",
                    DefineImage());
}

// Non-animated, non-interlaced, only global colormap, varying
// disposals, varying delays.
TEST_F(GifAnimationTest, ReadSingleFrameDelayOpaque) {
  const Frame frame = {10, 10, true, 10, 0, NULL, 0, -1, 2, 10, 10};
  synth_frames_.push_back(frame);

  SynthesizeAndRead("single_frame_opaque", DefineImage());
}

// Animated images.

TEST_F(GifAnimationTest, ReadMultipleFrameOpaque) {
  Frame frame1 = {20, 20, false, 100, 0, NULL, 0, -1, 2, 10, 10};
  synth_frames_.push_back(frame1);

  Frame frame2 = {20, 20, true, 100, 0, NULL, 0, -1, 3, 20, 20};
  synth_frames_.push_back(frame2);

  SynthesizeAndRead("multiple_frame_opaque", DefineImage());
}

TEST_F(GifAnimationTest, ReadMultipleFrameOpaqueFirstFallingOffImage) {
  Frame frame1 = {250, 250, true, 100, 0, NULL, 0, -1, 3, 90, 90};
  synth_frames_.push_back(frame1);

  Frame frame2 = {20, 20, false, 100, 0, NULL, 0, -1, 2, 79, 79};
  synth_frames_.push_back(frame2);

  SynthesizeAndRead("multiple_frame_opaque_1st_falling_off_image",
                    DefineImage());
}

TEST_F(GifAnimationTest, ReadMultipleFrameOpaqueSecondFallingOffImage) {
  Frame frame1 = {20, 20, false, 100, 0, NULL, 0, -1, 2, 79, 79};
  synth_frames_.push_back(frame1);

  Frame frame2 = {250, 250, true, 100, 0, NULL, 0, -1, 3, 90, 90};
  synth_frames_.push_back(frame2);

  SynthesizeAndRead("multiple_frame_opaque_2nd_falling_off_image",
                    DefineImage());
}

TEST_F(GifAnimationTest, ReadMultipleFrameOpaqueFirstFallingOffImageAtOrigin) {
  Frame frame1 = {250, 250, true, 100, 0, NULL, 0, -1, 3, 0, 0};
  synth_frames_.push_back(frame1);

  Frame frame2 = {20, 20, false, 100, 0, NULL, 0, -1, 2, 79, 79};
  synth_frames_.push_back(frame2);

  SynthesizeAndRead("multiple_frame_opaque_1st_falling_off_image_at_origin",
                    DefineImage());
}

TEST_F(GifAnimationTest, ReadMultipleFrameOpaqueFirstFallingOffXImage) {
  Frame frame1 = {250, 20, false, 100, 0, NULL, 0, -1, 3, 10, 10};
  synth_frames_.push_back(frame1);

  Frame frame2 = {20, 20, false, 100, 0, NULL, 0, -1, 2, 79, 79};
  synth_frames_.push_back(frame2);

  SynthesizeAndRead("multiple_frame_opaque_1st_falling_off_x_image",
                    DefineImage());
}

TEST_F(GifAnimationTest, ReadMultipleFrameOpaqueFirstFallingOffYImage) {
  Frame frame1 = {20, 250, false, 100, 0, NULL, 0, -1, 3, 10, 10};
  synth_frames_.push_back(frame1);

  Frame frame2 = {20, 20, false, 100, 0, NULL, 0, -1, 2, 79, 79};
  synth_frames_.push_back(frame2);

  SynthesizeAndRead("multiple_frame_opaque_1st_falling_off_y_image",
                    DefineImage());
}

TEST_F(GifAnimationTest, ReadMultipleFrameOpaqueSecondFallingOffImageAtOrigin) {
  Frame frame1 = {20, 20, false, 100, 0, NULL, 0, -1, 2, 79, 79};
  synth_frames_.push_back(frame1);

  Frame frame2 = {250, 250, false, 100, 0, NULL, 0, -1, 3, 0, 0};
  synth_frames_.push_back(frame2);

  SynthesizeAndRead("multiple_frame_opaque_2nd_falling_off_image_at_origin",
                    DefineImage());
}

TEST_F(GifAnimationTest, ReadMultipleFrameOpaqueNoDelay) {
  Frame frame1 = {20, 20, false, 0, 1, NULL, 0, -1, 2, 10, 10};
  synth_frames_.push_back(frame1);

  Frame frame2 = {20, 20, false, 0, 2, NULL, 0, -1, 3, 20, 20};
  synth_frames_.push_back(frame2);

  SynthesizeAndRead("multiple_frame_opaque_nodelay", DefineImage());
}

TEST_F(GifAnimationTest, ReadMultipleFrameTransparency) {
  Frame frame1 = {20, 20, false, 100, 0, NULL, 0, -1, 2, 10, 10};
  synth_frames_.push_back(frame1);

  Frame frame2 = {20, 20, false, 100, 0, NULL, 0, 3, 3, 20, 20};
  synth_frames_.push_back(frame2);

  Frame frame3 = {20, 20, false, 100, 0, NULL, 0, 2, 3, 25, 25};
  synth_frames_.push_back(frame3);

  SynthesizeAndRead("multiple_frame_transparency", DefineImage());
}

TEST_F(GifAnimationTest, ReadMultipleFrameNoDelay2FrameOpaque) {
  // Tests that the transparency information for one frame is not
  // carried to the next. By setting the transparent index, delay, and
  // disposal to -1, the synthesized image will not have a Graphic
  // Control Extension for the given frame, and we can test that
  // transparent index was not carried over.
  const int frame1_transparent_idx = 3;
  const int frame2_color_idx = frame1_transparent_idx;

  Frame frame1 =
      {20, 20, false, 0, 0, NULL, 0, frame1_transparent_idx, 2, 10, 10};
  synth_frames_.push_back(frame1);

  Frame frame2 =
      {20, 20, false, -1, -1, NULL, 0, -1, frame2_color_idx, 20, 20};
  synth_frames_.push_back(frame2);

  SynthesizeAndRead("multiple_frame_transparency_no_delay_2frame_opaque",
                    DefineImage());
}

TEST_F(GifAnimationTest, ReadMultipleFrameTransparencySkipScanlines) {
  read_all_scanlines_ = false;

  Frame frame1 = {20, 20, false, 100, 0, NULL, 0, -1, 2, 10, 10};
  synth_frames_.push_back(frame1);

  Frame frame2 = {20, 20, false, 100, 0, NULL, 0, 3, 3, 20, 20};
  synth_frames_.push_back(frame2);

  Frame frame3 = {20, 20, false, 100, 0, NULL, 0, 2, 3, 25, 25};
  synth_frames_.push_back(frame3);

  SynthesizeAndRead("multiple_frame_transparency_skip_scanlines",
                    DefineImage());
}

TEST_F(GifAnimationTest, ReadMultipleFrameTransparencyMixInterlaced) {
  Frame frame1 = {20, 20, false, 100, 0, NULL, 0, -1, 2, 10, 10};
  synth_frames_.push_back(frame1);

  Frame frame2 = {20, 20, true, 100, 0, NULL, 0, 3, 3, 20, 20};
  synth_frames_.push_back(frame2);

  Frame frame3 = {20, 20, false, 100, 0, NULL, 0, 2, 3, 25, 25};
  synth_frames_.push_back(frame3);

  SynthesizeAndRead("multiple_frame_transparency_mix_interlaced",
                    DefineImage());
}

TEST_F(GifAnimationTest, ReadMultipleFrameTransparencyMixColormaps) {
  Frame frame1 = {20, 20, false, 100, 0, NULL, 0, -1, 2, 10, 10};
  synth_frames_.push_back(frame1);

  Frame frame2 =
      {20, 20, false, 100, 0, kAlternateColorMap, kNumColors, 3, 3, 20, 20};
  synth_frames_.push_back(frame2);

  Frame frame3 = {20, 20, false, 100, 0, NULL, 0, 2, 3, 25, 25};
  synth_frames_.push_back(frame3);

  SynthesizeAndRead("multiple_frame_transparency_mix_colormaps", DefineImage());
}

TEST_F(GifAnimationTest, ReadMultipleFrameOpaqueFallingOffImage) {
  Frame frame1 = {10, 10, false, 0, 2, NULL, 0, -1, 2, 93, 93};
  synth_frames_.push_back(frame1);

  Frame frame2 = {10, 10, false, 0, 2, NULL, 0, -1, 4, 95, 95};
  synth_frames_.push_back(frame2);

  SynthesizeAndRead("multiple_frame_opaque_falling_off_image", DefineImage());
}

TEST_F(GifAnimationTest, ReadMultipleFrameTransparencyFallingOffImage) {
  Frame frame1 = {10, 10, false, 0, 3, NULL, 0, 4, 2, 93, 93};
  synth_frames_.push_back(frame1);

  Frame frame2 = {10, 10, false, 0, 3, NULL, 0, 4, 4, 95, 95};
  synth_frames_.push_back(frame2);

  SynthesizeAndRead("multiple_frame_transparent_falling_off_image",
                    DefineImage());
}

TEST_F(GifAnimationTest, ReadMultipleFrameOpaqueDisposal) {
  Frame frame1 = {20, 20, false, 100, 0, NULL, 0, -1, 2, 10, 10};
  synth_frames_.push_back(frame1);

  Frame frame2 = {20, 20, false, 100, 0, NULL, 0, -1, 3, 20, 20};
  synth_frames_.push_back(frame2);

  Frame frame3 = {20, 20, false, 100, 2, NULL, 0, -1, 4, 15, 15};
  synth_frames_.push_back(frame3);

  Frame frame4 = {20, 20, false, 100, 3, NULL, 0, -1, 1, 0, 0};
  synth_frames_.push_back(frame4);

  Frame frame5 = {20, 20, false, 100, 1, NULL, 0, -1, 5, 30, 30};
  synth_frames_.push_back(frame5);

  SynthesizeAndRead("multiple_frame_opaque_disposal", DefineImage());
}

TEST_F(GifAnimationTest, ReadMultipleFrameTransparencyLoopInfinite) {
  Image image = DefineImage();
  image.loop_count = 0;

  Frame frame1 = {20, 20, false, 50, 0, NULL, 0, -1, 2, 10, 10};
  synth_frames_.push_back(frame1);

  Frame frame2 = {20, 20, false, 50, 0, NULL, 0, 3, 3, 20, 20};
  synth_frames_.push_back(frame2);

  Frame frame3 = {20, 20, false, 50, 0, NULL, 0, 2, 3, 25, 25};
  synth_frames_.push_back(frame3);

  SynthesizeAndRead("multiple_frame_transparency_loop_infinite", image);
}

TEST_F(GifAnimationTest, ReadMultipleFrameTransparencyNoDelayLoopInfinite) {
  Image image = DefineImage();
  image.loop_count = 0;

  Frame frame1 = {20, 20, false, 0, 1, NULL, 0, -1, 2, 10, 10};
  synth_frames_.push_back(frame1);

  Frame frame2 = {20, 20, false, 0, 1, NULL, 0, 3, 3, 20, 20};
  synth_frames_.push_back(frame2);

  Frame frame3 = {20, 20, false, 0, 1, NULL, 0, 2, 3, 25, 25};
  synth_frames_.push_back(frame3);

  SynthesizeAndRead("multiple_frame_transparency_nodelay_loop_infinite", image);
}

TEST_F(GifAnimationTest, ReadMultipleFrameTransparencyLoopThrice) {
  Image image = DefineImage();
  image.loop_count = 3;

  Frame frame1 = {20, 20, false, 50, 0, NULL, 0, -1, 2, 10, 10};
  synth_frames_.push_back(frame1);

  Frame frame2 = {20, 20, false, 50, 0, NULL, 0, 3, 3, 20, 20};
  synth_frames_.push_back(frame2);

  Frame frame3 = {20, 20, false, 50, 0, NULL, 0, 2, 3, 25, 25};
  synth_frames_.push_back(frame3);

  SynthesizeAndRead("multiple_frame_transparency_loop_thrice", image);
}
}  // namespace
