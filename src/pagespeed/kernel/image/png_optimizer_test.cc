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

// Author: Bryan McQuade

#include <stdbool.h>
#include <cstdlib>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/gif_reader.h"
#include "pagespeed/kernel/image/png_optimizer.h"
#include "pagespeed/kernel/image/read_image.h"
#include "pagespeed/kernel/image/scanline_utils.h"
#include "pagespeed/kernel/image/test_utils.h"

extern "C" {
#ifdef USE_SYSTEM_LIBPNG
#include "png.h"                                               // NOLINT
#else
#include "third_party/libpng/png.h"
#endif
}

namespace {

using net_instaweb::MockMessageHandler;
using net_instaweb::NullMutex;
using pagespeed::image_compression::kGifTestDir;
using pagespeed::image_compression::kPngSuiteTestDir;
using pagespeed::image_compression::kPngSuiteGifTestDir;
using pagespeed::image_compression::kPngTestDir;
using pagespeed::image_compression::kValidGifImageCount;
using pagespeed::image_compression::kValidGifImages;
using pagespeed::image_compression::GifReader;
using pagespeed::image_compression::ImageCompressionInfo;
using pagespeed::image_compression::IMAGE_PNG;
using pagespeed::image_compression::PixelFormat;
using pagespeed::image_compression::PngCompressParams;
using pagespeed::image_compression::PngOptimizer;
using pagespeed::image_compression::PngReader;
using pagespeed::image_compression::PngReaderInterface;
using pagespeed::image_compression::PngScanlineReaderRaw;
using pagespeed::image_compression::PngScanlineReader;
using pagespeed::image_compression::PngScanlineWriter;
using pagespeed::image_compression::ReadTestFile;
using pagespeed::image_compression::ScanlineReaderInterface;
using pagespeed::image_compression::ScanlineWriterInterface;
using pagespeed::image_compression::ScopedPngStruct;
using pagespeed::image_compression::kMessagePatternAnimatedGif;
using pagespeed::image_compression::kMessagePatternFailedToRead;
using pagespeed::image_compression::kMessagePatternLibpngError;
using pagespeed::image_compression::kMessagePatternLibpngWarning;
using pagespeed::image_compression::kMessagePatternUnexpectedEOF;

// Message to ignore.
const char kMessagePatternBadGifDescriptor[] =
    "Failed to get image descriptor.";
const char kMessagePatternBadGifLine[] = "Failed to DGifGetLine";
const char kMessagePatternUnrecognizedColor[] = "Unrecognized color type.";

// "rgb_alpha.png" and "gray_alpha.png" have the same image contents, but
// with different format.
const char kImageRGBA[] = "rgb_alpha";
const char kImageGA[] = "gray_alpha";

// Structure that holds metadata and actual pixel data for a decoded
// PNG.
struct ReadPngDescriptor {
  unsigned char* img_bytes;  // The actual pixel data.
  unsigned char* img_rgba_bytes;  // RGBA data for the image.
  unsigned long width;                                         // NOLINT
  unsigned long height;                                        // NOLINT
  int channels;              // 3 for RGB, 4 for RGB+alpha
  unsigned long row_bytes;   // number of bytes in a row       // NOLINT
  unsigned char bg_red, bg_green, bg_blue;
  bool bgcolor_retval;

  ReadPngDescriptor() : img_bytes(NULL), img_rgba_bytes(NULL) {}
  ~ReadPngDescriptor() {
    free(img_bytes);
    if (channels != 4) {
      free(img_rgba_bytes);
    }
  }
};

// Decode the PNG and store the decoded data and metadata in the
// ReadPngDescriptor struct.
void PopulateDescriptor(const GoogleString& img,
                        ReadPngDescriptor* desc,
                        const char* identifier) {
  MockMessageHandler message_handler(new NullMutex);
  PngScanlineReader scanline_reader(&message_handler);
  scanline_reader.set_transform(
      // Expand paletted colors into true RGB triplets
      // Expand grayscale images to full 8 bits from 1, 2, or 4 bits/pixel
      // Expand paletted or RGB images with transparency to full alpha
      // channels so the data will be available as RGBA quartets.
      PNG_TRANSFORM_EXPAND |
      // Downsample images with 16bits per channel to 8bits per channel.
      PNG_TRANSFORM_STRIP_16 |
      // Convert grayscale images to RGB images.
      PNG_TRANSFORM_GRAY_TO_RGB);

  if (setjmp(*scanline_reader.GetJmpBuf())) {
    FAIL();
  }
  PngReader reader(&message_handler);
  if (!scanline_reader.InitializeRead(reader, img)) {
    FAIL();
  }

  int channels;
  switch (scanline_reader.GetPixelFormat()) {
    case pagespeed::image_compression::RGB_888:
      channels = 3;
      break;
    case pagespeed::image_compression::RGBA_8888:
      channels = 4;
      break;
    case pagespeed::image_compression::GRAY_8:
      channels = 1;
      break;
    default:
      PS_LOG_INFO((&message_handler), \
         "Unexpected pixel format: %d", scanline_reader.GetPixelFormat());
      channels = -1;
      break;
  }

  desc->width = scanline_reader.GetImageWidth();
  desc->height = scanline_reader.GetImageHeight();
  desc->channels = channels;
  desc->row_bytes = scanline_reader.GetBytesPerScanline();
  desc->img_bytes = static_cast<unsigned char*>(
      malloc(desc->row_bytes * desc->height));
  if (channels == 4) {
    desc->img_rgba_bytes = desc->img_bytes;
  } else {
    desc->img_rgba_bytes = static_cast<unsigned char*>(
        malloc(4 * desc->width * desc->height));
  }
  desc->bgcolor_retval = scanline_reader.GetBackgroundColor(
      &desc->bg_red, &desc->bg_green, &desc->bg_blue);

  unsigned char* next_scanline_to_write = desc->img_bytes;
  while (scanline_reader.HasMoreScanLines()) {
    void* scanline = NULL;
    if (scanline_reader.ReadNextScanline(&scanline)) {
      memcpy(next_scanline_to_write, scanline,
             scanline_reader.GetBytesPerScanline());
      next_scanline_to_write += scanline_reader.GetBytesPerScanline();
    }
  }
  if (channels != 4) {
    memset(desc->img_rgba_bytes, 0xff, 4 * desc->height * desc->width);
    unsigned char* next_img_byte = desc->img_bytes;
    unsigned char* next_rgba_byte = desc->img_rgba_bytes;
    for (unsigned long pixel_count = 0;             // NOLINT
         pixel_count < desc->height * desc->width;
         ++pixel_count) {
      if (channels == 3) {
        memcpy(next_rgba_byte, next_img_byte, channels);
      } else if (channels == 1) {
        memset(next_rgba_byte, *next_img_byte, 3);
      } else {
        FAIL() << "Unexpected number of channels: " << channels;
      }
      next_img_byte += channels;
      next_rgba_byte += 4;
    }
  }
}

void AssertPngEq(
    const GoogleString& orig, const GoogleString& opt, const char* identifier,
    const GoogleString& in_rgba) {
  // Gather data and metadata for the original and optimized PNGs.
  ReadPngDescriptor orig_desc;
  PopulateDescriptor(orig, &orig_desc, identifier);
  ReadPngDescriptor opt_desc;
  PopulateDescriptor(opt, &opt_desc, identifier);

  // Verify that the dimensions match.
  EXPECT_EQ(orig_desc.width, opt_desc.width)
      << "width mismatch for " << identifier;
  EXPECT_EQ(orig_desc.height, opt_desc.height)
      << "height mismatch for " << identifier;

  // If PNG background chunks are supported, verify that the
  // background chunks are not present in the optimized image.
#if defined(PNG_bKGD_SUPPORTED) || defined(PNG_READ_BACKGROUND_SUPPORTED)
  EXPECT_FALSE(opt_desc.bgcolor_retval) << "Unexpected: bgcolor";
#endif

  // Verify that the number of channels matches (should be 3 for RGB
  // or 4 for RGB+alpha)
  EXPECT_EQ(orig_desc.channels, opt_desc.channels)
      << "channel mismatch for " << identifier;

  // Verify that the number of bytes in a row matches.
  EXPECT_EQ(orig_desc.row_bytes, opt_desc.row_bytes)
      << "row_bytes mismatch for " << identifier;

  // Verify that the actual image data matches.
  if (orig_desc.row_bytes == opt_desc.row_bytes &&
      orig_desc.height == opt_desc.height) {
    const unsigned long img_bytes_size =   // NOLINT
      orig_desc.row_bytes * orig_desc.height;
    EXPECT_EQ(0,
              memcmp(orig_desc.img_bytes, opt_desc.img_bytes, img_bytes_size))
        << "image data mismatch for " << identifier;
  }

  if (!in_rgba.empty()) {
    unsigned long num_rgba_bytes = // NOLINT
        4 * opt_desc.height * opt_desc.width;
    EXPECT_EQ(in_rgba.length(), num_rgba_bytes)
        << "rgba data size mismatch for " << identifier;
    EXPECT_EQ(0,
              memcmp(opt_desc.img_rgba_bytes, in_rgba.c_str(), num_rgba_bytes))
        << "rgba data bytes mismatch for " << identifier;
  }
}

void AssertReadersMatch(ScanlineReaderInterface* reader1,
                        ScanlineReaderInterface* reader2) {
  MockMessageHandler message_handler(new NullMutex);

  // Make sure the images sizes and the pixel formats are the same.
  ASSERT_EQ(reader1->GetImageWidth(), reader2->GetImageWidth());
  ASSERT_EQ(reader1->GetImageHeight(), reader2->GetImageHeight());
  ASSERT_EQ(reader1->GetPixelFormat(), reader2->GetPixelFormat());

  const int width = reader1->GetImageWidth();
  const int num_channels =
    GetNumChannelsFromPixelFormat(reader1->GetPixelFormat(),
                                  &message_handler);
  uint8* pixels1 = NULL;
  uint8* pixels2 = NULL;

  // Decode and check the image a scanline at a time.
  while (reader1->HasMoreScanLines() &&
         reader2->HasMoreScanLines()) {
    ASSERT_TRUE(reader1->ReadNextScanline(
      reinterpret_cast<void**>(&pixels2)));

    ASSERT_TRUE(reader2->ReadNextScanline(
      reinterpret_cast<void**>(&pixels1)));

    for (int i = 0; i < width*num_channels; ++i) {
      ASSERT_EQ(pixels1[i], pixels2[i]);
    }
  }

  // Make sure both readers have exhausted all scanlines.
  ASSERT_FALSE(reader1->HasMoreScanLines());
  ASSERT_FALSE(reader2->HasMoreScanLines());
}

// These images were obtained from
// http://www.libpng.org/pub/png/pngsuite.html
ImageCompressionInfo kValidImages[] = {
  ImageCompressionInfo("basi0g01", 217, 208, 217, 32, 32, 1, 0, 1, 0),
  ImageCompressionInfo("basi0g02", 154, 154, 154, 32, 32, 2, 0, 2, 0),
  ImageCompressionInfo("basi0g04", 247, 145, 247, 32, 32, 4, 0, 4, 0),
  ImageCompressionInfo("basi0g08", 254, 250, 799, 32, 32, 8, 0, 8, 0),
  ImageCompressionInfo("basi0g16", 299, 285, 1223, 32, 32, 16, 0, 16, 0),
  ImageCompressionInfo("basi2c08", 315, 313, 1509, 32, 32, 8, 2, 8, 2),
  ImageCompressionInfo("basi2c16", 595, 557, 2863, 32, 32, 16, 2, 16, 2),
  ImageCompressionInfo("basi3p01", 132, 132, 132, 32, 32, 1, 3, 1, 3),
  ImageCompressionInfo("basi3p02", 193, 178, 178, 32, 32, 2, 3, 2, 3),
  ImageCompressionInfo("basi3p04", 327, 312, 312, 32, 32, 4, 3, 4, 3),
  ImageCompressionInfo("basi3p08", 1527, 1518, 1527, 32, 32, 8, 3, 8, 3),
  ImageCompressionInfo("basi4a08", 214, 209, 1450, 32, 32, 8, 4, 8, 4),
  ImageCompressionInfo("basi4a16", 2855, 1980, 1980, 32, 32, 16, 4, 16, 4),
  ImageCompressionInfo("basi6a08", 361, 350, 1591, 32, 32, 8, 6, 8, 6),
  ImageCompressionInfo("basi6a16", 4180, 4133, 4423, 32, 32, 16, 6, 16, 6),
  ImageCompressionInfo("basn0g01", 164, 164, 164, 32, 32, 1, 0, 1, 0),
  ImageCompressionInfo("basn0g02", 104, 104, 104, 32, 32, 2, 0, 2, 0),
  ImageCompressionInfo("basn0g04", 145, 103, 145, 32, 32, 4, 0, 4, 0),
  ImageCompressionInfo("basn0g08", 138, 132, 730, 32, 32, 8, 0, 8, 0),
  ImageCompressionInfo("basn0g16", 167, 152, 645, 32, 32, 16, 0, 16, 0),
  ImageCompressionInfo("basn2c08", 145, 145, 1441, 32, 32, 8, 2, 8, 2),
  ImageCompressionInfo("basn2c16", 302, 274, 2687, 32, 32, 16, 2, 16, 2),
  ImageCompressionInfo("basn3p01", 112, 112, 112, 32, 32, 1, 3, 1, 3),
  ImageCompressionInfo("basn3p02", 146, 131, 131, 32, 32, 2, 3, 2, 3),
  ImageCompressionInfo("basn3p04", 216, 201, 201, 32, 32, 4, 3, 4, 3),
  ImageCompressionInfo("basn3p08", 1286, 1286, 1286, 32, 32, 8, 3, 8, 3),
  ImageCompressionInfo("basn4a08", 126, 121, 1433, 32, 32, 8, 4, 8, 4),
  ImageCompressionInfo("basn4a16", 2206, 1185, 1185, 32, 32, 16, 4, 16, 4),
  ImageCompressionInfo("basn6a08", 184, 176, 1435, 32, 32, 8, 6, 8, 6),
  ImageCompressionInfo("basn6a16", 3435, 3271, 4181, 32, 32, 16, 6, 16, 6),
  ImageCompressionInfo("bgai4a08", 214, 209, 1450, 32, 32, 8, 4, 8, 4),
  ImageCompressionInfo("bgai4a16", 2855, 1980, 1980, 32, 32, 16, 4, 16, 4),
  ImageCompressionInfo("bgan6a08", 184, 176, 1435, 32, 32, 8, 6, 8, 6),
  ImageCompressionInfo("bgan6a16", 3435, 3271, 4181, 32, 32, 16, 6, 16, 6),
  ImageCompressionInfo("bgbn4a08", 140, 121, 1433, 32, 32, 8, 4, 8, 4),
  ImageCompressionInfo("bggn4a16", 2220, 1185, 1185, 32, 32, 16, 4, 16, 4),
  ImageCompressionInfo("bgwn6a08", 202, 176, 1435, 32, 32, 8, 6, 8, 6),
  ImageCompressionInfo("bgyn6a16", 3453, 3271, 4181, 32, 32, 16, 6, 16, 6),
  ImageCompressionInfo("ccwn2c08", 1514, 1456, 1742, 32, 32, 8, 2, 8, 2),
  ImageCompressionInfo("ccwn3p08", 1554, 1499, 1510, 32, 32, 8, 3, 8, 3),
  ImageCompressionInfo("cdfn2c08", 404, 498, 532, 8, 32, 8, 2, 8, 3),
  ImageCompressionInfo("cdhn2c08", 344, 476, 491, 32, 8, 8, 2, 8, 3),
  ImageCompressionInfo("cdsn2c08", 232, 255, 258, 8, 8, 8, 2, 8, 3),
  ImageCompressionInfo("cdun2c08", 724, 928, 942, 32, 32, 8, 2, 8, 3),
  ImageCompressionInfo("ch1n3p04", 258, 201, 201, 32, 32, 4, 3, 4, 3),
  ImageCompressionInfo("ch2n3p08", 1810, 1286, 1286, 32, 32, 8, 3, 8, 3),
  ImageCompressionInfo("cm0n0g04", 292, 271, 273, 32, 32, 4, 0, 4, 0),
  ImageCompressionInfo("cm7n0g04", 292, 271, 273, 32, 32, 4, 0, 4, 0),
  ImageCompressionInfo("cm9n0g04", 292, 271, 273, 32, 32, 4, 0, 4, 0),
  ImageCompressionInfo("cs3n2c16", 214, 178, 216, 32, 32, 16, 2, 16, 2),
  ImageCompressionInfo("cs3n3p08", 259, 244, 244, 32, 32, 8, 3, 8, 3),
  ImageCompressionInfo("cs5n2c08", 186, 226, 256, 32, 32, 8, 2, 8, 3),
  ImageCompressionInfo("cs5n3p08", 271, 256, 256, 32, 32, 8, 3, 8, 3),
  ImageCompressionInfo("cs8n2c08", 149, 226, 256, 32, 32, 8, 2, 8, 3),
  ImageCompressionInfo("cs8n3p08", 256, 256, 256, 32, 32, 8, 3, 8, 3),
  ImageCompressionInfo("ct0n0g04", 273, 271, 273, 32, 32, 4, 0, 4, 0),
  ImageCompressionInfo("ct1n0g04", 792, 271, 273, 32, 32, 4, 0, 4, 0),
  ImageCompressionInfo("ctzn0g04", 753, 271, 273, 32, 32, 4, 0, 4, 0),
  ImageCompressionInfo("f00n0g08", 319, 312, 319, 32, 32, 8, 0, 8, 0),
  ImageCompressionInfo("f00n2c08", 2475, 1070, 2475, 32, 32, 8, 2, 8, 2),
  ImageCompressionInfo("f01n0g08", 321, 246, 283, 32, 32, 8, 0, 8, 0),
  ImageCompressionInfo("f01n2c08", 1180, 965, 2546, 32, 32, 8, 2, 8, 2),
  ImageCompressionInfo("f02n0g08", 355, 289, 297, 32, 32, 8, 0, 8, 0),
  ImageCompressionInfo("f02n2c08", 1729, 1024, 2512, 32, 32, 8, 2, 8, 2),
  ImageCompressionInfo("f03n0g08", 389, 292, 296, 32, 32, 8, 0, 8, 0),
  ImageCompressionInfo("f03n2c08", 1291, 1062, 2509, 32, 32, 8, 2, 8, 2),
  ImageCompressionInfo("f04n0g08", 269, 273, 281, 32, 32, 8, 0, 8, 0),
  ImageCompressionInfo("f04n2c08", 985, 985, 2546, 32, 32, 8, 2, 8, 2),
  ImageCompressionInfo("g03n0g16", 345, 273, 308, 32, 32, 16, 0, 8, 0),
  ImageCompressionInfo("g03n2c08", 370, 396, 490, 32, 32, 8, 2, 8, 3),
  ImageCompressionInfo("g03n3p04", 214, 214, 214, 32, 32, 4, 3, 4, 3),
  ImageCompressionInfo("g04n0g16", 363, 287, 310, 32, 32, 16, 0, 8, 0),
  ImageCompressionInfo("g04n2c08", 377, 399, 493, 32, 32, 8, 2, 8, 3),
  ImageCompressionInfo("g04n3p04", 219, 219, 219, 32, 32, 4, 3, 4, 3),
  ImageCompressionInfo("g05n0g16", 339, 275, 306, 32, 32, 16, 0, 8, 0),
  ImageCompressionInfo("g05n2c08", 350, 402, 488, 32, 32, 8, 2, 8, 3),
  ImageCompressionInfo("g05n3p04", 206, 206, 206, 32, 32, 4, 3, 4, 3),
  ImageCompressionInfo("g07n0g16", 321, 261, 305, 32, 32, 16, 0, 8, 0),
  ImageCompressionInfo("g07n2c08", 340, 401, 488, 32, 32, 8, 2, 8, 3),
  ImageCompressionInfo("g07n3p04", 207, 207, 207, 32, 32, 4, 3, 4, 3),
  ImageCompressionInfo("g10n0g16", 262, 210, 306, 32, 32, 16, 0, 8, 0),
  ImageCompressionInfo("g10n2c08", 285, 403, 495, 32, 32, 8, 2, 8, 3),
  ImageCompressionInfo("g10n3p04", 214, 214, 214, 32, 32, 4, 3, 4, 3),
  ImageCompressionInfo("g25n0g16", 383, 305, 305, 32, 32, 16, 0, 8, 0),
  ImageCompressionInfo("g25n2c08", 405, 399, 470, 32, 32, 8, 2, 8, 3),
  ImageCompressionInfo("g25n3p04", 215, 215, 215, 32, 32, 4, 3, 4, 3),
  ImageCompressionInfo("oi1n0g16", 167, 152, 645, 32, 32, 16, 0, 16, 0),
  ImageCompressionInfo("oi1n2c16", 302, 274, 2687, 32, 32, 16, 2, 16, 2),
  ImageCompressionInfo("oi2n0g16", 179, 152, 645, 32, 32, 16, 0, 16, 0),
  ImageCompressionInfo("oi2n2c16", 314, 274, 2687, 32, 32, 16, 2, 16, 2),
  ImageCompressionInfo("oi4n0g16", 203, 152, 645, 32, 32, 16, 0, 16, 0),
  ImageCompressionInfo("oi4n2c16", 338, 274, 2687, 32, 32, 16, 2, 16, 2),
  ImageCompressionInfo("oi9n0g16", 1283, 152, 645, 32, 32, 16, 0, 16, 0),
  ImageCompressionInfo("oi9n2c16", 3038, 274, 2687, 32, 32, 16, 2, 16, 2),
  ImageCompressionInfo("pp0n2c16", 962, 934, 3347, 32, 32, 16, 2, 16, 2),
  ImageCompressionInfo("pp0n6a08", 818, 818, 3666, 32, 32, 8, 6, 8, 6),
  ImageCompressionInfo("ps1n0g08", 1477, 132, 730, 32, 32, 8, 0, 8, 0),
  ImageCompressionInfo("ps1n2c16", 1641, 274, 2687, 32, 32, 16, 2, 16, 2),
  ImageCompressionInfo("ps2n0g08", 2341, 132, 730, 32, 32, 8, 0, 8, 0),
  ImageCompressionInfo("ps2n2c16", 2505, 274, 2687, 32, 32, 16, 2, 16, 2),
  ImageCompressionInfo("s01i3p01", 113, 98, 98, 1, 1, 1, 3, 1, 3),
  ImageCompressionInfo("s01n3p01", 113, 98, 98, 1, 1, 1, 3, 1, 3),
  ImageCompressionInfo("s02i3p01", 114, 99, 99, 2, 2, 1, 3, 1, 3),
  ImageCompressionInfo("s02n3p01", 115, 100, 100, 2, 2, 1, 3, 1, 3),
  ImageCompressionInfo("s03i3p01", 118, 103, 103, 3, 3, 1, 3, 1, 3),
  ImageCompressionInfo("s03n3p01", 120, 105, 105, 3, 3, 1, 3, 1, 3),
  ImageCompressionInfo("s04i3p01", 126, 111, 111, 4, 4, 1, 3, 1, 3),
  ImageCompressionInfo("s04n3p01", 121, 106, 106, 4, 4, 1, 3, 1, 3),
  ImageCompressionInfo("s05i3p02", 134, 119, 119, 5, 5, 2, 3, 2, 3),
  ImageCompressionInfo("s05n3p02", 129, 114, 114, 5, 5, 2, 3, 2, 3),
  ImageCompressionInfo("s06i3p02", 143, 128, 128, 6, 6, 2, 3, 2, 3),
  ImageCompressionInfo("s06n3p02", 131, 116, 116, 6, 6, 2, 3, 2, 3),
  ImageCompressionInfo("s07i3p02", 149, 134, 134, 7, 7, 2, 3, 2, 3),
  ImageCompressionInfo("s07n3p02", 138, 123, 123, 7, 7, 2, 3, 2, 3),
  ImageCompressionInfo("s08i3p02", 149, 134, 134, 8, 8, 2, 3, 2, 3),
  ImageCompressionInfo("s08n3p02", 139, 124, 124, 8, 8, 2, 3, 2, 3),
  ImageCompressionInfo("s09i3p02", 147, 132, 132, 9, 9, 2, 3, 2, 3),
  ImageCompressionInfo("s09n3p02", 143, 128, 128, 9, 9, 2, 3, 2, 3),
  ImageCompressionInfo("s32i3p04", 355, 340, 340, 32, 32, 4, 3, 4, 3),
  ImageCompressionInfo("s32n3p04", 263, 248, 248, 32, 32, 4, 3, 4, 3),
  ImageCompressionInfo("s33i3p04", 385, 370, 370, 33, 33, 4, 3, 4, 3),
  ImageCompressionInfo("s33n3p04", 329, 314, 314, 33, 33, 4, 3, 4, 3),
  ImageCompressionInfo("s34i3p04", 349, 332, 334, 34, 34, 4, 3, 4, 3),
  ImageCompressionInfo("s34n3p04", 248, 229, 233, 34, 34, 4, 3, 4, 3),
  ImageCompressionInfo("s35i3p04", 399, 384, 384, 35, 35, 4, 3, 4, 3),
  ImageCompressionInfo("s35n3p04", 338, 313, 323, 35, 35, 4, 3, 4, 3),
  ImageCompressionInfo("s36i3p04", 356, 339, 341, 36, 36, 4, 3, 4, 3),
  ImageCompressionInfo("s36n3p04", 258, 240, 243, 36, 36, 4, 3, 4, 3),
  ImageCompressionInfo("s37i3p04", 393, 378, 378, 37, 37, 4, 3, 4, 3),
  ImageCompressionInfo("s37n3p04", 336, 317, 321, 37, 37, 4, 3, 4, 3),
  ImageCompressionInfo("s38i3p04", 357, 339, 342, 38, 38, 4, 3, 4, 3),
  ImageCompressionInfo("s38n3p04", 245, 228, 230, 38, 38, 4, 3, 4, 3),
  ImageCompressionInfo("s39i3p04", 420, 405, 405, 39, 39, 4, 3, 4, 3),
  ImageCompressionInfo("s39n3p04", 352, 336, 337, 39, 39, 4, 3, 4, 3),
  ImageCompressionInfo("s40i3p04", 357, 340, 342, 40, 40, 4, 3, 4, 3),
  ImageCompressionInfo("s40n3p04", 256, 237, 241, 40, 40, 4, 3, 4, 3),
  ImageCompressionInfo("tbbn1g04", 419, 405, 405, 32, 32, 4, 0, 4, 0),
  ImageCompressionInfo("tbbn2c16", 1994, 1095, 1113, 32, 32, 16, 2, 8, 3),
  ImageCompressionInfo("tbbn3p08", 1128, 1095, 1115, 32, 32, 8, 3, 8, 3),
  ImageCompressionInfo("tbgn2c16", 1994, 1095, 1113, 32, 32, 16, 2, 8, 3),
  ImageCompressionInfo("tbgn3p08", 1128, 1095, 1115, 32, 32, 8, 3, 8, 3),
  ImageCompressionInfo("tbrn2c08", 1347, 1095, 1113, 32, 32, 8, 2, 8, 3),
  ImageCompressionInfo("tbwn1g16", 1146, 582, 599, 32, 32, 16, 0, 8, 0),
  ImageCompressionInfo("tbwn3p08", 1131, 1095, 1115, 32, 32, 8, 3, 8, 3),
  ImageCompressionInfo("tbyn3p08", 1131, 1095, 1115, 32, 32, 8, 3, 8, 3),
  ImageCompressionInfo("tp0n1g08", 689, 568, 585, 32, 32, 8, 0, 8, 0),
  ImageCompressionInfo("tp0n2c08", 1311, 1099, 1119, 32, 32, 8, 2, 8, 3),
  ImageCompressionInfo("tp0n3p08", 1120, 1098, 1120, 32, 32, 8, 3, 8, 3),
  ImageCompressionInfo("tp1n3p08", 1115, 1095, 1115, 32, 32, 8, 3, 8, 3),
  ImageCompressionInfo("z00n2c08", 3172, 224, 1956, 32, 32, 8, 2, 8, 2),
  ImageCompressionInfo("z03n2c08", 232, 224, 1956, 32, 32, 8, 2, 8, 2),
  ImageCompressionInfo("z06n2c08", 224, 224, 1956, 32, 32, 8, 2, 8, 2),
  ImageCompressionInfo("z09n2c08", 224, 224, 1956, 32, 32, 8, 2, 8, 2),
};

const char* kInvalidFiles[] = {
  "emptyfile",
  "x00n0g01",
  "xcrn0g04",
  "xlfn0g04",
};

struct OpaqueImageInfo {
  const char* filename;
  bool is_opaque;
  int in_color_type;
  int out_color_type;
};

OpaqueImageInfo kOpaqueImagesWithAlpha[] = {
  { "rgba_opaque", 1, 6, 2 },
  { "grey_alpha_opaque", 1, 4, 0 },
  { "bgai4a16", 0, 4, 4 }
};

void AssertMatch(const GoogleString& in,
                 const GoogleString& ref,
                 PngReaderInterface* reader,
                 const ImageCompressionInfo& info,
                 GoogleString in_rgba) {
  MockMessageHandler message_handler(new NullMutex);
  PngReader png_reader(&message_handler);
  int width, height, bit_depth, color_type;
  GoogleString out;
  EXPECT_EQ(info.original_size, in.size())
      << info.filename;
  ASSERT_TRUE(reader->GetAttributes(
      in, &width, &height, &bit_depth, &color_type)) << info.filename;
  EXPECT_EQ(info.width, width) << info.filename;
  EXPECT_EQ(info.height, height) << info.filename;
  EXPECT_EQ(info.original_bit_depth, bit_depth) << info.filename;
  EXPECT_EQ(info.original_color_type, color_type) << info.filename;

  ASSERT_TRUE(PngOptimizer::OptimizePng(*reader, in, &out, &message_handler))
      << info.filename;
  EXPECT_EQ(info.compressed_size_default, out.size()) << info.filename;
  AssertPngEq(ref, out, info.filename, in_rgba);

  ASSERT_TRUE(png_reader.GetAttributes(
      out, &width, &height, &bit_depth, &color_type)) << info.filename;
  EXPECT_EQ(info.compressed_bit_depth, bit_depth) << info.filename;
  EXPECT_EQ(info.compressed_color_type, color_type) << info.filename;

  ASSERT_TRUE(PngOptimizer::OptimizePngBestCompression(*reader, in, &out,
      &message_handler)) << info.filename;
  EXPECT_EQ(info.compressed_size_best, out.size()) << info.filename;
  AssertPngEq(ref, out, info.filename, in_rgba);

  ASSERT_TRUE(png_reader.GetAttributes(
      out, &width, &height, &bit_depth, &color_type)) << info.filename;
  EXPECT_EQ(info.compressed_bit_depth, bit_depth) << info.filename;
  EXPECT_EQ(info.compressed_color_type, color_type) << info.filename;
}

void AssertMatch(const GoogleString& in,
                 const GoogleString& ref,
                 PngReaderInterface* reader,
                 const ImageCompressionInfo& info) {
  static GoogleString in_rgba;
  AssertMatch(in, ref, reader, info, in_rgba);
}

bool InitializeEntireReader(const GoogleString& image_string,
                            const PngReader& png_reader,
                            PngScanlineReader* entire_image_reader) {
  // Initialize entire_image_reader.
  if (!entire_image_reader->Reset()) {
    return false;
  }
  entire_image_reader->set_transform(
    PNG_TRANSFORM_EXPAND |
    PNG_TRANSFORM_STRIP_16);

  if (entire_image_reader->InitializeRead(png_reader, image_string) == false) {
    return false;
  }

  // Skip the images which are not supported by PngScanlineReader.
  return (entire_image_reader->GetPixelFormat()
          != pagespeed::image_compression::UNSUPPORTED);
}

const size_t kValidImageCount = arraysize(kValidImages);
const size_t kInvalidFileCount = arraysize(kInvalidFiles);
const size_t kOpaqueImagesWithAlphaCount = arraysize(kOpaqueImagesWithAlpha);

class PngOptimizerTest : public testing::Test {
 public:
  PngOptimizerTest()
    : message_handler_(new NullMutex) {
  }

 protected:
  virtual void SetUp() {
    message_handler_.AddPatternToSkipPrinting(kMessagePatternAnimatedGif);
    message_handler_.AddPatternToSkipPrinting(kMessagePatternBadGifDescriptor);
    message_handler_.AddPatternToSkipPrinting(kMessagePatternBadGifLine);
    message_handler_.AddPatternToSkipPrinting(kMessagePatternFailedToRead);
    message_handler_.AddPatternToSkipPrinting(kMessagePatternLibpngError);
    message_handler_.AddPatternToSkipPrinting(kMessagePatternLibpngWarning);
    message_handler_.AddPatternToSkipPrinting(kMessagePatternUnexpectedEOF);
  }

 protected:
  MockMessageHandler message_handler_;
  scoped_ptr<PngReaderInterface> reader_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PngOptimizerTest);
};

class PngScanlineReaderRawTest : public testing::Test {
 public:
  PngScanlineReaderRawTest()
    : message_handler_(new NullMutex) {
  }

 protected:
  MockMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PngScanlineReaderRawTest);
};

class PngScanlineWriterTest : public testing::Test {
 public:
  PngScanlineWriterTest()
    : params_(PngCompressParams(PNG_FILTER_NONE, Z_DEFAULT_STRATEGY)),
      message_handler_(new NullMutex) {
  }

  bool Initialize() {
    writer_.reset(CreateScanlineWriter(
        pagespeed::image_compression::IMAGE_PNG, pixel_format_, width_,
        height_, &params_, &output_, &message_handler_));
    return (writer_ != NULL);
  }

 protected:
  scoped_ptr<ScanlineWriterInterface> writer_;
  GoogleString output_;
  PngCompressParams params_;
  unsigned char scanline_[3];
  static const int width_ = 3;
  static const int height_ = 2;
  static const PixelFormat pixel_format_ = pagespeed::image_compression::GRAY_8;
  MockMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PngScanlineWriterTest);
};

TEST_F(PngOptimizerTest, ValidPngs) {
  reader_.reset(new PngReader(&message_handler_));
  for (size_t i = 0; i < kValidImageCount; i++) {
    GoogleString in, out;
    ReadTestFile(kPngSuiteTestDir, kValidImages[i].filename, "png", &in);
    AssertMatch(in, in, reader_.get(), kValidImages[i]);
  }
}

TEST(PngScanlineReaderTest, InitializeRead_validPngs) {
  MockMessageHandler message_handler(new NullMutex);
  PngScanlineReader scanline_reader(&message_handler);
  if (setjmp(*scanline_reader.GetJmpBuf())) {
    ASSERT_FALSE(true) << "Execution should never reach here";
  }
  for (size_t i = 0; i < kValidImageCount; i++) {
    GoogleString in, out;
    ReadTestFile(kPngSuiteTestDir, kValidImages[i].filename, "png", &in);
    PngReader png_reader(&message_handler);
    ASSERT_TRUE(scanline_reader.Reset());

    int width, height, bit_depth, color_type;
    ASSERT_TRUE(png_reader.GetAttributes(
        in, &width, &height, &bit_depth, &color_type));

    EXPECT_EQ(kValidImages[i].original_color_type, color_type);
    ASSERT_TRUE(scanline_reader.InitializeRead(png_reader, in));
    EXPECT_EQ(kValidImages[i].original_color_type,
              scanline_reader.GetColorType());
  }

  for (size_t i = 0; i < kOpaqueImagesWithAlphaCount; i++) {
    GoogleString in, out;
    ReadTestFile(kPngSuiteTestDir, kOpaqueImagesWithAlpha[i].filename, "png",
                 &in);
    PngReader png_reader(&message_handler);
    ASSERT_TRUE(scanline_reader.Reset());

    int width, height, bit_depth, color_type;
    ASSERT_TRUE(png_reader.GetAttributes(
        in, &width, &height, &bit_depth, &color_type));

    EXPECT_EQ(kOpaqueImagesWithAlpha[i].in_color_type, color_type);
    ASSERT_TRUE(scanline_reader.InitializeRead(png_reader, in));
    EXPECT_EQ(kOpaqueImagesWithAlpha[i].out_color_type,
              scanline_reader.GetColorType());
  }
}

TEST_F(PngOptimizerTest, ValidPngs_isOpaque) {
  ScopedPngStruct read(ScopedPngStruct::READ, &message_handler_);

  for (size_t i = 0; i < kOpaqueImagesWithAlphaCount; i++) {
    GoogleString in, out;
    ReadTestFile(kPngSuiteTestDir, kOpaqueImagesWithAlpha[i].filename, "png",
                 &in);
    reader_.reset(new PngReader(&message_handler_));
    ASSERT_TRUE(reader_->ReadPng(in, read.png_ptr(), read.info_ptr(), 0));
    EXPECT_EQ(kOpaqueImagesWithAlpha[i].is_opaque,
              PngReaderInterface::IsAlphaChannelOpaque(
              read.png_ptr(), read.info_ptr(), &message_handler_));
    ASSERT_TRUE(read.reset());
  }
}

TEST_F(PngOptimizerTest, LargerPng) {
  reader_.reset(new PngReader(&message_handler_));
  GoogleString in, out;
  ReadTestFile(kPngTestDir, "this_is_a_test", "png", &in);
  ASSERT_EQ(static_cast<size_t>(20316), in.length());
  ASSERT_TRUE(PngOptimizer::OptimizePng(*reader_, in, &out,
      &message_handler_));

  int width, height, bit_depth, color_type;
  ASSERT_TRUE(reader_->GetAttributes(
      in, &width, &height, &bit_depth, &color_type));
  EXPECT_EQ(640, width);
  EXPECT_EQ(400, height);
  EXPECT_EQ(8, bit_depth);
  EXPECT_EQ(2, color_type);

  ASSERT_TRUE(reader_->GetAttributes(
      out, &width, &height, &bit_depth, &color_type));
  EXPECT_EQ(640, width);
  EXPECT_EQ(400, height);
  EXPECT_EQ(8, bit_depth);
  EXPECT_EQ(0, color_type);
}

TEST_F(PngOptimizerTest, InvalidPngs) {
  reader_.reset(new PngReader(&message_handler_));
  for (size_t i = 0; i < kInvalidFileCount; i++) {
    GoogleString in, out;
    ReadTestFile(kPngSuiteTestDir, kInvalidFiles[i], "png", &in);
    ASSERT_FALSE(PngOptimizer::OptimizePngBestCompression(*reader_,
        in, &out, &message_handler_));
    ASSERT_FALSE(PngOptimizer::OptimizePng(*reader_, in, &out,
        &message_handler_));

    int width, height, bit_depth, color_type;
    const bool get_attributes_result = reader_->GetAttributes(
        in, &width, &height, &bit_depth, &color_type);
    bool expected_get_attributes_result = false;
    if (strcmp("x00n0g01", kInvalidFiles[i]) == 0) {
      // Special case: even though the image is invalid, it has a
      // valid IDAT chunk, so we can read its attributes.
      expected_get_attributes_result = true;
    }
    EXPECT_EQ(expected_get_attributes_result, get_attributes_result)
        << kInvalidFiles[i];
  }
}

TEST_F(PngOptimizerTest, FixPngOutOfBoundReadCrash) {
  reader_.reset(new PngReader(&message_handler_));
  GoogleString in, out;
  ReadTestFile(kPngTestDir, "read_from_stream_crash", "png", &in);
  ASSERT_EQ(static_cast<size_t>(193), in.length());
  ASSERT_FALSE(PngOptimizer::OptimizePng(*reader_, in, &out,
      &message_handler_));

  int width, height, bit_depth, color_type;
  ASSERT_TRUE(reader_->GetAttributes(
      in, &width, &height, &bit_depth, &color_type));
  EXPECT_EQ(32, width);
  EXPECT_EQ(32, height);
  EXPECT_EQ(2, bit_depth);
  EXPECT_EQ(3, color_type);
}

TEST_F(PngOptimizerTest, PartialPng) {
  reader_.reset(new PngReader(&message_handler_));
  GoogleString in, out;
  int width, height, bit_depth, color_type;
  ReadTestFile(kPngTestDir, "pagespeed-128", "png", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  // Loop, removing the last byte repeatedly to generate every
  // possible partial version of the animated PNG.
  while (true) {
    if (in.size() == 0) {
      break;
    }
    // Remove the last byte.
    in.erase(in.length() - 1);
    EXPECT_FALSE(PngOptimizer::OptimizePng(*reader_, in, &out,
        &message_handler_));

    // See if we can extract image attributes. Doing so requires that
    // at least 33 bytes are available (signature plus full IDAT
    // chunk).
    bool png_header_available = (in.size() >= 33);
    bool get_attributes_result =
        reader_->GetAttributes(in, &width, &height, &bit_depth, &color_type);
    EXPECT_EQ(png_header_available, get_attributes_result) << in.size();
    if (get_attributes_result) {
      EXPECT_EQ(128, width);
      EXPECT_EQ(128, height);
      EXPECT_EQ(8, bit_depth);
      EXPECT_EQ(3, color_type);
    }
  }
}

TEST_F(PngOptimizerTest, ValidGifs) {
  reader_.reset(new GifReader(&message_handler_));
  for (size_t i = 0; i < kValidGifImageCount; i++) {
    GoogleString in, ref, gif_rgba;
    ReadTestFile(
        kPngSuiteGifTestDir, kValidGifImages[i].filename, "gif", &in);
    ReadTestFile(
        kPngSuiteGifTestDir, kValidGifImages[i].filename, "gif.rgba",
        &gif_rgba);
    ReadTestFile(kPngSuiteTestDir, kValidGifImages[i].filename, "png", &ref);
    AssertMatch(in, ref, reader_.get(), kValidGifImages[i], gif_rgba);
  }
}

TEST_F(PngOptimizerTest, AnimatedGif) {
  reader_.reset(new GifReader(&message_handler_));
  GoogleString in, out;
  ReadTestFile(kGifTestDir, "animated", "gif", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  ASSERT_FALSE(PngOptimizer::OptimizePng(*reader_, in, &out,
      &message_handler_));

  int width, height, bit_depth, color_type;
  ASSERT_TRUE(reader_->GetAttributes(
      in, &width, &height, &bit_depth, &color_type));
  EXPECT_EQ(120, width);
  EXPECT_EQ(50, height);
  EXPECT_EQ(8, bit_depth);
  EXPECT_EQ(3, color_type);
}

TEST_F(PngOptimizerTest, InterlacedGif) {
  reader_.reset(new GifReader(&message_handler_));
  GoogleString in, out;
  ReadTestFile(kGifTestDir, "interlaced", "gif", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  ASSERT_TRUE(PngOptimizer::OptimizePng(*reader_, in, &out,
      &message_handler_));

  int width, height, bit_depth, color_type;
  ASSERT_TRUE(reader_->GetAttributes(
      in, &width, &height, &bit_depth, &color_type));
  EXPECT_EQ(213, width);
  EXPECT_EQ(323, height);
  EXPECT_EQ(8, bit_depth);
  EXPECT_EQ(3, color_type);
}

TEST_F(PngOptimizerTest, TransparentGif) {
  reader_.reset(new GifReader(&message_handler_));
  GoogleString in, out;
  ReadTestFile(kGifTestDir, "transparent", "gif", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  ASSERT_TRUE(PngOptimizer::OptimizePng(*reader_, in, &out,
      &message_handler_));

  int width, height, bit_depth, color_type;
  ASSERT_TRUE(reader_->GetAttributes(
      in, &width, &height, &bit_depth, &color_type));
  EXPECT_EQ(320, width);
  EXPECT_EQ(320, height);
  EXPECT_EQ(8, bit_depth);
  EXPECT_EQ(3, color_type);
}

// Verify that we fail gracefully when processing partial versions of
// the animated GIF.
TEST_F(PngOptimizerTest, PartialAnimatedGif) {
  reader_.reset(new GifReader(&message_handler_));
  GoogleString in, out;
  int width, height, bit_depth, color_type;
  ReadTestFile(kGifTestDir, "animated", "gif", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  // Loop, removing the last byte repeatedly to generate every
  // possible partial version of the animated gif.
  while (true) {
    if (in.size() == 0) {
      break;
    }
    // Remove the last byte.
    in.erase(in.length() - 1);
    EXPECT_FALSE(PngOptimizer::OptimizePng(*reader_, in, &out,
        &message_handler_));

    // See if we can extract image attributes. Doing so requires that
    // at least 10 bytes are available.
    bool gif_header_available = (in.size() >= 10);
    bool get_attributes_result =
        reader_->GetAttributes(in, &width, &height, &bit_depth, &color_type);
    EXPECT_EQ(gif_header_available, get_attributes_result) << in.size();
    if (get_attributes_result) {
      EXPECT_EQ(120, width);
      EXPECT_EQ(50, height);
      EXPECT_EQ(8, bit_depth);
      EXPECT_EQ(3, color_type);
    }
  }
}

// Make sure we do not leak memory when attempting to optimize a GIF
// that fails to decode.
TEST_F(PngOptimizerTest, BadGifNoLeak) {
  reader_.reset(new GifReader(&message_handler_));
  GoogleString in, out;
  ReadTestFile(kGifTestDir, "bad", "gif", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  ASSERT_FALSE(PngOptimizer::OptimizePng(*reader_, in, &out,
                                         &message_handler_));
  int width, height, bit_depth, color_type;
  ASSERT_FALSE(reader_->GetAttributes(
      in, &width, &height, &bit_depth, &color_type));
}

TEST_F(PngOptimizerTest, InvalidGifs) {
  // Verify that we fail gracefully when trying to parse PNGs using
  // the GIF reader.
  reader_.reset(new GifReader(&message_handler_));
  for (size_t i = 0; i < kValidImageCount; i++) {
    GoogleString in, out;
    ReadTestFile(kPngSuiteTestDir, kValidImages[i].filename, "png", &in);
    ASSERT_FALSE(PngOptimizer::OptimizePng(*reader_, in, &out,
                                           &message_handler_));
    int width, height, bit_depth, color_type;
    ASSERT_FALSE(reader_->GetAttributes(
        in, &width, &height, &bit_depth, &color_type));
  }

  // Also verify we fail gracefully for the invalid PNG images.
  for (size_t i = 0; i < kInvalidFileCount; i++) {
    GoogleString in, out;
    ReadTestFile(kPngSuiteTestDir, kInvalidFiles[i], "png", &in);
    ASSERT_FALSE(PngOptimizer::OptimizePng(*reader_, in, &out,
                                           &message_handler_));
    int width, height, bit_depth, color_type;
    ASSERT_FALSE(reader_->GetAttributes(
        in, &width, &height, &bit_depth, &color_type));
  }
}

// Make sure that after we fail, we're still able to successfully
// compress valid images.
TEST_F(PngOptimizerTest, SuccessAfterFailure) {
  reader_.reset(new PngReader(&message_handler_));
  for (size_t i = 0; i < kInvalidFileCount; i++) {
    {
      GoogleString in, out;
      ReadTestFile(kPngSuiteTestDir, kInvalidFiles[i], "png", &in);
      ASSERT_FALSE(PngOptimizer::OptimizePng(*reader_, in, &out,
                                             &message_handler_));
    }

    {
      GoogleString in, out;
      ReadTestFile(kPngSuiteTestDir, kValidImages[i].filename, "png", &in);
      ASSERT_TRUE(PngOptimizer::OptimizePng(*reader_, in, &out,
                                            &message_handler_));
      int width, height, bit_depth, color_type;
      ASSERT_TRUE(reader_->GetAttributes(
          in, &width, &height, &bit_depth, &color_type));
    }
  }
}

TEST_F(PngOptimizerTest, ScopedPngStruct) {
  ScopedPngStruct read(ScopedPngStruct::READ, &message_handler_);
  ASSERT_TRUE(read.valid());
  ASSERT_NE(static_cast<png_structp>(NULL), read.png_ptr());
  ASSERT_NE(static_cast<png_infop>(NULL), read.info_ptr());

  ScopedPngStruct write(ScopedPngStruct::WRITE, &message_handler_);
  ASSERT_TRUE(write.valid());
  ASSERT_NE(static_cast<png_structp>(NULL), write.png_ptr());
  ASSERT_NE(static_cast<png_infop>(NULL), write.info_ptr());

#ifdef NDEBUG
  ScopedPngStruct invalid(static_cast<ScopedPngStruct::Type>(-1),
                          &message_handler_);
  ASSERT_FALSE(invalid.valid());
  ASSERT_EQ(static_cast<png_structp>(NULL), invalid.png_ptr());
  ASSERT_EQ(static_cast<png_infop>(NULL), invalid.info_ptr());
#else
  ASSERT_DEATH(ScopedPngStruct t =
               ScopedPngStruct(static_cast<ScopedPngStruct::Type>(-1),
                               &message_handler_),
               "Check failed: type==READ || type==WRITE");
#endif
}

TEST(PngReaderTest, ReadTransparentPng) {
  MockMessageHandler message_handler(new NullMutex);
  PngReader reader(&message_handler);
  ScopedPngStruct read(ScopedPngStruct::READ, &message_handler);
  GoogleString in;
  ReadTestFile(kPngSuiteTestDir, "basn4a16", "png", &in);
  // Don't require_opaque.
  ASSERT_TRUE(reader.ReadPng(in, read.png_ptr(), read.info_ptr(),
                             PNG_TRANSFORM_IDENTITY, false));
  ASSERT_FALSE(reader.IsAlphaChannelOpaque(read.png_ptr(), read.info_ptr(),
                                           &message_handler));
  ASSERT_TRUE(read.reset());

  // Don't transform but require opaque.
  ASSERT_FALSE(reader.ReadPng(in, read.png_ptr(), read.info_ptr(),
                              PNG_TRANSFORM_IDENTITY, true));
  ASSERT_TRUE(read.reset());

  // Strip the alpha channel and require opaque.
  ASSERT_TRUE(reader.ReadPng(in, read.png_ptr(), read.info_ptr(),
                             PNG_TRANSFORM_STRIP_ALPHA, true));
#ifndef NDEBUG
  ASSERT_DEATH(reader.IsAlphaChannelOpaque(read.png_ptr(), read.info_ptr(),
                                           &message_handler),
               "IsAlphaChannelOpaque called for image without alpha channel.");
#else
  ASSERT_FALSE(reader.IsAlphaChannelOpaque(read.png_ptr(), read.info_ptr(),
                                           &message_handler));
#endif
  ASSERT_TRUE(read.reset());

  // Strip the alpha channel and don't require opaque.
  ASSERT_TRUE(reader.ReadPng(in, read.png_ptr(), read.info_ptr(),
                             PNG_TRANSFORM_STRIP_ALPHA, false));
#ifndef NDEBUG
  ASSERT_DEATH(reader.IsAlphaChannelOpaque(read.png_ptr(), read.info_ptr(),
                                           &message_handler),
               "IsAlphaChannelOpaque called for image without alpha channel.");
#else
  ASSERT_FALSE(reader.IsAlphaChannelOpaque(read.png_ptr(), read.info_ptr(),
                                           &message_handler));
#endif
  ASSERT_TRUE(read.reset());
}

TEST_F(PngScanlineReaderRawTest, ValidPngsRow) {
  // Create a reader which tries to read a row of image at a time.
  PngScanlineReaderRaw per_row_reader(&message_handler_);

  // Create a reader which reads the entire image.
  PngReader png_reader(&message_handler_);
  PngScanlineReader entire_image_reader(&message_handler_);
  if (setjmp(*entire_image_reader.GetJmpBuf()) != 0) {
    FAIL();
  }

  for (size_t i = 0; i < kValidImageCount; i++) {
    GoogleString image_string;
    ReadTestFile(kPngSuiteTestDir, kValidImages[i].filename, "png",
                 &image_string);

    // Initialize entire_image_reader (PngScanlineReader).
    if (!InitializeEntireReader(image_string, png_reader,
                                &entire_image_reader)) {
      // This image is not supported by PngScanlineReader. Skip it.
      continue;
    }

    // Initialize per_row_reader.
    ASSERT_TRUE(per_row_reader.Initialize(image_string.data(),
                                          image_string.length()));

    // Make sure the images sizes and the pixel formats are the same.
    ASSERT_EQ(entire_image_reader.GetImageWidth(),
              per_row_reader.GetImageWidth());
    ASSERT_EQ(entire_image_reader.GetImageHeight(),
              per_row_reader.GetImageHeight());
    ASSERT_EQ(entire_image_reader.GetPixelFormat(),
              per_row_reader.GetPixelFormat());

    const int width = per_row_reader.GetImageWidth();
    const int num_channels =
      GetNumChannelsFromPixelFormat(per_row_reader.GetPixelFormat(),
                                    &message_handler_);
    uint8* buffer_per_row = NULL;
    uint8* buffer_entire = NULL;

    // Decode and check the image a row at a time.
    while (per_row_reader.HasMoreScanLines() &&
           entire_image_reader.HasMoreScanLines()) {
      ASSERT_TRUE(entire_image_reader.ReadNextScanline(
        reinterpret_cast<void**>(&buffer_entire)));

      ASSERT_TRUE(per_row_reader.ReadNextScanline(
        reinterpret_cast<void**>(&buffer_per_row)));

      for (int i = 0; i < width*num_channels; ++i) {
        ASSERT_EQ(buffer_entire[i], buffer_per_row[i]);
      }
    }

    // Make sure both readers have exhausted all image rows.
    ASSERT_FALSE(per_row_reader.HasMoreScanLines());
    ASSERT_FALSE(entire_image_reader.HasMoreScanLines());
  }
}

TEST_F(PngScanlineReaderRawTest, ValidPngsEntire) {
  PngReader png_reader(&message_handler_);
  PngScanlineReader entire_image_reader(&message_handler_);
  if (setjmp(*entire_image_reader.GetJmpBuf()) != 0) {
    FAIL();
  }

  for (size_t i = 0; i < kValidImageCount; ++i) {
    GoogleString image_string;
    ReadTestFile(kPngSuiteTestDir, kValidImages[i].filename, "png",
                 &image_string);

    // Initialize entire_image_reader (PngScanlineReader).
    if (!InitializeEntireReader(image_string, png_reader,
                                &entire_image_reader)) {
      // This image is not supported by PngScanlineReader. Skip it.
      continue;
    }

    size_t width, bytes_per_row;
    void* buffer_for_raw_reader;
    pagespeed::image_compression::PixelFormat pixel_format;
    ASSERT_TRUE(ReadImage(pagespeed::image_compression::IMAGE_PNG,
                          image_string.data(), image_string.length(),
                          &buffer_for_raw_reader, &pixel_format, &width, NULL,
                          &bytes_per_row, &message_handler_));
    uint8* buffer_per_row = static_cast<uint8*>(buffer_for_raw_reader);
    int num_channels = GetNumChannelsFromPixelFormat(pixel_format,
                                                     &message_handler_);

    // Check the image row by row.
    while (entire_image_reader.HasMoreScanLines()) {
      uint8* buffer_entire = NULL;
      ASSERT_TRUE(entire_image_reader.ReadNextScanline(
        reinterpret_cast<void**>(&buffer_entire)));

      for (size_t i = 0; i < width*num_channels; ++i) {
        ASSERT_EQ(buffer_entire[i], buffer_per_row[i]);
      }
      buffer_per_row += bytes_per_row;
    }

    free(buffer_for_raw_reader);
  }
}

TEST_F(PngScanlineReaderRawTest, PartialRead) {
  uint8* buffer = NULL;
  GoogleString image_string;
  ReadTestFile(kPngSuiteTestDir, kValidImages[0].filename, "png",
               &image_string);

  // Initialize a reader but do not read any scanline.
  PngScanlineReaderRaw reader1(&message_handler_);
  ASSERT_TRUE(reader1.Initialize(image_string.data(), image_string.length()));

  // Initialize a reader and read one scanline.
  PngScanlineReaderRaw reader2(&message_handler_);
  ASSERT_TRUE(reader2.Initialize(image_string.data(), image_string.length()));
  ASSERT_TRUE(reader2.ReadNextScanline(reinterpret_cast<void**>(&buffer)));

  // Initialize a reader, and try to read a scanline after the image has been
  // depleted.
  PngScanlineReaderRaw reader3(&message_handler_);
  ASSERT_TRUE(reader3.Initialize(image_string.data(), image_string.length()));
  while (reader3.HasMoreScanLines()) {
    ASSERT_TRUE(reader3.ReadNextScanline(reinterpret_cast<void**>(&buffer)));
  }
  ASSERT_FALSE(reader3.ReadNextScanline(reinterpret_cast<void**>(&buffer)));
}

TEST_F(PngScanlineReaderRawTest, ReadAfterReset) {
  uint8* buffer = NULL;
  GoogleString image_string;
  ReadTestFile(kPngSuiteTestDir, kValidImages[0].filename, "png",
               &image_string);

  // Initialize a reader and read one scanline.
  PngScanlineReaderRaw reader(&message_handler_);
  ASSERT_TRUE(reader.Initialize(image_string.data(), image_string.length()));
  ASSERT_TRUE(reader.ReadNextScanline(reinterpret_cast<void**>(&buffer)));
  // Now re-initialize the reader.
  ASSERT_TRUE(reader.Initialize(image_string.data(), image_string.length()));

  // Decode the entire image using ReadImage(). This copy of image will be used
  // as the baseline.
  size_t width, bytes_per_row;
  void* buffer_for_raw_reader;
  pagespeed::image_compression::PixelFormat pixel_format;
  ASSERT_TRUE(ReadImage(pagespeed::image_compression::IMAGE_PNG,
                        image_string.data(), image_string.length(),
                        &buffer_for_raw_reader, &pixel_format, &width, NULL,
                        &bytes_per_row, &message_handler_));
  uint8* buffer_entire = static_cast<uint8*>(buffer_for_raw_reader);
  int num_channels = GetNumChannelsFromPixelFormat(pixel_format,
                                                   &message_handler_);

  // Compare the image row by row.
  while (reader.HasMoreScanLines()) {
    uint8* buffer_row = NULL;
    ASSERT_TRUE(reader.ReadNextScanline(
      reinterpret_cast<void**>(&buffer_row)));

    for (size_t i = 0; i < width*num_channels; ++i) {
      ASSERT_EQ(buffer_entire[i], buffer_row[i]);
    }
    buffer_entire += bytes_per_row;
  }

  free(buffer_for_raw_reader);
}

TEST_F(PngScanlineReaderRawTest, InvalidPngs) {
  message_handler_.AddPatternToSkipPrinting(kMessagePatternLibpngError);
  message_handler_.AddPatternToSkipPrinting(kMessagePatternLibpngWarning);
  message_handler_.AddPatternToSkipPrinting(kMessagePatternUnexpectedEOF);
  PngScanlineReaderRaw reader(&message_handler_);
  for (size_t i = 0; i < kInvalidFileCount; i++) {
    GoogleString image_string;
    ReadTestFile(kPngSuiteTestDir, kInvalidFiles[i], "png", &image_string);

    ASSERT_FALSE(reader.Initialize(image_string.data(), image_string.length()));

    ASSERT_FALSE(ReadImage(pagespeed::image_compression::IMAGE_PNG,
                           image_string.data(), image_string.length(),
                           NULL, NULL, NULL, NULL, NULL, &message_handler_));
  }
}

// Make sure that PNG files are written correctly. We firstly decompress
// a PNG image; then compress it to a new PNG image; finally verify that
// the new PNG is the same as the original one. Information to be verified
// include pixel values, pixel type, and image size.
//
// Libpng provides several options for writing a PNG. To verify that
// PngScanlineWriter works on all of these options, the test uses a rotation
// of configurations and verifies that all of the rewritten images are good.
TEST_F(PngScanlineWriterTest, RewritePng) {
  message_handler_.AddPatternToSkipPrinting(kMessagePatternUnrecognizedColor);
  PngScanlineReaderRaw original_reader(&message_handler_);
  PngScanlineReaderRaw rewritten_reader(&message_handler_);

  // List of filters supported by libpng.
  const int png_filter_list[] = {
    PNG_FILTER_NONE,  // 0x08
    PNG_FILTER_SUB,   // 0x10
    PNG_FILTER_UP,    // 0x20
    PNG_FILTER_AVG,   // 0x40
    PNG_FILTER_PAETH  // 0x80
  };

  for (size_t i = 0; i < kValidImageCount; i++) {
    GoogleString original_image;
    GoogleString rewritten_image;

    ReadTestFile(kPngSuiteTestDir, kValidImages[i].filename, "png",
                 &original_image);

    // Initialize a PNG reader for reading the original image.
    if (original_reader.Initialize(original_image.data(),
                                   original_image.length()) == false) {
      // Some images in kValidImages[] have unsupported formats, for example,
      // GRAY_ALPHA. These images are skipped.
      continue;
    }

    // Get the sizes and pixel format of the original image.
    const size_t width = original_reader.GetImageWidth();
    const size_t height = original_reader.GetImageHeight();
    const PixelFormat pixel_format = original_reader.GetPixelFormat();

    // Use a new combination of filter and compression level for writing
    // this image.
    const int num_z = Z_FIXED - Z_DEFAULT_STRATEGY + 1;
    int compression_strategy = Z_DEFAULT_STRATEGY + (i % num_z);
    int filter_level = png_filter_list[(i / num_z) % 5];
    PngCompressParams params(filter_level, compression_strategy);

    // Initialize the writer.
    writer_.reset(CreateScanlineWriter(
        pagespeed::image_compression::IMAGE_PNG, pixel_format, width,
        height, &params, &rewritten_image, &message_handler_));
    ASSERT_NE(static_cast<ScanlineWriterInterface *>(NULL), writer_.get());

    // Read the scanlines from the original image and write them to the new one.
    while (original_reader.HasMoreScanLines()) {
      uint8* scanline = NULL;
      ASSERT_TRUE(original_reader.ReadNextScanline(
          reinterpret_cast<void**>(&scanline)));
      ASSERT_TRUE(writer_->WriteNextScanline(
          reinterpret_cast<void*>(scanline)));
    }

    // Make sure that the readers has exhausted the original image.
    ASSERT_FALSE(original_reader.HasMoreScanLines());
    // Make sure that the writer has received all of the image data, and
    // finalize it.
    ASSERT_TRUE(writer_->FinalizeWrite());

    // Now create readers for reading the original and the rewritten images.
    ASSERT_TRUE(original_reader.Initialize(original_image.data(),
                                           original_image.length()));
    ASSERT_TRUE(rewritten_reader.Initialize(rewritten_image.data(),
                                            rewritten_image.length()));

    // Now make sure that the original and rewritten images have the
    // same sizes, types, and pixel values.
    AssertReadersMatch(&original_reader, &rewritten_reader);
  }
}

// Attempt to finalize without writing all of the scanlines.
TEST_F(PngScanlineWriterTest, EarlyFinalize) {
  ASSERT_TRUE(Initialize());
  ASSERT_TRUE(writer_->WriteNextScanline(reinterpret_cast<void*>(scanline_)));
  ASSERT_FALSE(writer_->FinalizeWrite());
}

// Write insufficient number of scanlines and do not finalize at the end.
TEST_F(PngScanlineWriterTest, MissingScanlines) {
  ASSERT_TRUE(Initialize());
  ASSERT_TRUE(writer_->WriteNextScanline(reinterpret_cast<void*>(scanline_)));
}

// Write too many scanlines.
TEST_F(PngScanlineWriterTest, TooManyScanlines) {
  ASSERT_TRUE(Initialize());
  ASSERT_TRUE(writer_->WriteNextScanline(reinterpret_cast<void*>(scanline_)));
  ASSERT_TRUE(writer_->WriteNextScanline(reinterpret_cast<void*>(scanline_)));
  ASSERT_FALSE(writer_->WriteNextScanline(reinterpret_cast<void*>(scanline_)));
}

// Write a scanline, and then re-initialize and write too many scanlines.
TEST_F(PngScanlineWriterTest, ReinitializeAndTooManyScanlines) {
  ASSERT_TRUE(Initialize());
  ASSERT_TRUE(writer_->WriteNextScanline(reinterpret_cast<void*>(scanline_)));

  ASSERT_TRUE(Initialize());
  ASSERT_TRUE(writer_->WriteNextScanline(reinterpret_cast<void*>(scanline_)));
  ASSERT_TRUE(writer_->WriteNextScanline(reinterpret_cast<void*>(scanline_)));
  ASSERT_FALSE(writer_->WriteNextScanline(reinterpret_cast<void*>(scanline_)));
}

TEST_F(PngScanlineWriterTest, DecodeGrayAlpha) {
  GoogleString rgba_image, ga_image;
  ASSERT_TRUE(ReadTestFile(kPngTestDir, kImageRGBA, "png", &rgba_image));
  ASSERT_TRUE(ReadTestFile(kPngTestDir, kImageGA, "png", &ga_image));
  DecodeAndCompareImages(IMAGE_PNG, rgba_image.c_str(), rgba_image.length(),
                         IMAGE_PNG, ga_image.c_str(), ga_image.length(),
                         &message_handler_);
}

}  // namespace
