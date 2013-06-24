/*
 * Copyright 2010 Google Inc.
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
// Author: jmaessen@google.com (Jan Maessen)

// Unit tests for Image class used in rewriting.

#include "net/instaweb/rewriter/public/image.h"

#include <algorithm>

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/image_testing_peer.h"
#include "net/instaweb/rewriter/public/image_data_lookup.h"
#include "net/instaweb/rewriter/public/image_test_base.h"
#include "net/instaweb/rewriter/public/image_url_encoder.h"
#include "net/instaweb/util/public/base64_util.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/data_url.h"
#include "net/instaweb/util/public/dynamic_annotations.h"  // RunningOnValgrind
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/image/jpeg_optimizer_test_helper.h"
#include "pagespeed/kernel/image/jpeg_utils.h"

using pagespeed::image_compression::JpegUtils;
using pagespeed_testing::image_compression::GetNumScansInJpeg;
using pagespeed_testing::image_compression::IsJpegSegmentPresent;
using pagespeed_testing::image_compression::GetColorProfileMarker;
using pagespeed_testing::image_compression::GetExifDataMarker;
using pagespeed_testing::image_compression::GetJpegNumComponentsAndSamplingFactors;

namespace {

const char kProgressiveHeader[] = "\xFF\xC2";
const int kProgressiveHeaderStartIndex = 158;
const char kMessagePatternDataTruncated[] = "*data truncated*";
const char kMessagePatternFailedToCreateWebp[] = "*Failed to create webp*";
const char kMessagePatternNoWebpDimension[] = "*Couldn't find * dimensions*";
const char kMessagePatternTimedOut[] = "*conversion timed out*";

}  // namespace

namespace net_instaweb {

class ConversionVarChecker {
 public:
  explicit ConversionVarChecker(Image::CompressionOptions* options) {
    webp_conversion_variables_.Get(
        Image::ConversionVariables::FROM_GIF)->timeout_count =
        simple_stats_.AddVariable("gif_webp_timeout");
    webp_conversion_variables_.Get(
        Image::ConversionVariables::FROM_GIF)->success_ms =
        simple_stats_.AddHistogram("gif_webp_success");
    webp_conversion_variables_.Get(
        Image::ConversionVariables::FROM_GIF)->failure_ms =
        simple_stats_.AddHistogram("gif_webp_failure");

    webp_conversion_variables_.Get(
        Image::ConversionVariables::FROM_PNG)->timeout_count =
        simple_stats_.AddVariable("png_webp_timeout");
    webp_conversion_variables_.Get(
        Image::ConversionVariables::FROM_PNG)->success_ms =
        simple_stats_.AddHistogram("png_webp_success");
    webp_conversion_variables_.Get(
        Image::ConversionVariables::FROM_PNG)->failure_ms =
        simple_stats_.AddHistogram("png_webp_failure");

    webp_conversion_variables_.Get(
        Image::ConversionVariables::FROM_JPEG)->timeout_count =
        simple_stats_.AddVariable("jpeg_webp_timeout");
    webp_conversion_variables_.Get(
        Image::ConversionVariables::FROM_JPEG)->success_ms =
        simple_stats_.AddHistogram("jpeg_webp_success");
    webp_conversion_variables_.Get(
        Image::ConversionVariables::FROM_JPEG)->failure_ms =
        simple_stats_.AddHistogram("jpeg_webp_failure");

    webp_conversion_variables_.Get(
        Image::ConversionVariables::NONOPAQUE)->timeout_count =
        simple_stats_.AddVariable("webp_alpha_timeout");
    webp_conversion_variables_.Get(
        Image::ConversionVariables::NONOPAQUE)->success_ms =
        simple_stats_.AddHistogram("webp_alpha_success");
    webp_conversion_variables_.Get(
        Image::ConversionVariables::NONOPAQUE)->failure_ms =
        simple_stats_.AddHistogram("webp_alpha_failure");

    webp_conversion_variables_.Get(
        Image::ConversionVariables::OPAQUE)->timeout_count =
        simple_stats_.AddVariable("webp_opaque_timeout");
    webp_conversion_variables_.Get(
        Image::ConversionVariables::OPAQUE)->success_ms =
        simple_stats_.AddHistogram("webp_opaque_success");
    webp_conversion_variables_.Get(
        Image::ConversionVariables::OPAQUE)->failure_ms =
        simple_stats_.AddHistogram("webp_opaque_failure");

    options->webp_conversion_variables = &webp_conversion_variables_;
  }

  void Test(int gif_webp_timeout,
            int gif_webp_success,
            int gif_webp_failure,

            int png_webp_timeout,
            int png_webp_success,
            int png_webp_failure,

            int jpeg_webp_timeout,
            int jpeg_webp_success,
            int jpeg_webp_failure,

            bool opaque) {
    EXPECT_EQ(gif_webp_timeout,
              webp_conversion_variables_.Get(
                  Image::ConversionVariables::FROM_GIF)->
              timeout_count->Get());
    EXPECT_EQ(gif_webp_success,
              webp_conversion_variables_.Get(
                  Image::ConversionVariables::FROM_GIF)->
              success_ms->Count());
    EXPECT_EQ(gif_webp_failure,
              webp_conversion_variables_.Get(
                  Image::ConversionVariables::FROM_GIF)->
              failure_ms->Count());

    EXPECT_EQ(png_webp_timeout,
              webp_conversion_variables_.Get(
                  Image::ConversionVariables::FROM_PNG)->
              timeout_count->Get());
    EXPECT_EQ(png_webp_success,
              webp_conversion_variables_.Get(
                  Image::ConversionVariables::FROM_PNG)->
              success_ms->Count());
    EXPECT_EQ(png_webp_failure,
              webp_conversion_variables_.Get(
                  Image::ConversionVariables::FROM_PNG)->
              failure_ms->Count());

    EXPECT_EQ(jpeg_webp_timeout,
              webp_conversion_variables_.Get(
                  Image::ConversionVariables::FROM_JPEG)->
              timeout_count->Get());
    EXPECT_EQ(jpeg_webp_success,
              webp_conversion_variables_.Get(
                  Image::ConversionVariables::FROM_JPEG)->
              success_ms->Count());
    EXPECT_EQ(jpeg_webp_failure,
              webp_conversion_variables_.Get(
                  Image::ConversionVariables::FROM_JPEG)->
              failure_ms->Count());

    int total_timeout =
        gif_webp_timeout +
        png_webp_timeout +
        jpeg_webp_timeout;
    int total_success =
        gif_webp_success +
        png_webp_success +
        jpeg_webp_success;
    int total_failure =
        gif_webp_failure +
        png_webp_failure +
        jpeg_webp_failure;

    Image::ConversionBySourceVariable* webp_transparency =
        webp_conversion_variables_.Get(
            (opaque ?
             Image::ConversionVariables::OPAQUE :
             Image::ConversionVariables::NONOPAQUE));

    EXPECT_EQ(total_timeout,
              webp_transparency->timeout_count->Get());
    EXPECT_EQ(total_success,
              webp_transparency->success_ms->Count());
    EXPECT_EQ(total_failure,
              webp_transparency->failure_ms->Count());
  }

 private:
  SimpleStats simple_stats_;
  Image::ConversionVariables webp_conversion_variables_;
};

class ImageTest : public ImageTestBase {
 public:
  ImageTest() : options_(new Image::CompressionOptions()) {}

 protected:
  GoogleString* GetOutputContents(Image* image) {
    return &(image->output_contents_);
  }

  void WriteToBuffer(const char* contents, GoogleString* str) {
    *str = contents;
  }

  void ExpectEmptyOutput(Image* image) {
    EXPECT_FALSE(image->output_valid_);
    EXPECT_TRUE(image->output_contents_.empty());
  }

  void ExpectContentType(ImageType image_type, Image* image) {
    EXPECT_EQ(image_type, image->image_type_);
  }

  void ExpectDimensions(ImageType image_type, int size,
                        int expected_width, int expected_height,
                        Image *image) {
    EXPECT_EQ(size, image->input_size());
    EXPECT_EQ(image_type, image->image_type());
    ImageDim image_dim;
    image_dim.Clear();
    image->Dimensions(&image_dim);
    EXPECT_TRUE(ImageUrlEncoder::HasValidDimensions(image_dim));
    EXPECT_EQ(expected_width, image_dim.width());
    EXPECT_EQ(expected_height, image_dim.height());
    EXPECT_EQ(StringPrintf("%dx%dxZZ", image_dim.width(), image_dim.height()),
              EncodeUrlAndDimensions("ZZ", image_dim));
  }

  void CheckInvalid(const GoogleString& name, const GoogleString& contents,
                    ImageType input_type, ImageType output_type,
                    bool progressive) {
    ImagePtr image(ImageFromString(output_type, name, contents, progressive));
    EXPECT_EQ(contents.size(), image->input_size());
    EXPECT_EQ(input_type, image->image_type());
    ImageDim  image_dim;
    image_dim.Clear();
    image->Dimensions(&image_dim);
    EXPECT_FALSE(ImageUrlEncoder::HasValidDimension(image_dim));
    EXPECT_FALSE(image_dim.has_width());
    EXPECT_FALSE(image_dim.has_height());
    EXPECT_EQ(contents.size(), image->output_size());
    EXPECT_EQ("xZZ", EncodeUrlAndDimensions("ZZ", image_dim));
  }

  bool CheckImageFromFile(const char* filename,
                          ImageType input_type,
                          ImageType output_type,
                          int min_bytes_to_type,
                          int min_bytes_to_dimensions,
                          int width, int height,
                          int size, bool optimizable) {
    return CheckImageFromFile(filename, input_type, output_type, output_type,
                              min_bytes_to_type, min_bytes_to_dimensions,
                              width, height, size, optimizable);
  }

  bool CheckImageFromFile(const char* filename,
                          ImageType input_type,
                          ImageType intended_output_type,
                          ImageType actual_output_type,
                          int min_bytes_to_type,
                          int min_bytes_to_dimensions,
                          int width, int height,
                          int size, bool optimizable) {
    // Set options to convert to intended_output_type, but to allow for
    // negative tests, don't clear any other options.
    if (intended_output_type == IMAGE_WEBP) {
      options_->preferred_webp = Image::WEBP_LOSSY;
    } else if (intended_output_type == IMAGE_WEBP_LOSSLESS_OR_ALPHA) {
      options_->preferred_webp = Image::WEBP_LOSSLESS;
    }
    switch (intended_output_type) {
      case IMAGE_WEBP:
      case IMAGE_WEBP_LOSSLESS_OR_ALPHA:
        options_->convert_jpeg_to_webp = true;
        FALLTHROUGH_INTENDED;
      case IMAGE_JPEG:
        options_->convert_png_to_jpeg = true;
        FALLTHROUGH_INTENDED;
      case IMAGE_PNG:
        options_->convert_gif_to_png = true;
        break;
      default:
        break;
    }

    bool progressive = options_->progressive_jpeg;
    int jpeg_quality = options_->jpeg_quality;
    GoogleString contents;
    ImagePtr image(ReadFromFileWithOptions(
        filename, &contents, options_.release()));
    ExpectDimensions(input_type, size, width, height, image.get());
    if (optimizable) {
      EXPECT_GT(size, image->output_size());
      ExpectDimensions(actual_output_type, size, width, height, image.get());
    } else {
      EXPECT_EQ(size, image->output_size());
      ExpectDimensions(input_type, size, width, height, image.get());
    }

    // Construct data url, then decode it and check for match.
    CachedResult cached;
    GoogleString data_url;
    EXPECT_NE(IMAGE_UNKNOWN, image->image_type());
    StringPiece image_contents = image->Contents();

    progressive &= ImageTestingPeer::ShouldConvertToProgressive(jpeg_quality,
                                                                image.get());
    if (progressive) {
      EXPECT_STREQ(kProgressiveHeader, image_contents.substr(
          kProgressiveHeaderStartIndex, strlen(kProgressiveHeader)));
    }

    cached.set_inlined_data(image_contents.data(), image_contents.size());
    cached.set_inlined_image_type(static_cast<int>(image->image_type()));
    DataUrl(
        *Image::TypeToContentType(
            static_cast<ImageType>(cached.inlined_image_type())),
        BASE64, cached.inlined_data(), &data_url);
    GoogleString data_header("data:");
    data_header.append(image->content_type()->mime_type());
    data_header.append(";base64,");
    EXPECT_EQ(data_header, data_url.substr(0, data_header.size()));
    StringPiece encoded_contents(
        data_url.data() + data_header.size(),
        data_url.size() - data_header.size());
    GoogleString decoded_contents;
    EXPECT_TRUE(Mime64Decode(encoded_contents, &decoded_contents));
    EXPECT_EQ(image->Contents(), decoded_contents);

    // Now truncate the file in various ways and make sure we still
    // get partial data.
    handler_.AddPatternToSkipPrinting(kMessagePatternDataTruncated);
    handler_.AddPatternToSkipPrinting(kMessagePatternNoWebpDimension);
    GoogleString dim_data(contents, 0, min_bytes_to_dimensions);
    ImagePtr dim_image(
        ImageFromString(intended_output_type, filename, dim_data, progressive));
    ExpectDimensions(input_type, min_bytes_to_dimensions, width, height,
                     dim_image.get());
    EXPECT_EQ(min_bytes_to_dimensions, dim_image->output_size());

    GoogleString no_dim_data(contents, 0, min_bytes_to_dimensions - 1);
    CheckInvalid(filename, no_dim_data, input_type, intended_output_type,
                 progressive);
    GoogleString type_data(contents, 0, min_bytes_to_type);
    CheckInvalid(filename, type_data, input_type, intended_output_type,
                 progressive);
    GoogleString junk(contents, 0, min_bytes_to_type - 1);
    CheckInvalid(filename, junk, IMAGE_UNKNOWN, IMAGE_UNKNOWN,
                 progressive);
    return progressive;
  }

  GoogleString EncodeUrlAndDimensions(const StringPiece& origin_url,
                                      const ImageDim& dim) {
    StringVector v;
    v.push_back(origin_url.as_string());
    GoogleString out;
    ResourceContext data;
    *data.mutable_desired_image_dims() = dim;
    encoder_.Encode(v, &data, &out);
    return out;
  }

  bool DecodeUrlAndDimensions(const StringPiece& encoded,
                              ImageDim* dim,
                              GoogleString* url) {
    ResourceContext context;
    StringVector urls;
    bool result = encoder_.Decode(encoded, &urls, &context, &handler_);
    if (result) {
      EXPECT_EQ(1, urls.size());
      url->assign(urls.back());
      *dim = context.desired_image_dims();
    }
    return result;
  }

  void ExpectBadDim(const StringPiece& url) {
    GoogleString origin_url;
    ImageDim dim;
    EXPECT_FALSE(DecodeUrlAndDimensions(url, &dim, &origin_url));
    EXPECT_FALSE(ImageUrlEncoder::HasValidDimension(dim));
  }

  void SetJpegRecompressionAndQuality(Image::CompressionOptions* options) {
    options->jpeg_quality = 85;
    options->recompress_jpeg = true;
  }

  ImageUrlEncoder encoder_;
  scoped_ptr<Image::CompressionOptions> options_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageTest);
};

TEST_F(ImageTest, EmptyImageUnidentified) {
  CheckInvalid("Empty string", "", IMAGE_UNKNOWN, IMAGE_UNKNOWN,
               false);
}

TEST_F(ImageTest, InputWebpTest) {
  CheckImageFromFile(
      kScenery, IMAGE_WEBP, IMAGE_WEBP,
      20,  // Min bytes to bother checking file type at all.
      30,
      550, 368,
      30320, false);
}


TEST_F(ImageTest, WebpLowResTest) {
  // FYI: Takes ~20000 ms to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  Image::CompressionOptions* options = new Image::CompressionOptions();
  options->recompress_webp = true;
  options->preferred_webp = Image::WEBP_LOSSY;
  GoogleString contents;
  ImagePtr image(ReadFromFileWithOptions(kScenery, &contents, options));
  int filesize = 30320;
  image->SetTransformToLowRes();
  EXPECT_GT(filesize, image->output_size());
}

TEST_F(ImageTest, WebpLaLowResTest) {
  // FYI: This test will also probably take very long to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  Image::CompressionOptions* options = new Image::CompressionOptions();
  options->recompress_webp = true;
  options->preferred_webp = Image::WEBP_LOSSLESS;
  GoogleString contents;
  ImagePtr image(ReadFromFileWithOptions(kScenery, &contents, options));
  int filesize = 30320;
  image->SetTransformToLowRes();
  EXPECT_GT(filesize, image->output_size());
}

TEST_F(ImageTest, PngTest) {
  options_->recompress_png = true;
  CheckImageFromFile(
      kBikeCrash, IMAGE_PNG, IMAGE_PNG,
      ImageHeaders::kPngHeaderLength,
      ImageHeaders::kIHDRDataStart + ImageHeaders::kPngIntSize * 2,
      100, 100,
      26548, true);
}

TEST_F(ImageTest, PngToWebpTest) {
  // FYI: This test will also probably take very long to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  ConversionVarChecker conversion_var_checker(options_.get());
  options_->webp_quality = 75;
  CheckImageFromFile(
      kBikeCrash, IMAGE_PNG, IMAGE_WEBP,
      ImageHeaders::kPngHeaderLength,
      ImageHeaders::kIHDRDataStart + ImageHeaders::kPngIntSize * 2,
      100, 100,
      26548, true);
  conversion_var_checker.Test(0, 0, 0,   // gif
                              0, 1, 0,   // png
                              0, 0, 0,   // jpeg
                              true);
}

TEST_F(ImageTest, PngToWebpFailToJpegDueToPreferredTest) {
  // FYI: This test will also probably take very long to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  ConversionVarChecker conversion_var_checker(options_.get());
  options_->preferred_webp = Image::WEBP_NONE;
  options_->webp_quality = 75;
  options_->jpeg_quality = 85;
  options_->convert_jpeg_to_webp = true;
  CheckImageFromFile(
      kBikeCrash, IMAGE_PNG, IMAGE_JPEG,
      ImageHeaders::kPngHeaderLength,
      ImageHeaders::kIHDRDataStart + ImageHeaders::kPngIntSize * 2,
      100, 100,
      26548, true);
  conversion_var_checker.Test(0, 0, 0,   // gif
                              0, 0, 0,   // png
                              0, 0, 0,   // jpeg
                              true);
}

TEST_F(ImageTest, PngToWebpLaTest) {
  // FYI: This test will also probably take very long to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  ConversionVarChecker conversion_var_checker(options_.get());
  options_->webp_quality = 75;
  CheckImageFromFile(
      kBikeCrash, IMAGE_PNG, IMAGE_WEBP_LOSSLESS_OR_ALPHA,
      ImageHeaders::kPngHeaderLength,
      ImageHeaders::kIHDRDataStart + ImageHeaders::kPngIntSize * 2,
      100, 100,
      26548, true);
  conversion_var_checker.Test(0, 0, 0,   // gif
                              0, 1, 0,   // png
                              0, 0, 0,   // jpeg
                              true);
}

TEST_F(ImageTest, PngAlphaToWebpFailToPngTest) {
  // FYI: This test will also probably take very long to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  Image::CompressionOptions* options = new Image::CompressionOptions;
  ConversionVarChecker conversion_var_checker(options);
  options->preferred_webp = Image::WEBP_LOSSY;
  options->allow_webp_alpha = false;
  options->webp_quality = 75;
  options->jpeg_quality = 85;
  options->convert_png_to_jpeg = true;
  options->convert_jpeg_to_webp = true;
  EXPECT_EQ(0, options->conversions_attempted);

  GoogleString buffer;
  ImagePtr image(ReadFromFileWithOptions(kCuppaTransparent, &buffer, options));
  image->output_size();
  EXPECT_EQ(ContentType::kPng, image->content_type()->type());
  EXPECT_EQ(2, options->conversions_attempted);
  conversion_var_checker.Test(0, 0, 0,   // gif
                              0, 0, 1,   // png
                              0, 0, 0,   // jpeg
                              true);
}

TEST_F(ImageTest, PngAlphaToWebpTest) {
  // FYI: This test will also probably take very long to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  Image::CompressionOptions* options = new Image::CompressionOptions;
  ConversionVarChecker conversion_var_checker(options);
  options->preferred_webp = Image::WEBP_LOSSY;
  options->allow_webp_alpha = true;
  options->convert_png_to_jpeg = true;
  options->convert_jpeg_to_webp = true;
  options->webp_quality = 75;
  options->jpeg_quality = 85;
  EXPECT_EQ(0, options->conversions_attempted);

  GoogleString buffer;
  ImagePtr image(ReadFromFileWithOptions(kCuppaTransparent, &buffer, options));
  image->output_size();
  EXPECT_EQ(ContentType::kWebp, image->content_type()->type());
  EXPECT_EQ(1, options->conversions_attempted);
  conversion_var_checker.Test(0, 0, 0,   // gif
                              0, 1, 0,   // png
                              0, 0, 0,   // jpeg
                              false);
}

TEST_F(ImageTest, PngAlphaToWebpTestFailsBecauseTooManyTries) {
  // FYI: This test will also probably take very long to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  Image::CompressionOptions* options = new Image::CompressionOptions;
  ConversionVarChecker conversion_var_checker(options);
  options->preferred_webp = Image::WEBP_LOSSY;
  options->allow_webp_alpha = true;
  options->convert_png_to_jpeg = true;
  options->convert_jpeg_to_webp = true;
  options->webp_quality = 75;
  options->jpeg_quality = 85;
  options->conversions_attempted = 2;

  GoogleString buffer;
  ImagePtr image(ReadFromFileWithOptions(kCuppaTransparent, &buffer, options));
  image->output_size();
  EXPECT_EQ(ContentType::kPng, image->content_type()->type());
  EXPECT_EQ(2, options->conversions_attempted);
  conversion_var_checker.Test(0, 0, 0,   // gif
                              0, 0, 1,   // png
                              0, 0, 0,   // jpeg
                              false);
}

// This tests that we compress the alpha channel on the webp. If we
// don't on this image, it becomes larger than the original.
TEST_F(ImageTest, PngLargeAlphaToWebpTest) {
  // FYI: This test will also probably take very long to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  Image::CompressionOptions* options = new Image::CompressionOptions;
  ConversionVarChecker conversion_var_checker(options);
  options->preferred_webp = Image::WEBP_LOSSY;
  options->allow_webp_alpha = true;
  options->convert_png_to_jpeg = true;
  options->convert_jpeg_to_webp = true;
  options->webp_quality = 75;
  options->jpeg_quality = 85;
  EXPECT_EQ(0, options->conversions_attempted);

  GoogleString buffer;
  ImagePtr image(ReadFromFileWithOptions(kRedbrush, &buffer, options));
  EXPECT_GT(image->input_size(), image->output_size());
  EXPECT_EQ(ContentType::kWebp, image->content_type()->type());
  EXPECT_EQ(1, options->conversions_attempted);
  conversion_var_checker.Test(0, 0, 0,   // gif
                              0, 1, 0,   // png
                              0, 0, 0,   // jpeg
                              false);
}

// This tests that we compress the alpha channel on the webp. If we
// don't on this image, it becomes larger than the original.
TEST_F(ImageTest, PngLargeAlphaToWebpLaTest) {
  // FYI: This test will also probably take very long to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  Image::CompressionOptions* options = new Image::CompressionOptions;
  ConversionVarChecker conversion_var_checker(options);
  options->preferred_webp = Image::WEBP_LOSSLESS;
  options->allow_webp_alpha = true;
  options->convert_png_to_jpeg = true;
  options->convert_jpeg_to_webp = true;
  options->webp_quality = 75;
  options->jpeg_quality = 85;
  EXPECT_EQ(0, options->conversions_attempted);

  GoogleString buffer;
  ImagePtr image(ReadFromFileWithOptions(kRedbrush, &buffer, options));
  EXPECT_GT(image->input_size(), image->output_size());
  EXPECT_EQ(ContentType::kWebp, image->content_type()->type());
  EXPECT_EQ(1, options->conversions_attempted);
  conversion_var_checker.Test(0, 0, 0,   // gif
                              0, 1, 0,   // png
                              0, 0, 0,   // jpeg
                              false);
  // TODO(vchudnov): Check that the pixels match.
}

// Same image and settings that succeed in PngLargeAlphaToWebpTest,
// should fail when using a very short timeout.
TEST_F(ImageTest, PngLargeAlphaToWebpTimesOutToPngTest) {
  // FYI: This test will also probably take very long to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  Image::CompressionOptions* options = new Image::CompressionOptions;
  ConversionVarChecker conversion_var_checker(options);
  options->preferred_webp = Image::WEBP_LOSSY;
  options->allow_webp_alpha = true;
  options->convert_png_to_jpeg = true;
  options->convert_jpeg_to_webp = true;
  options->webp_quality = 75;
  options->jpeg_quality = 85;
  options->webp_conversion_timeout_ms = 1;
  EXPECT_EQ(0, options->conversions_attempted);
  conversion_var_checker.Test(0, 0, 0,   // gif
                              0, 0, 0,   // png
                              0, 0, 0,   // jpeg
                              false);

  handler_.AddPatternToSkipPrinting(kMessagePatternTimedOut);
  GoogleString buffer;
  ImagePtr image(ReadFromFileWithOptions(kRedbrush, &buffer, options));
  timer_.SetTimeDeltaUs(1);  // When setting deadline
  timer_.SetTimeDeltaUs(1);  // Before attempting webp lossless
  timer_.SetTimeDeltaUs(     // During conversion
      1000 * options->webp_conversion_timeout_ms + 1);
  image->output_size();
  EXPECT_EQ(ContentType::kPng, image->content_type()->type());
  conversion_var_checker.Test(0, 0, 0,   // gif
                              1, 0, 0,   // png
                              0, 0, 0,   // jpeg
                              false);

  // One attempt for WebpP conversion, one attempt for the fall-back
  // to PNG/JPEG.
  EXPECT_EQ(2, options->conversions_attempted);
}

// Same image and settings that succeed in PngLargeAlphaToWebpTest,
// should succeed if processing is really fast.
TEST_F(ImageTest, PngLargeAlphaToWebpDoesNotTimeOutTest) {
  // FYI: This test will also probably take very long to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  Image::CompressionOptions* options = new Image::CompressionOptions;
  ConversionVarChecker conversion_var_checker(options);
  options->preferred_webp = Image::WEBP_LOSSY;
  options->allow_webp_alpha = true;
  options->convert_png_to_jpeg = true;
  options->convert_jpeg_to_webp = true;
  options->webp_quality = 75;
  options->jpeg_quality = 85;
  options->webp_conversion_timeout_ms = 1;
  EXPECT_EQ(0, options->conversions_attempted);
  conversion_var_checker.Test(0, 0, 0,   // gif
                              0, 0, 0,   // png
                              0, 0, 0,   // jpeg
                              false);

  GoogleString buffer;
  ImagePtr image(ReadFromFileWithOptions(kRedbrush, &buffer, options));
  timer_.SetTimeDeltaUs(1);  // When setting deadline
  timer_.SetTimeDeltaUs(1);  // Before attempting webp lossless
  timer_.SetTimeDeltaUs(     // During conversion
      1000 * options->webp_conversion_timeout_ms - 2);
  image->output_size();
  EXPECT_EQ(ContentType::kWebp, image->content_type()->type());
  conversion_var_checker.Test(0, 0, 0,   // gif
                              0, 1, 0,   // png
                              0, 0, 0,   // jpeg
                              false);

  // One attempt for WebpP conversion.
  EXPECT_EQ(1, options->conversions_attempted);
}

TEST_F(ImageTest, PngToJpegTest) {
  options_->jpeg_quality = 85;
  CheckImageFromFile(
      kBikeCrash, IMAGE_PNG, IMAGE_JPEG,
      ImageHeaders::kPngHeaderLength,
      ImageHeaders::kIHDRDataStart + ImageHeaders::kPngIntSize * 2,
      100, 100,
      26548, true);
}

TEST_F(ImageTest, TooSmallToConvertPngToProgressiveJpegTest) {
  options_->progressive_jpeg = true;
  options_->jpeg_quality = 85;
  bool progressive = CheckImageFromFile(
      kBikeCrash, IMAGE_PNG, IMAGE_JPEG,
      ImageHeaders::kPngHeaderLength,
      ImageHeaders::kIHDRDataStart + ImageHeaders::kPngIntSize * 2,
      100, 100,
      26548, true);
  EXPECT_FALSE(progressive);
}

TEST_F(ImageTest, PngToProgressiveJpegTest) {
  options_->progressive_jpeg = true;
  options_->jpeg_quality = 85;
  options_->progressive_jpeg_min_bytes = 100;  // default is 10k.
  bool progressive = CheckImageFromFile(
      kBikeCrash, IMAGE_PNG, IMAGE_JPEG,
      ImageHeaders::kPngHeaderLength,
      ImageHeaders::kIHDRDataStart + ImageHeaders::kPngIntSize * 2,
      100, 100,
      26548, true);
  EXPECT_TRUE(progressive);
}

TEST_F(ImageTest, GifToPngTest) {
  CheckImageFromFile(
      kIronChef, IMAGE_GIF, IMAGE_PNG,
      8,  // Min bytes to bother checking file type at all.
      ImageHeaders::kGifDimStart + ImageHeaders::kGifIntSize * 2,
      192, 256,
      24941, true);
}

TEST_F(ImageTest, GifToPngDisabledTest) {
  Image::CompressionOptions* options = new Image::CompressionOptions;
  options->convert_gif_to_png = false;
  EXPECT_EQ(0, options->conversions_attempted);

  GoogleString buffer;
  ImagePtr image(ReadFromFileWithOptions(kIronChef, &buffer, options));
  image->output_size();
  EXPECT_EQ(ContentType::kGif, image->content_type()->type());
  EXPECT_EQ(0, options->conversions_attempted);
}

TEST_F(ImageTest, GifToJpegTest) {
  options_->jpeg_quality = 85;
  CheckImageFromFile(
      kIronChef, IMAGE_GIF, IMAGE_JPEG,
      8,  // Min bytes to bother checking file type at all.
      ImageHeaders::kGifDimStart + ImageHeaders::kGifIntSize * 2,
      192, 256,
      24941, true);
}

TEST_F(ImageTest, GifToWebpTest) {
  // FYI: This test will also probably take very long to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  ConversionVarChecker conversion_var_checker(options_.get());
  options_->webp_quality = 25;
  CheckImageFromFile(
      kIronChef, IMAGE_GIF, IMAGE_WEBP,
      8,  // Min bytes to bother checking file type at all.
      ImageHeaders::kGifDimStart + ImageHeaders::kGifIntSize * 2,
      192, 256,
      24941, true);
  conversion_var_checker.Test(0, 1, 0,   // gif
                              0, 0, 0,   // png
                              0, 0, 0,   // jpeg
                              true);
}

TEST_F(ImageTest, GifToWebpLaTest) {
  // FYI: This test will also probably take very long to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  ConversionVarChecker conversion_var_checker(options_.get());
  options_->webp_quality = 75;
  CheckImageFromFile(
      kIronChef, IMAGE_GIF, IMAGE_WEBP_LOSSLESS_OR_ALPHA,
      8,  // Min bytes to bother checking file type at all.
      ImageHeaders::kGifDimStart + ImageHeaders::kGifIntSize * 2,
      192, 256,
      24941, true);
  conversion_var_checker.Test(0, 1, 0,   // gif
                              0, 0, 0,   // png
                              0, 0, 0,   // jpeg
                              true);
}

TEST_F(ImageTest, AnimationTest) {
  CheckImageFromFile(
      kCradle, IMAGE_GIF, IMAGE_PNG,
      8,  // Min bytes to bother checking file type at all.
      ImageHeaders::kGifDimStart + ImageHeaders::kGifIntSize * 2,
      200, 150,
      583374, false);
}

TEST_F(ImageTest, JpegTest) {
  options_->recompress_jpeg = true;
  CheckImageFromFile(
      kPuzzle, IMAGE_JPEG, IMAGE_JPEG,
      8,  // Min bytes to bother checking file type at all.
      6468,  // Specific to this test
      1023, 766,
      241260, true);
}

TEST_F(ImageTest, ProgressiveJpegTest) {
  options_->recompress_jpeg = true;
  options_->progressive_jpeg = true;
  CheckImageFromFile(
      kPuzzle, IMAGE_JPEG, IMAGE_JPEG,
      8,  // Min bytes to bother checking file type at all.
      6468,  // Specific to this test
      1023, 766,
      241260, true);
}

TEST_F(ImageTest, NumProgressiveScansTest) {
  Image::CompressionOptions* options = new Image::CompressionOptions();
  SetJpegRecompressionAndQuality(options);
  options->progressive_jpeg = true;
  options->jpeg_num_progressive_scans = 3;

  GoogleString buffer;
  ImagePtr image(ReadFromFileWithOptions(kPuzzle, &buffer, options));
  EXPECT_GT(buffer.size(), image->output_size());
  EXPECT_EQ(3, GetNumScansInJpeg(image->Contents().as_string()));
}

TEST_F(ImageTest, UseJpegLossyIfInputQualityIsLowTest) {
  Image::CompressionOptions* options = new Image::CompressionOptions();
  SetJpegRecompressionAndQuality(options);
  options->progressive_jpeg = true;

  GoogleString buffer;
  // Input image quality is 50.
  ImagePtr image(ReadFromFileWithOptions(kAppSegments, &buffer, options));
  EXPECT_GT(buffer.size(), image->output_size());
  EXPECT_EQ(
      50, JpegUtils::GetImageQualityFromImage(image->Contents().data(),
                                              image->Contents().size()));

  // When num progressive scans is set, we use lossy path. The compression
  // quality is the minimum of the input and the configuration, i.e., 50.
  options = new Image::CompressionOptions();
  SetJpegRecompressionAndQuality(options);
  options->progressive_jpeg = true;
  buffer.clear();
  options->jpeg_num_progressive_scans = 1;
  image.reset(ReadFromFileWithOptions(kAppSegments, &buffer, options));
  EXPECT_GT(buffer.size(), image->output_size());
  EXPECT_EQ(
      50, JpegUtils::GetImageQualityFromImage(image->Contents().data(),
                                              image->Contents().size()));

  // Empty image will return -1 when we try to determine its quality.
  options = new Image::CompressionOptions();
  SetJpegRecompressionAndQuality(options);
  options->progressive_jpeg = true;
  image.reset(NewImage("", "", GTestTempDir(), options,
                       &timer_, &handler_));
  EXPECT_EQ(
      -1, JpegUtils::GetImageQualityFromImage(image->Contents().data(),
                                              image->Contents().size()));
}

TEST_F(ImageTest, JpegRetainColorProfileTest) {
  Image::CompressionOptions* options = new Image::CompressionOptions();
  SetJpegRecompressionAndQuality(options);
  options->retain_color_profile = true;

  GoogleString buffer;
  ImagePtr image(ReadFromFileWithOptions(kAppSegments, &buffer, options));
  EXPECT_TRUE(IsJpegSegmentPresent(buffer, GetColorProfileMarker()));
  EXPECT_GT(buffer.size(), image->output_size());
  EXPECT_TRUE(IsJpegSegmentPresent(image->Contents().as_string(),
                                   GetColorProfileMarker()));
  // Try stripping the color profile information.
  options = new Image::CompressionOptions();
  SetJpegRecompressionAndQuality(options);
  options->retain_color_profile = false;
  buffer.clear();
  image.reset(ReadFromFileWithOptions(kAppSegments, &buffer, options));
  EXPECT_TRUE(IsJpegSegmentPresent(buffer, GetColorProfileMarker()));
  EXPECT_GT(buffer.size(), image->output_size());
  EXPECT_FALSE(IsJpegSegmentPresent(image->Contents().as_string(),
                                    GetColorProfileMarker()));
}

TEST_F(ImageTest, JpegRetainColorSamplingTest) {
  int num_components, h_sampling_factor, v_sampling_factor;
  Image::CompressionOptions* options = new Image::CompressionOptions();
  SetJpegRecompressionAndQuality(options);
  options->retain_color_profile = false;

  GoogleString buffer;
  // Input image color sampling is YUV 422. By defult we force YUV420.
  ImagePtr image(ReadFromFileWithOptions(kPuzzle, &buffer, options));
  GetJpegNumComponentsAndSamplingFactors(
      buffer, &num_components, &h_sampling_factor, &v_sampling_factor);
  EXPECT_EQ(3, num_components);
  EXPECT_EQ(2, h_sampling_factor);
  EXPECT_EQ(1, v_sampling_factor);
  EXPECT_GT(buffer.size(), image->output_size());
  GetJpegNumComponentsAndSamplingFactors(
      image->Contents().as_string(), &num_components, &h_sampling_factor,
      &v_sampling_factor);
  EXPECT_EQ(3, num_components);
  EXPECT_EQ(2, h_sampling_factor);
  EXPECT_EQ(2, v_sampling_factor);

  // Try retaining the color sampling.
  options = new Image::CompressionOptions();
  SetJpegRecompressionAndQuality(options);
  options->retain_color_sampling = true;
  buffer.clear();
  image.reset(ReadFromFileWithOptions(kPuzzle, &buffer, options));
  EXPECT_GT(buffer.size(), image->output_size());
  GetJpegNumComponentsAndSamplingFactors(
      image->Contents().as_string(), &num_components, &h_sampling_factor,
      &v_sampling_factor);
  EXPECT_EQ(3, num_components);
  EXPECT_EQ(2, h_sampling_factor);
  EXPECT_EQ(1, v_sampling_factor);
}

TEST_F(ImageTest, JpegRetainExifDataTest) {
  Image::CompressionOptions* options = new Image::CompressionOptions();
  SetJpegRecompressionAndQuality(options);
  options->retain_exif_data = true;

  GoogleString buffer;
  ImagePtr image(ReadFromFileWithOptions(kAppSegments, &buffer, options));
  EXPECT_TRUE(IsJpegSegmentPresent(buffer, GetExifDataMarker()));
  EXPECT_GT(buffer.size(), image->output_size());
  EXPECT_TRUE(IsJpegSegmentPresent(image->Contents().as_string(),
                                   GetExifDataMarker()));
  // Try stripping the color profile information.
  options = new Image::CompressionOptions();
  SetJpegRecompressionAndQuality(options);
  options->retain_exif_data = false;
  buffer.clear();
  image.reset(ReadFromFileWithOptions(kAppSegments, &buffer, options));
  EXPECT_TRUE(IsJpegSegmentPresent(buffer, GetExifDataMarker()));
  EXPECT_GT(buffer.size(), image->output_size());
  EXPECT_FALSE(IsJpegSegmentPresent(image->Contents().as_string(),
                                    GetExifDataMarker()));
}

TEST_F(ImageTest, WebpTest) {
  // FYI: Takes ~70000 ms to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  options_->webp_quality = 75;
  CheckImageFromFile(
      kPuzzle, IMAGE_JPEG, IMAGE_WEBP,
      8,  // Min bytes to bother checking file type at all.
      6468,  // Specific to this test
      1023, 766,
      241260, true);
}

TEST_F(ImageTest, JpegToWebpTimesOutTest) {
  Image::CompressionOptions* options = new Image::CompressionOptions;
  ConversionVarChecker conversion_var_checker(options);
  options->recompress_jpeg = true;
  options->convert_jpeg_to_webp = true;
  options->preferred_webp = Image::WEBP_LOSSY;
  options->webp_quality = 75;
  options->webp_conversion_timeout_ms = 1;
  timer_.SetTimeDeltaUs(1);  // When setting deadline
  timer_.SetTimeDeltaUs(     // During conversion
      1000 * options->webp_conversion_timeout_ms + 1);

  EXPECT_EQ(0, options->conversions_attempted);
  conversion_var_checker.Test(0, 0, 0,   // gif
                              0, 0, 0,   // png
                              0, 0, 0,   // jpeg
                              true);

  handler_.AddPatternToSkipPrinting(kMessagePatternFailedToCreateWebp);
  handler_.AddPatternToSkipPrinting(kMessagePatternTimedOut);
  GoogleString buffer;
  ImagePtr image(ReadFromFileWithOptions(kPuzzle, &buffer, options));
  image->output_size();
  EXPECT_EQ(ContentType::kJpeg, image->content_type()->type());

  EXPECT_EQ(2, options->conversions_attempted);
  conversion_var_checker.Test(0, 0, 0,   // gif
                              0, 0, 0,   // png
                              1, 0, 0,   // jpeg
                              true);
}

TEST_F(ImageTest, JpegToWebpDoesNotTimeOutTest) {
  // FYI: This test will probably take very long to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  Image::CompressionOptions* options = new Image::CompressionOptions;
  ConversionVarChecker conversion_var_checker(options);
  options->recompress_jpeg = true;
  options->convert_jpeg_to_webp = true;
  options->preferred_webp = Image::WEBP_LOSSY;
  options->webp_quality = 75;
  options->webp_conversion_timeout_ms = 1;
  timer_.SetTimeDeltaUs(1);  // When setting deadline
  timer_.SetTimeDeltaUs(     // During conversion
      1000 * options->webp_conversion_timeout_ms - 1);

  EXPECT_EQ(0, options->conversions_attempted);
  conversion_var_checker.Test(0, 0, 0,   // gif
                              0, 0, 0,   // png
                              0, 0, 0,   // jpeg
                              true);

  handler_.AddPatternToSkipPrinting(kMessagePatternTimedOut);
  GoogleString buffer;
  ImagePtr image(ReadFromFileWithOptions(kPuzzle, &buffer, options));
  image->output_size();
  EXPECT_EQ(ContentType::kWebp, image->content_type()->type());

  EXPECT_EQ(1, options->conversions_attempted);
  conversion_var_checker.Test(0, 0, 0,   // gif
                              0, 0, 0,   // png
                              0, 1, 0,   // jpeg
                              true);
}

TEST_F(ImageTest, WebpNonLaFromJpgTest) {
  // FYI: Takes ~70000 ms to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }
  ConversionVarChecker conversion_var_checker(options_.get());
  options_->webp_quality = 75;
  // Note that jpeg->webp cannot return a lossless webp.
  CheckImageFromFile(
      kPuzzle, IMAGE_JPEG, IMAGE_WEBP_LOSSLESS_OR_ALPHA,
      IMAGE_WEBP,
      8,  // Min bytes to bother checking file type at all.
      6468,  // Specific to this test
      1023, 766,
      241260, true);
  conversion_var_checker.Test(0, 0, 0,   // gif
                              0, 0, 0,   // png
                              0, 1, 0,   // jpeg
                              true);
}

TEST_F(ImageTest, DrawImage) {
  Image::CompressionOptions* options = new Image::CompressionOptions();
  options->recompress_png = true;
  GoogleString buf1;
  ImagePtr image1(ReadFromFileWithOptions(kBikeCrash, &buf1, options));
  ImageDim image_dim1;
  image1->Dimensions(&image_dim1);

  options = new Image::CompressionOptions();
  options->recompress_png = true;
  GoogleString buf2;
  ImagePtr image2(ReadFromFileWithOptions(kCuppa, &buf2, options));
  ImageDim image_dim2;
  image2->Dimensions(&image_dim2);

  int width = std::max(image_dim1.width(), image_dim2.width());
  int height = image_dim1.height() + image_dim2.height();
  ASSERT_GT(width, 0);
  ASSERT_GT(height, 0);
  options = new Image::CompressionOptions();
  options->recompress_png = true;
  ImagePtr canvas(BlankImageWithOptions(width, height, IMAGE_PNG,
                                        GTestTempDir(), &timer_,
                                        &handler_, options));
  EXPECT_TRUE(canvas->DrawImage(image1.get(), 0, 0));
  EXPECT_TRUE(canvas->DrawImage(image2.get(), 0, image_dim1.height()));
  // The combined image should be bigger than either of the components, but
  // smaller than their unoptimized sum.
  EXPECT_GT(canvas->output_size(), image1->output_size());
  EXPECT_GT(canvas->output_size(), image2->output_size());
  EXPECT_GT(image1->input_size() + image2->input_size(),
            canvas->output_size());
}

TEST_F(ImageTest, BlankWhiteImage) {
  int width = 1000, height = 1000;
  Image::CompressionOptions* options = new Image::CompressionOptions();

  options->use_white_for_blank_image = true;
  ImagePtr blank(BlankImageWithOptions(width, height, IMAGE_PNG, GTestTempDir(),
                                       &timer_, &handler_, options));
  bool loaded = blank->EnsureLoaded(false);
  EXPECT_EQ(loaded, true);
  EXPECT_GT(blank->Contents().size(), 0);

  ImageDim blank_dim;
  blank->Dimensions(&blank_dim);
  EXPECT_EQ(blank_dim.width(), width);
  EXPECT_EQ(blank_dim.height(), height);
}

// Test OpenCV bug where width * height of image could be allocated on the
// stack. kLarge is a 10000x10000 image, so it will try to allocate > 100MB
// on the stack, which should overflow the stack and SEGV.
TEST_F(ImageTest, OpencvStackOverflow) {
  // This test takes ~90000 ms on Valgrind and need not be run there.
  if (RunningOnValgrind()) {
    return;
  }

  GoogleString buf;
  ImagePtr image(ReadImageFromFile(IMAGE_JPEG, kLarge, &buf, false));

  ImageDim new_dim;
  new_dim.set_width(1);
  new_dim.set_height(1);
  image->ResizeTo(new_dim);
}

TEST_F(ImageTest, ResizeTo) {
  GoogleString buf;
  ImagePtr image(ReadImageFromFile(IMAGE_JPEG, kPuzzle, &buf, false));

  ImageDim new_dim;
  new_dim.set_width(10);
  new_dim.set_height(10);
  image->ResizeTo(new_dim);

  ExpectEmptyOutput(image.get());
  ExpectContentType(IMAGE_JPEG, image.get());
}

TEST_F(ImageTest, CompressJpegUsingLossyOrLossless) {
  Image::CompressionOptions* options = new Image::CompressionOptions();
  SetJpegRecompressionAndQuality(options);
  GoogleString buffer;

  // Input image quality is 50. When jpeg_quality is set to -1, lossless
  // will be used and the quality of the input image will be preserved.
  options->jpeg_quality = -1;
  ImagePtr image(ReadFromFileWithOptions(kAppSegments, &buffer, options));
  EXPECT_GT(buffer.size(), image->output_size());
  EXPECT_EQ(
      50, JpegUtils::GetImageQualityFromImage(image->Contents().data(),
                                              image->Contents().size()));

  // When jpeg_num_progressive_scans > 0, lossy will be used and the quality
  // will be set to the minimum of input quality and jpeg_quality.
  options = new Image::CompressionOptions();
  SetJpegRecompressionAndQuality(options);
  options->jpeg_num_progressive_scans = 1;
  options->jpeg_quality = 51;
  buffer.clear();
  image.reset(ReadFromFileWithOptions(kAppSegments, &buffer, options));
  EXPECT_GT(buffer.size(), image->output_size());
  EXPECT_EQ(
      50, JpegUtils::GetImageQualityFromImage(image->Contents().data(),
                                              image->Contents().size()));

  // When jpeg_quality is less than input quality, lossy will be used and the
  // output quality is the minimum of them.
  options = new Image::CompressionOptions();
  SetJpegRecompressionAndQuality(options);
  options->jpeg_quality = 49;
  buffer.clear();
  image.reset(ReadFromFileWithOptions(kAppSegments, &buffer, options));
  EXPECT_GT(buffer.size(), image->output_size());
  EXPECT_EQ(
      49, JpegUtils::GetImageQualityFromImage(image->Contents().data(),
                                              image->Contents().size()));
}

void SetBaseJpegOptions(Image::CompressionOptions* options) {
  options->preferred_webp = Image::WEBP_LOSSY;
  options->allow_webp_alpha = true;
  options->convert_gif_to_png = true;
  options->convert_png_to_jpeg = true;
  options->webp_quality = 75;
  options->jpeg_quality = 85;
}

TEST_F(ImageTest, IgnoreTimeoutWhenFinishingWebp) {
  // FYI: This test will also probably take very long to run under Valgrind.
  if (RunningOnValgrind()) {
    return;
  }

  // Get the jpeg reference image
  Image::CompressionOptions* jpeg_options = new Image::CompressionOptions;
  SetBaseJpegOptions(jpeg_options);

  handler_.AddPatternToSkipPrinting(kMessagePatternTimedOut);
  GoogleString jpeg_buffer;
  ImagePtr jpeg_image(ReadFromFileWithOptions(kBikeCrash,
                                              &jpeg_buffer,
                                              jpeg_options));

  jpeg_image->output_size();
  EXPECT_EQ(ContentType::kJpeg, jpeg_image->content_type()->type());


  // Get the webp reference image
  Image::CompressionOptions* webp_options = new Image::CompressionOptions;
  SetBaseJpegOptions(webp_options);
  webp_options->convert_jpeg_to_webp = true;
  webp_options->webp_conversion_timeout_ms = 1;
  GoogleString webp_buffer;
  ImagePtr webp_image(ReadFromFileWithOptions(kBikeCrash,
                                              &webp_buffer,
                                              webp_options));

  webp_image->output_size();
  EXPECT_EQ(ContentType::kWebp, webp_image->content_type()->type());


  // Make sure that if the timeout occurs before the first byte is
  // written, we do indeed time out.
  Image::CompressionOptions* timed_out_webp_options =
      new Image::CompressionOptions;
  SetBaseJpegOptions(timed_out_webp_options);
  timed_out_webp_options->convert_jpeg_to_webp = true;
  timed_out_webp_options->webp_conversion_timeout_ms = 1;

  GoogleString timed_out_webp_buffer;
  ImagePtr timed_out_webp_image(
      ReadFromFileWithOptions(kBikeCrash,
                              &timed_out_webp_buffer,
                              timed_out_webp_options));
  timer_.SetTimeMs(10);
  timer_.SetTimeDeltaUs(1);  // When setting deadline
  timer_.SetTimeDeltaUs(1);  // Before attempting webp lossless
  timer_.SetTimeDeltaUs(1);
  timer_.SetTimeDeltaUs(2000);

  timed_out_webp_image->output_size();
  EXPECT_EQ(ContentType::kJpeg, timed_out_webp_image->content_type()->type());
  EXPECT_EQ(jpeg_image->Contents(),
            timed_out_webp_image->Contents());

  // Test that if we time out after the first output byte is emitted, we keep
  // going with the webp output.
  Image::CompressionOptions* almost_done_webp_options =
      new Image::CompressionOptions;
  SetBaseJpegOptions(almost_done_webp_options);
  almost_done_webp_options->convert_jpeg_to_webp = true;
  almost_done_webp_options->webp_conversion_timeout_ms = 1;

  const char* kSomeData = "some data";
  GoogleString almost_done_webp_buffer;
  ImagePtr almost_done_webp_image(
      ReadFromFileWithOptions(kBikeCrash,
                              &almost_done_webp_buffer,
                              almost_done_webp_options));
  timer_.SetTimeMs(20);
  timer_.SetTimeDeltaUs(1);  // When setting deadline
  timer_.SetTimeDeltaUs(1);  // Before attempting webp lossless
  timer_.SetTimeDeltaUs(1);
  timer_.SetTimeDeltaUs(1);
  timer_.SetTimeDeltaUs(1);
  timer_.SetTimeDeltaUs(1);
  // We need to specify the template typenames explicitly below
  // because the compiler can't decide whether to use this test class
  // or its base class, ImageTest.
  timer_.SetTimeDeltaUsWithCallback(
      2000,
      MakeFunction<
        ImageTest_IgnoreTimeoutWhenFinishingWebp_Test,
        const char*,
        GoogleString*>(
            this,
            &ImageTest_IgnoreTimeoutWhenFinishingWebp_Test::WriteToBuffer,
            kSomeData,
            GetOutputContents(almost_done_webp_image.get())));

  almost_done_webp_image->output_size();
  EXPECT_EQ(ContentType::kWebp,
            almost_done_webp_image->content_type()->type());
  GoogleString expected = kSomeData;
  expected.append(webp_image->Contents().as_string());
  EXPECT_EQ(expected,
            almost_done_webp_image->Contents());
}
}  // namespace net_instaweb
