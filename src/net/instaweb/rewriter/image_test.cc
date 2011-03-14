// Copyright 2010 Google Inc. All Rights Reserved.
// Author: jmaessen@google.com (Jan Maessen)

// Unit tests for Image class used in rewriting.

#include "net/instaweb/rewriter/public/image.h"
#include "net/instaweb/rewriter/public/img_rewrite_filter.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/base64_util.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/url_escaper.h"

namespace {

const char kTestData[] = "/net/instaweb/rewriter/testdata/";
const char kCuppa[] = "Cuppa.png";
const char kBikeCrash[] = "BikeCrashIcn.png";
const char kIronChef[] = "IronChef2.gif";
const char kCradle[] = "CradleAnimation.gif";
const char kPuzzle[] = "Puzzle.jpg";

}  // namespace

namespace net_instaweb {

class ImageTest : public testing::Test {
 protected:
  typedef scoped_ptr<Image> ImagePtr;

  ImageTest() { }
  Image* ImageFromString(const std::string& name,
                         const std::string& contents) {
    return new Image(contents, name, GTestTempDir(), &handler_);
  }

  void ExpectDimensions(Image::Type image_type, int size,
                        int expected_width, int expected_height,
                        Image *image) {
    EXPECT_EQ(size, image->input_size());
    EXPECT_EQ(image_type, image->image_type());
    // Arbitrary but bogus values to make sure we get dimensions.
    ImageDim image_dim;
    image_dim.set_dims(-7, -9);
    image_dim.invalidate();
    image->Dimensions(&image_dim);
    EXPECT_TRUE(image_dim.valid());
    EXPECT_EQ(expected_width, image_dim.width());
    EXPECT_EQ(expected_height, image_dim.height());
    std::string encoded("ZZ");
    image_dim.EncodeTo(&encoded);
    EXPECT_EQ(StringPrintf("ZZ%dx%dx", image_dim.width(), image_dim.height()),
              encoded);
  }

  void CheckInvalid(const std::string& name, const std::string& contents,
                    Image::Type image_type) {
    ImagePtr image(ImageFromString(name, contents));
    EXPECT_EQ(contents.size(), image->input_size());
    EXPECT_EQ(image_type, image->image_type());
    // Arbitrary but bogus values to check for accidental modification.
    ImageDim image_dim;
    image_dim.set_dims(-7, -9);
    image_dim.invalidate();
    image->Dimensions(&image_dim);
    EXPECT_FALSE(image_dim.valid());
    EXPECT_EQ(-7, image_dim.width());
    EXPECT_EQ(-9, image_dim.height());
    EXPECT_EQ(contents.size(), image->output_size());
    std::string encoded("ZZ");
    image_dim.EncodeTo(&encoded);
    EXPECT_EQ("ZZx", encoded);
  }

  Image* ReadImageFromFile(const char* filename, std::string* buffer) {
    EXPECT_TRUE(file_system_.ReadFile(
        StrCat(GTestSrcDir(), kTestData, filename).c_str(),
        buffer, &handler_));
    return ImageFromString(filename, *buffer);
  }

  void CheckImageFromFile(const char* filename,
                          Image::Type image_type,
                          int min_bytes_to_type,
                          int min_bytes_to_dimensions,
                          int width, int height,
                          int size, bool optimizable) {
    std::string contents;
    ImagePtr image(ReadImageFromFile(filename, &contents));
    ExpectDimensions(image_type, size, width, height, image.get());
    if (optimizable) {
      EXPECT_GT(size, image->output_size());
    } else {
      EXPECT_EQ(size, image->output_size());
    }

    // Construct data url, then decode it and check for match.
    std::string data_url;
    EXPECT_FALSE(NULL == image->content_type());
    EXPECT_TRUE(ImgRewriteFilter::CanInline(
        image->output_size(), image->Contents(), image->content_type(),
        &data_url));
    std::string data_header("data:");
    data_header.append(image->content_type()->mime_type());
    data_header.append(";base64,");
    EXPECT_EQ(data_header, data_url.substr(0, data_header.size()));
    StringPiece encoded_contents(
        data_url.data() + data_header.size(),
        data_url.size() - data_header.size());
    std::string decoded_contents;
    EXPECT_TRUE(Mime64Decode(encoded_contents, &decoded_contents));
    EXPECT_EQ(image->Contents(), decoded_contents);

    // Now truncate the file in various ways and make sure we still
    // get partial data.
    std::string dim_data(contents, 0, min_bytes_to_dimensions);
    ImagePtr dim_image(ImageFromString(filename, dim_data));
    ExpectDimensions(image_type, min_bytes_to_dimensions, width, height,
                     dim_image.get());
    EXPECT_EQ(min_bytes_to_dimensions, dim_image->output_size());

    std::string no_dim_data(contents, 0, min_bytes_to_dimensions - 1);
    CheckInvalid(filename, no_dim_data, image_type);
    std::string type_data(contents, 0, min_bytes_to_type);
    CheckInvalid(filename, type_data, image_type);
    std::string junk(contents, 0, min_bytes_to_type - 1);
    CheckInvalid(filename, junk, Image::IMAGE_UNKNOWN);
  }

  // These tests are local methods here in order to obtain access to local
  // constants in the friend class Image.

  void DoPngTest() {
    CheckImageFromFile(
        kBikeCrash, Image::IMAGE_PNG,
        ImageHeaders::kPngHeaderLength,
        ImageHeaders::kIHDRDataStart + ImageHeaders::kPngIntSize * 2,
        100, 100,
        26548, true);
  }

  void DoGifTest() {
    CheckImageFromFile(
        kIronChef, Image::IMAGE_GIF,
        8,  // Min bytes to bother checking file type at all.
        ImageHeaders::kGifDimStart + ImageHeaders::kGifIntSize * 2,
        192, 256,
        24941, true);
  }

  void DoAnimationTest() {
    CheckImageFromFile(
        kCradle, Image::IMAGE_GIF,
        8,  // Min bytes to bother checking file type at all.
        ImageHeaders::kGifDimStart + ImageHeaders::kGifIntSize * 2,
        200, 150,
        583374, false);
  }

  void DoJpegTest() {
    CheckImageFromFile(
        kPuzzle, Image::IMAGE_JPEG,
        8,  // Min bytes to bother checking file type at all.
        6468,  // Specific to this test
        1023, 766,
        241260, true);
  }

  std::string EncodeUrlAndDimensions(const StringPiece& origin_url,
                                      const ImageDim& dim) {
    ResourceContext data;
    dim.ToResourceContext(&data);
    StringVector v;
    v.push_back(origin_url.as_string());
    std::string out;
    ImageUrlEncoder encoder;
    encoder.Encode(v, &data, &out);
    return out;
  }

  StdioFileSystem file_system_;
  GoogleMessageHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(ImageTest);
};

TEST_F(ImageTest, EmptyImageUnidentified) {
  CheckInvalid("Empty string", "", Image::IMAGE_UNKNOWN);
}

TEST_F(ImageTest, PngTest) {
  DoPngTest();
}

TEST_F(ImageTest, GifTest) {
  DoGifTest();
}

TEST_F(ImageTest, AnimationTest) {
  DoAnimationTest();
}

TEST_F(ImageTest, JpegTest) {
  DoJpegTest();
}

TEST_F(ImageTest, DrawImage) {
  std::string buf1;
  ImagePtr image1(ReadImageFromFile(kBikeCrash, &buf1));
  ImageDim image_dim1;
  image1->Dimensions(&image_dim1);

  std::string buf2;
  ImagePtr image2(ReadImageFromFile(kCuppa, &buf2));
  ImageDim image_dim2;
  image2->Dimensions(&image_dim2);

  int width = std::max(image_dim1.width(), image_dim2.width());
  int height = image_dim1.height() + image_dim2.height();
  ASSERT_GT(width, 0);
  ASSERT_GT(height, 0);
  ImagePtr canvas(new Image(width, height, Image::IMAGE_PNG,
                            GTestTempDir(), &handler_));
  EXPECT_TRUE(canvas->DrawImage(image1.get(), 0, 0));
  EXPECT_TRUE(canvas->DrawImage(image2.get(), 0, image_dim1.height()));
  // The combined image should be bigger than either of the components, but
  // smaller than their unoptimized sum.
  EXPECT_GT(canvas->output_size(), image1->output_size());
  EXPECT_GT(canvas->output_size(), image2->output_size());
  EXPECT_GT(image1->input_size() + image2->input_size(),
            canvas->output_size());
}


const char kActualUrl[] = "http://encoded.url/with/various.stuff";

TEST_F(ImageTest, NoDims) {
  const char kNoDimsUrl[] = "x,hencoded.url,_with,_various.stuff";
  std::string origin_url;
  ImageUrlEncoder encoder;
  ImageDim dim;
  EXPECT_TRUE(encoder.DecodeUrlAndDimensions(kNoDimsUrl, &dim, &origin_url));
  EXPECT_FALSE(dim.valid());
  EXPECT_EQ(kActualUrl, origin_url);
  EXPECT_EQ(kNoDimsUrl, EncodeUrlAndDimensions(origin_url, dim));
}

TEST_F(ImageTest, HasDims) {
  const char kDimsUrl[] = "17x33x,hencoded.url,_with,_various.stuff";
  std::string origin_url;
  ImageUrlEncoder encoder;
  ImageDim dim;
  EXPECT_TRUE(encoder.DecodeUrlAndDimensions(kDimsUrl, &dim, &origin_url));
  EXPECT_TRUE(dim.valid());
  EXPECT_EQ(17, dim.width());
  EXPECT_EQ(33, dim.height());
  EXPECT_EQ(kActualUrl, origin_url);
  EXPECT_EQ(kDimsUrl, EncodeUrlAndDimensions(origin_url, dim));
}

TEST(ImageUrlTest, BadFirst) {
  const char kBadFirst[] = "badx33x,hencoded.url,_with,_various.stuff";
  std::string origin_url;
  ImageUrlEncoder encoder;
  ImageDim dim;
  EXPECT_FALSE(encoder.DecodeUrlAndDimensions(kBadFirst, &dim, &origin_url));
  EXPECT_FALSE(dim.valid());
}

TEST(ImageUrlTest, BadSecond) {
  const char kBadSecond[] = "17xbadx,hencoded.url,_with,_various.stuff";
  std::string origin_url;
  ImageUrlEncoder encoder;
  ImageDim dim;
  EXPECT_FALSE(encoder.DecodeUrlAndDimensions(kBadSecond, &dim, &origin_url));
  EXPECT_FALSE(dim.valid());
}

TEST(ImageUrlTest, NoXs) {
  const char kNoXs[] = ",hencoded.url,_with,_various.stuff";
  std::string origin_url;
  ImageUrlEncoder encoder;
  ImageDim dim;
  EXPECT_FALSE(encoder.DecodeUrlAndDimensions(kNoXs, &dim, &origin_url));
  EXPECT_FALSE(dim.valid());
}

TEST(ImageUrlTest, BlankSecond) {
  const char kBlankSecond[] = "17xx,hencoded.url,_with,_various.stuff";
  std::string origin_url;
  ImageUrlEncoder encoder;
  ImageDim dim;
  EXPECT_FALSE(encoder.DecodeUrlAndDimensions(kBlankSecond, &dim, &origin_url));
  EXPECT_FALSE(dim.valid());
}

}  // namespace net_instaweb
