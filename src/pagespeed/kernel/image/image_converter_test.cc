/*
 * Copyright 2011 Google Inc.
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

// Author: Satyanarayana Manyam

#include "base/logging.h"

#include "base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/gif_reader.h"
#include "pagespeed/kernel/image/image_converter.h"
#include "pagespeed/kernel/image/png_optimizer.h"
#include "pagespeed/kernel/image/test_utils.h"

namespace {

using pagespeed::image_compression::kGifTestDir;
using pagespeed::image_compression::kPngSuiteTestDir;
using pagespeed::image_compression::kPngSuiteGifTestDir;
using pagespeed::image_compression::kPngTestDir;
using pagespeed::image_compression::GifReader;
using pagespeed::image_compression::ImageConverter;
using pagespeed::image_compression::JpegLossyOptions;
using pagespeed::image_compression::PngOptimizer;
using pagespeed::image_compression::PngReader;
using pagespeed::image_compression::PngReaderInterface;
using pagespeed::image_compression::WebpConfiguration;
using pagespeed::image_compression::ReadTestFile;

struct ImageCompressionInfo {
  const char* filename;
  size_t original_size;
  size_t compressed_size;
  bool is_png;
};

// These images were obtained from
// http://www.libpng.org/pub/png/pngsuite.html
ImageCompressionInfo kValidImages[] = {
  { "basi0g01", 217, 208, 1},
  { "basi0g02", 154, 154, 1},
  { "basi0g04", 247, 145, 1},
  { "basi0g08", 254, 250, 1},
  { "basi0g16", 299, 285, 1},
  { "basi2c08", 315, 313, 1},
  { "basi2c16", 595, 419, 0},
  { "basi3p01", 132, 132, 1},
  { "basi3p02", 193, 178, 1},
  { "basi3p04", 327, 312, 1},
  { "basi4a08", 214, 209, 1},
  { "basi4a16", 2855, 1980, 1},
  { "basi6a08", 361, 350, 1},
  { "basi6a16", 4180, 4133, 1},
  { "basn0g01", 164, 164, 1},
  { "basn0g02", 104, 104, 1},
  { "basn0g04", 145, 103, 1},
  { "basn0g08", 138, 132, 1},
  { "basn0g16", 167, 152, 1},
  { "basn2c08", 145, 145, 1},
  { "basn2c16", 302, 274, 1},
  { "basn3p01", 112, 112, 1},
  { "basn3p02", 146, 131, 1},
  { "basn3p04", 216, 201, 1},
  { "basn4a08", 126, 121, 1},
  { "basn4a16", 2206, 1185, 1},
  { "basn6a08", 184, 176, 1},
  { "basn6a16", 3435, 3271, 1},
  { "bgai4a08", 214, 209, 1},
  { "bgai4a16", 2855, 1980, 1},
  { "bgan6a08", 184, 176, 1},
  { "bgan6a16", 3435, 3271, 1},
  { "bgbn4a08", 140, 121, 1},
  { "bggn4a16", 2220, 1185, 1},
  { "bgwn6a08", 202, 176, 1},
  { "bgyn6a16", 3453, 3271, 1},
  { "cdfn2c08", 404, 498, 1},
  { "cdhn2c08", 344, 476, 1},
  { "cdsn2c08", 232, 255, 1},
  { "cdun2c08", 724, 928, 1},
  { "ch1n3p04", 258, 201, 1},
  { "cm0n0g04", 292, 271, 1},
  { "cm7n0g04", 292, 271, 1},
  { "cm9n0g04", 292, 271, 1},
  { "cs3n2c16", 214, 178, 1},
  { "cs3n3p08", 259, 244, 1},
  { "cs5n2c08", 186, 226, 1},
  { "cs5n3p08", 271, 256, 1},
  { "cs8n2c08", 149, 226, 1},
  { "cs8n3p08", 256, 256, 1},
  { "ct0n0g04", 273, 271, 1},
  { "ct1n0g04", 792, 271, 1},
  { "ctzn0g04", 753, 271, 1},
  { "f00n0g08", 319, 312, 1},
  { "f01n0g08", 321, 246, 1},
  { "f02n0g08", 355, 289, 1},
  { "f03n0g08", 389, 292, 1},
  { "f04n0g08", 269, 273, 1},
  { "g03n0g16", 345, 273, 1},
  { "g03n2c08", 370, 396, 1},
  { "g03n3p04", 214, 214, 1},
  { "g04n0g16", 363, 287, 1},
  { "g04n2c08", 377, 399, 1},
  { "g04n3p04", 219, 219, 1},
  { "g05n0g16", 339, 275, 1},
  { "g05n2c08", 350, 402, 1},
  { "g05n3p04", 206, 206, 1},
  { "g07n0g16", 321, 261, 1},
  { "g07n2c08", 340, 401, 1},
  { "g07n3p04", 207, 207, 1},
  { "g10n0g16", 262, 210, 1},
  { "g10n2c08", 285, 403, 1},
  { "g10n3p04", 214, 214, 1},
  { "g25n0g16", 383, 305, 1},
  { "g25n2c08", 405, 399, 1},
  { "g25n3p04", 215, 215, 1},
  { "oi1n0g16", 167, 152, 1},
  { "oi1n2c16", 302, 274, 1},
  { "oi2n0g16", 179, 152, 1},
  { "oi2n2c16", 314, 274, 1},
  { "oi4n0g16", 203, 152, 1},
  { "oi4n2c16", 338, 274, 1},
  { "oi9n0g16", 1283, 152, 1},
  { "oi9n2c16", 3038, 274, 1},
  { "pp0n2c16", 962, 419, 0},
  { "pp0n6a08", 818, 818, 1},
  { "ps1n0g08", 1477, 132, 1},
  { "ps1n2c16", 1641, 274, 1},
  { "ps2n0g08", 2341, 132, 1},
  { "ps2n2c16", 2505, 274, 1},
  { "s01i3p01", 113, 98, 1},
  { "s01n3p01", 113, 98, 1},
  { "s02i3p01", 114, 99, 1},
  { "s02n3p01", 115, 100, 1},
  { "s03i3p01", 118, 103, 1},
  { "s03n3p01", 120, 105, 1},
  { "s04i3p01", 126, 111, 1},
  { "s04n3p01", 121, 106, 1},
  { "s05i3p02", 134, 119, 1},
  { "s05n3p02", 129, 114, 1},
  { "s06i3p02", 143, 128, 1},
  { "s06n3p02", 131, 116, 1},
  { "s07i3p02", 149, 134, 1},
  { "s07n3p02", 138, 123, 1},
  { "s08i3p02", 149, 134, 1},
  { "s08n3p02", 139, 124, 1},
  { "s09i3p02", 147, 132, 1},
  { "s09n3p02", 143, 128, 1},
  { "s32i3p04", 355, 340, 1},
  { "s32n3p04", 263, 248, 1},
  { "s33i3p04", 385, 370, 1},
  { "s33n3p04", 329, 314, 1},
  { "s34i3p04", 349, 332, 1},
  { "s34n3p04", 248, 229, 1},
  { "s35i3p04", 399, 384, 1},
  { "s35n3p04", 338, 313, 1},
  { "s36i3p04", 356, 339, 1},
  { "s36n3p04", 258, 240, 1},
  { "s37i3p04", 393, 378, 1},
  { "s37n3p04", 336, 317, 1},
  { "s38i3p04", 357, 339, 1},
  { "s38n3p04", 245, 228, 1},
  { "s39i3p04", 420, 405, 1},
  { "s39n3p04", 352, 336, 1},
  { "s40i3p04", 357, 340, 1},
  { "s40n3p04", 256, 237, 1},
  { "tbbn1g04", 419, 405, 1},
  { "tbbn2c16", 1994, 1095, 1},
  { "tbbn3p08", 1128, 1095, 1},
  { "tbgn2c16", 1994, 1095, 1},
  { "tbgn3p08", 1128, 1095, 1},
  { "tbrn2c08", 1347, 1095, 1},
  { "tbwn1g16", 1146, 582, 1},
  { "tbwn3p08", 1131, 1095, 1},
  { "tbyn3p08", 1131, 1095, 1},
  { "tp0n1g08", 689, 568, 1},
  { "tp1n3p08", 1115, 1095, 1},
  { "z00n2c08", 3172, 224, 1},
  { "z03n2c08", 232, 224, 1},
  { "z06n2c08", 224, 224, 1},
  { "z09n2c08", 224, 224, 1},
  { "basi3p08", 1527, 565, 0},
  { "basn3p08", 1286, 565, 0},
  { "ccwn2c08", 1514, 764, 0},
  { "ccwn3p08", 1554, 779, 0},
  { "ch2n3p08", 1810, 565, 0},
  { "f00n2c08", 2475, 706, 0},
  { "f01n2c08", 1180, 657, 0},
  { "f02n2c08", 1729, 696, 0},
  { "f03n2c08", 1291, 697, 0},
  { "f04n2c08", 985, 672, 0},
  { "tp0n2c08", 1311, 875, 0},
  { "tp0n3p08", 1120, 875, 0},
};

const char* kInvalidFiles[] = {
  "nosuchfile",
  "emptyfile",
  "x00n0g01",
  "xcrn0g04",
  "xlfn0g04",
};

struct GifImageCompressionInfo {
  const char* filename;
  size_t original_size;
  size_t png_size;
  size_t jpeg_size;
  size_t webp_size;
};

GifImageCompressionInfo kValidGifImages[] = {
  { "basi0g01", 153,  166,  1036, 120},
  { "basi0g02", 185,  112,  664,  74},
  { "basi0g04", 344,  144,  439,  104},
  { "basi0g08", 1736, 116,  468,  582},
  { "basn0g01", 153,  166,  1036, 120},
  { "basn0g02", 185,  112,  664,  74},
  { "basn0g04", 344,  144,  439,  104},
  { "basn0g08", 1736, 116,  468,  582},
  { "basi3p01", 138,   96,  789,  56},
  { "basi3p02", 186,  115,  1157, 74},
  { "basi3p04", 344,  185,  992,  136},
  { "basi3p08", 1737, 1270, 929,  810},
  { "basn3p01", 138,  96,   789,  56},
  { "basn3p02", 186,  115,  1157, 74},
  { "basn3p04", 344,  185,  992,  136},
  { "basn3p08", 1737, 1270, 929,  810}
};

const size_t kValidImageCount = arraysize(kValidImages);
const size_t kValidGifImageCount = arraysize(kValidGifImages);
const size_t kInvalidFileCount = arraysize(kInvalidFiles);

TEST(ImageConverterTest, OptimizePngOrConvertToJpeg_invalidPngs) {
  scoped_ptr<PngReaderInterface> png_struct_reader(new PngReader);
  pagespeed::image_compression::JpegCompressionOptions options;
  for (size_t i = 0; i < kInvalidFileCount; i++) {
    GoogleString in, out;
    bool is_out_png;
    ReadTestFile(kPngSuiteTestDir, kInvalidFiles[i], "png", &in);
    ASSERT_FALSE(ImageConverter::OptimizePngOrConvertToJpeg(
        *(png_struct_reader.get()), in, options, &out, &is_out_png));
  }
}

TEST(ImageConverterTest, OptimizePngOrConvertToJpeg) {
  PngReader png_struct_reader;
  pagespeed::image_compression::JpegCompressionOptions options;
  // We are using default lossy options for conversion.
  options.lossy = true;
  options.progressive = false;
  for (size_t i = 0; i < kValidImageCount; i++) {
    GoogleString in, out;
    bool is_out_png;
    ReadTestFile(kPngSuiteTestDir, kValidImages[i].filename, "png", &in);
    ASSERT_TRUE(ImageConverter::OptimizePngOrConvertToJpeg(
        png_struct_reader, in, options, &out, &is_out_png));

    // Verify that the size matches.
    EXPECT_EQ(kValidImages[i].compressed_size, out.size())
        << "size mismatch for " << kValidImages[i].filename;
    // Verify that out put image type matches.

    EXPECT_EQ(kValidImages[i].is_png, is_out_png)
        << "image type mismatch for " << kValidImages[i].filename;
  }
}

TEST(ImageConverterTest, ConvertPngToWebp_invalidPngs) {
  PngReader png_struct_reader;
  WebpConfiguration webp_config;

  for (size_t i = 0; i < kInvalidFileCount; i++) {
    GoogleString in, out;
    ReadTestFile(kPngSuiteTestDir, kInvalidFiles[i], "png", &in);
    bool is_opaque = false;
    ASSERT_FALSE(ImageConverter::ConvertPngToWebp(
        png_struct_reader, in, webp_config, &out, &is_opaque));
  }
}

TEST(ImageConverterTest, ConvertOpaqueGifToPng) {
  GifReader png_struct_reader;
  for (size_t i = 0; i < kValidGifImageCount; i++) {
    GoogleString in, out;
     ReadTestFile(
        kPngSuiteGifTestDir, kValidGifImages[i].filename, "gif", &in);
    EXPECT_EQ(kValidGifImages[i].original_size, in.size())
        << "input size mismatch for " << kValidGifImages[i].filename;
    ASSERT_TRUE(PngOptimizer::OptimizePngBestCompression(
        png_struct_reader, in, &out));
    // Verify that the size matches.
    EXPECT_EQ(kValidGifImages[i].png_size, out.size())
        << "output size mismatch for " << kValidGifImages[i].filename;
  }
}

TEST(ImageConverterTest, ConvertOpaqueGifToJpeg) {
  GifReader png_struct_reader;
  pagespeed::image_compression::JpegCompressionOptions options;
  options.lossy = true;
  options.progressive = false;
  options.lossy_options.quality = 100;
  for (size_t i = 0; i < kValidGifImageCount; i++) {
    GoogleString in, out;
     ReadTestFile(
        kPngSuiteGifTestDir, kValidGifImages[i].filename, "gif", &in);
    EXPECT_EQ(kValidGifImages[i].original_size, in.size())
        << "input size mismatch for " << kValidGifImages[i].filename;
    ASSERT_TRUE(ImageConverter::ConvertPngToJpeg(
        png_struct_reader, in, options, &out));
    // Verify that the size matches.
    EXPECT_EQ(kValidGifImages[i].jpeg_size, out.size())
        << "output size mismatch for " << kValidGifImages[i].filename;
  }
}

TEST(ImageConverterTest, ConvertOpaqueGifToWebp) {
  GifReader png_struct_reader;
  pagespeed::image_compression::WebpConfiguration options;
  for (size_t i = 0; i < kValidGifImageCount; i++) {
    GoogleString in, out;
     ReadTestFile(
        kPngSuiteGifTestDir, kValidGifImages[i].filename, "gif", &in);
    EXPECT_EQ(kValidGifImages[i].original_size, in.size())
        << "input size mismatch for " << kValidGifImages[i].filename;
    bool is_opaque = false;
    ASSERT_TRUE(ImageConverter::ConvertPngToWebp(
        png_struct_reader, in, options, &out, &is_opaque));
    // Verify that the size matches.
    // TODO(vchudnov): Have a more thorough comparison.
    EXPECT_LT(kValidGifImages[i].webp_size, in.size())
        << "webp size is not smaller for " << kValidGifImages[i].filename;
    EXPECT_TRUE(is_opaque) << kValidGifImages[i].filename;
  }
}

TEST(ImageConverterTest, ConvertTransparentGifToPng) {
  GifReader png_struct_reader;
  GoogleString in, out;
  ReadTestFile(kGifTestDir, "transparent", "gif", &in);
  EXPECT_EQ(static_cast<size_t>(55800), in.size())
      << "input size mismatch";
  ASSERT_TRUE(PngOptimizer::OptimizePngBestCompression(
      png_struct_reader, in, &out));
  // Verify that the size matches.
  EXPECT_EQ(static_cast<size_t>(25020), out.size())
      << "output size mismatch";
}

TEST(ImageConverterTest, ConvertTransparentGifToWebp) {
  GifReader png_struct_reader;
  pagespeed::image_compression::WebpConfiguration options;
  GoogleString in, out;
  ReadTestFile(kGifTestDir, "transparent", "gif", &in);
  EXPECT_EQ(static_cast<size_t>(55800), in.size())
      << "input size mismatch";
  bool is_opaque = false;
  ASSERT_TRUE(ImageConverter::ConvertPngToWebp(
      png_struct_reader, in, options, &out, &is_opaque));

  // Verify that the size matches.
  // TODO(vchudnov): Have a more thorough comparison.
  EXPECT_LT(out.size(), in.size())
      << "webpsize is not smaller";

  EXPECT_FALSE(is_opaque);
}

TEST(ImageConverterTest, NotConvertTransparentGifToJpeg) {
  GifReader png_struct_reader;
  pagespeed::image_compression::JpegCompressionOptions options;
  options.lossy = true;
  options.progressive = false;
  options.lossy_options.quality = 100;
  GoogleString in, out;
  ReadTestFile(kGifTestDir, "transparent", "gif", &in);
  EXPECT_EQ(static_cast<size_t>(55800), in.size())
      << "input size mismatch";
  ASSERT_FALSE(ImageConverter::ConvertPngToJpeg(
      png_struct_reader, in, options, &out));
  // Verify that the size matches.
  EXPECT_EQ(static_cast<size_t>(0), out.size())
      << "output size mismatch";
}

// To manually inspect all gif conversions tested, uncomment the lines
// indicated in the *Convert*GifTo* test cases above, run this
// test, and then generate an html page as follows:
//
/*
 (echo '<table border="1" style="background-color: gray;">'
  echo '<tr><th>name</th><th>gif</th><th>png</th>'
  echo '<th>jpeg</th><th>webp</th></tr>'
  ls /tmp/image_converter_test/gif-*gif | sed -s 's/\.gif//' | \
  xargs --replace=X \
  echo '<tr><td>X</td><td><img src="X.gif"></td><td><img src="X.png"></td>' \
  '<td><img src="X.jpg"></td><td><img src="X.webp"></td></tr>'
  echo '</table>') > /tmp/allimages.html
*/


// TODO(vchudnov): add webp tests to do pixel-for-pixel comparisons
// and to test GetSmallestOfPngJpegWebp

}  // namespace
