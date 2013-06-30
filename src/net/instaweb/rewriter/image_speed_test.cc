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

// Author: huibao@google.com (Huibao Lin)
//
// CPU: Intel Nehalem with HyperThreading (4 cores) dL1:32KB dL2:256KB
// Benchmark              Time(ns)    CPU(ns) Iterations
// -----------------------------------------------------
// BM_ConvertJpegToJpeg   15885596   15900000        100
// BM_ConvertJpegToWebp  117559345  117700000        100
// BM_ConvertPngToPng      2641828    2625483        259
// BM_ConvertPngToWebp     1850260    1847826        368
// BM_ConvertGifToPng     46872103   46900000        100
// BM_ConvertGifToWebp    88707819   88800000        100
// BM_ConvertWebpToWebp   43523271   43600000        100

#include "net/instaweb/rewriter/image_types.pb.h"
#include "net/instaweb/rewriter/public/image.h"
#include "net/instaweb/util/public/benchmark.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/null_mutex.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const char kTestData[] = "/net/instaweb/rewriter/testdata/";
const char kCuppa[] = "Cuppa.png";
const char kIronChef[] = "IronChef2.gif";
const char kPuzzle[] = "Puzzle.jpg";
const char kScenery[] = "Scenery.webp";

// The original quality of Puzzle.jpg is 97. Rewrite it to a lower
// quality.
const int kNewQuality = 80;

class TestImageRewrite {
 public:
  TestImageRewrite(const char* file_name,
                   net_instaweb::Image::CompressionOptions* options) :
      handler_(new net_instaweb::NullMutex),
      timer_(0),
      options_(options),
      file_name_(file_name) {
  }

  bool Initialize(net_instaweb::ImageType type) {
    expected_output_image_type_ = type;
    GoogleString file_path = StrCat(net_instaweb::GTestSrcDir(), kTestData,
                                    file_name_);
    return file_system_.ReadFile(file_path.c_str(), &contents_,
                                 &handler_);
  }

  void Rewrite() {
    // Reset conversions_attempted. This field is increased each time
    // the image is rewritten, and the image will not be rewritten if
    // this field is greater than the limit.
    options_->conversions_attempted = 0;

    // Rewrite the image.
    net_instaweb::Image* image =
        NewImage(contents_, file_name_, "/NOT-USED",
                 options_, &timer_, &handler_);
    image->Contents();
    EXPECT_EQ(expected_output_image_type_, image->image_type());
    EXPECT_NE(contents_.length(), image->output_size());
  }

 private:
  net_instaweb::StdioFileSystem file_system_;
  net_instaweb::MockMessageHandler handler_;
  net_instaweb::MockTimer timer_;
  net_instaweb::Image::CompressionOptions* options_;
  net_instaweb::ImageType expected_output_image_type_;
  const char* file_name_;
  GoogleString contents_;
};

static void BM_ConvertJpegToJpeg(int iters) {
  net_instaweb::Image::CompressionOptions options;
  options.recompress_jpeg = true;
  options.jpeg_quality = kNewQuality;

  TestImageRewrite test_rewrite(kPuzzle, &options);
  ASSERT_TRUE(test_rewrite.Initialize(net_instaweb::IMAGE_JPEG));
  for (int i = 0; i < iters; ++i) {
    test_rewrite.Rewrite();
  }
}
BENCHMARK(BM_ConvertJpegToJpeg);

static void BM_ConvertJpegToWebp(int iters) {
  net_instaweb::Image::CompressionOptions options;
  options.preferred_webp = net_instaweb::Image::WEBP_LOSSY;
  options.convert_jpeg_to_webp = true;
  options.webp_quality = kNewQuality;

  TestImageRewrite test_rewrite(kPuzzle, &options);
  ASSERT_TRUE(test_rewrite.Initialize(net_instaweb::IMAGE_WEBP));
  for (int i = 0; i < iters; ++i) {
    test_rewrite.Rewrite();
  }
}
BENCHMARK(BM_ConvertJpegToWebp);

static void BM_ConvertPngToPng(int iters) {
  net_instaweb::Image::CompressionOptions options;
  options.recompress_png = true;

  TestImageRewrite test_rewrite(kCuppa, &options);
  ASSERT_TRUE(test_rewrite.Initialize(net_instaweb::IMAGE_PNG));
  for (int i = 0; i < iters; ++i) {
    test_rewrite.Rewrite();
  }
}
BENCHMARK(BM_ConvertPngToPng);

static void BM_ConvertPngToWebp(int iters) {
  net_instaweb::Image::CompressionOptions options;
  options.preferred_webp = net_instaweb::Image::WEBP_LOSSLESS;
  options.allow_webp_alpha = true;
  options.preserve_lossless = true;

  TestImageRewrite test_rewrite(kCuppa, &options);
  ASSERT_TRUE(test_rewrite.Initialize(
      net_instaweb::IMAGE_WEBP_LOSSLESS_OR_ALPHA));
  for (int i = 0; i < iters; ++i) {
    test_rewrite.Rewrite();
  }
}
BENCHMARK(BM_ConvertPngToWebp);

static void BM_ConvertGifToPng(int iters) {
  net_instaweb::Image::CompressionOptions options;
  options.convert_gif_to_png = true;

  TestImageRewrite test_rewrite(kIronChef, &options);
  ASSERT_TRUE(test_rewrite.Initialize(net_instaweb::IMAGE_PNG));
  for (int i = 0; i < iters; ++i) {
    test_rewrite.Rewrite();
  }
}
BENCHMARK(BM_ConvertGifToPng);

// To convert a GIF image to WebP we actually convert the GIF image to PNG,
// and then from PNG to WebpP.
static void BM_ConvertGifToWebp(int iters) {
  net_instaweb::Image::CompressionOptions options;
  options.preferred_webp = net_instaweb::Image::WEBP_LOSSLESS;
  options.allow_webp_alpha = true;
  options.preserve_lossless = true;
  options.convert_gif_to_png = true;

  TestImageRewrite test_rewrite(kIronChef, &options);
  ASSERT_TRUE(test_rewrite.Initialize(
      net_instaweb::IMAGE_WEBP_LOSSLESS_OR_ALPHA));
  for (int i = 0; i < iters; ++i) {
    test_rewrite.Rewrite();
  }
}
BENCHMARK(BM_ConvertGifToWebp);

static void BM_ConvertWebpToWebp(int iters) {
  net_instaweb::Image::CompressionOptions options;
  options.preferred_webp = net_instaweb::Image::WEBP_LOSSLESS;
  options.recompress_webp = true;
  options.webp_quality = kNewQuality;

  TestImageRewrite test_rewrite(kScenery, &options);
  ASSERT_TRUE(test_rewrite.Initialize(net_instaweb::IMAGE_WEBP));
  for (int i = 0; i < iters; ++i) {
    test_rewrite.Rewrite();
  }
}
BENCHMARK(BM_ConvertWebpToWebp);

}  // namespace

}  // namespace net_instaweb
