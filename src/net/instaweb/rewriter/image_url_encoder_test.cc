/*
 * Copyright 2012 Google Inc.
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
// Author: poojatandon@google.com (Pooja Verlani)

// Unit test for image_url_encoder.

#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/image_url_encoder.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "testing/base/public/gunit.h"

namespace net_instaweb {
namespace {

const char kDimsUrl[] = "17x33x,hencoded.url,_with,_various.stuff";
const char kNoDimsUrl[] = "x,hencoded.url,_with,_various.stuff";
const char kActualUrl[] = "http://encoded.url/with/various.stuff";

}  // namespace

class ImageUrlEncoderTest : public ::testing::Test {
 protected:
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

  ImageUrlEncoder encoder_;
  GoogleMessageHandler handler_;
};

TEST_F(ImageUrlEncoderTest, TestEncodingAndDecoding) {
  GoogleString kOriginalUrl = "a.jpg";
  StringVector url_vector;
  url_vector.push_back(kOriginalUrl);

  GoogleString encoded_url;

  ResourceContext context;
  context.set_libwebp_level(ResourceContext::LIBWEBP_LOSSY_ONLY);
  context.set_mobile_user_agent(true);

  ImageDim dim;
  dim.set_width(1024);
  dim.set_height(768);
  *context.mutable_desired_image_dims() = dim;

  encoder_.Encode(url_vector, &context, &encoded_url);
  EXPECT_EQ("1024x768xa.jpg", encoded_url);

  encoder_.Decode(encoded_url, &url_vector, &context, &handler_);

  // Check the resource context returned.
  EXPECT_EQ(context.libwebp_level(), ResourceContext::LIBWEBP_LOSSY_ONLY);
  EXPECT_TRUE(context.mobile_user_agent());

  // Check the decoded url after encoding is the same as original.
  GoogleString decoded_url = url_vector.back();
  EXPECT_EQ(kOriginalUrl, decoded_url);
}

TEST_F(ImageUrlEncoderTest, TestEncodingAndDecodingWithoutWebpAndMobileUA) {
  GoogleString kOriginalUrl = "a.jpg";
  StringVector url_vector;
  url_vector.push_back(kOriginalUrl);

  GoogleString encoded_url;

  ResourceContext context;
  context.set_libwebp_level(ResourceContext::LIBWEBP_NONE);
  context.set_mobile_user_agent(false);

  ImageDim dim;
  dim.set_width(1024);
  dim.set_height(768);
  *context.mutable_desired_image_dims() = dim;

  encoder_.Encode(url_vector, &context, &encoded_url);
  EXPECT_EQ("1024x768xa.jpg", encoded_url);

  encoder_.Decode(encoded_url, &url_vector, &context, &handler_);

  // Check the resource context returned.
  EXPECT_EQ(context.libwebp_level(), ResourceContext::LIBWEBP_NONE);
  EXPECT_FALSE(context.mobile_user_agent());

  // Check the decoded url after encoding is the same as original.
  GoogleString decoded_url = url_vector.back();
  EXPECT_EQ(kOriginalUrl, decoded_url);
}

TEST_F(ImageUrlEncoderTest, TestLegacyMobileWebpDecoding) {
  StringPiece kEncodedUrl = "1024x768mwa.jpg";
  StringVector url_vector;
  ResourceContext context;

  context.set_libwebp_level(ResourceContext::LIBWEBP_LOSSY_ONLY);
  encoder_.Decode(kEncodedUrl, &url_vector, &context, &handler_);
  EXPECT_EQ(context.libwebp_level(), ResourceContext::LIBWEBP_LOSSY_ONLY);
  EXPECT_TRUE(context.mobile_user_agent());

  GoogleString decoded_url = url_vector.back();
  EXPECT_EQ("a.jpg", decoded_url);
}

TEST_F(ImageUrlEncoderTest, TestLegacyMobileDecoding) {
  StringPiece kEncodedUrl = "1024x768mxa.jpg";
  StringVector url_vector;
  ResourceContext context;

  // Set webp lossy similar to the ImageRewriteFilter flow.
  context.set_libwebp_level(ResourceContext::LIBWEBP_LOSSY_ONLY);
  encoder_.Decode(kEncodedUrl, &url_vector, &context, &handler_);
  // While decoding in ImageUrlEncoder, we don't unset libwep_level if
  // already set and no encoding char("w"/"v") is found, we get a false
  // ResourceContext, but its alright since the UA is a webp-capable one.

  EXPECT_EQ(context.libwebp_level(), ResourceContext::LIBWEBP_LOSSY_ONLY);
  EXPECT_TRUE(context.mobile_user_agent());

  GoogleString decoded_url = url_vector.back();
  EXPECT_EQ("a.jpg", decoded_url);
}

TEST_F(ImageUrlEncoderTest, NoDimsWebpOrMobile) {
  GoogleString origin_url;
  ImageDim dim;
  EXPECT_TRUE(DecodeUrlAndDimensions(kNoDimsUrl, &dim, &origin_url));
  EXPECT_FALSE(ImageUrlEncoder::HasValidDimensions(dim));
  EXPECT_EQ(kActualUrl, origin_url);
  EXPECT_EQ(kNoDimsUrl, EncodeUrlAndDimensions(origin_url, dim));
}

TEST_F(ImageUrlEncoderTest, NoDimsWebpLa) {
  const char kLegacyWebpLaNoMobileUrl[] = "v,hencoded.url,_with,_various.stuff";
  GoogleString origin_url;
  ImageDim dim;
  EXPECT_TRUE(DecodeUrlAndDimensions(kLegacyWebpLaNoMobileUrl,
                                     &dim, &origin_url));
  EXPECT_FALSE(ImageUrlEncoder::HasValidDimensions(dim));
  EXPECT_EQ(kActualUrl, origin_url);
  EXPECT_EQ(kNoDimsUrl, EncodeUrlAndDimensions(origin_url, dim));
}

TEST_F(ImageUrlEncoderTest, NoDimsWebpLaMobile) {
  const char kLegacyWebpLaMobileUrl[] = "mv,hencoded.url,_with,_various.stuff";
  GoogleString origin_url;
  ImageDim dim;
  EXPECT_TRUE(DecodeUrlAndDimensions(
      kLegacyWebpLaMobileUrl, &dim, &origin_url));
  EXPECT_FALSE(ImageUrlEncoder::HasValidDimensions(dim));
  EXPECT_EQ(kActualUrl, origin_url);
  EXPECT_EQ(kNoDimsUrl, EncodeUrlAndDimensions(origin_url, dim));
}

TEST_F(ImageUrlEncoderTest, HasDims) {
  GoogleString origin_url;
  ImageDim dim;
  EXPECT_TRUE(DecodeUrlAndDimensions(kDimsUrl, &dim, &origin_url));
  EXPECT_TRUE(ImageUrlEncoder::HasValidDimensions(dim));
  EXPECT_EQ(17, dim.width());
  EXPECT_EQ(33, dim.height());
  EXPECT_EQ(kActualUrl, origin_url);
  EXPECT_EQ(kDimsUrl, EncodeUrlAndDimensions(origin_url, dim));
}

TEST_F(ImageUrlEncoderTest, HasDimsWebp) {
  const char kLegacyWebpUrl[] = "17x33w,hencoded.url,_with,_various.stuff";
  GoogleString origin_url;
  ImageDim dim;
  EXPECT_TRUE(DecodeUrlAndDimensions(kLegacyWebpUrl, &dim, &origin_url));
  EXPECT_TRUE(ImageUrlEncoder::HasValidDimensions(dim));
  EXPECT_EQ(17, dim.width());
  EXPECT_EQ(33, dim.height());
  EXPECT_EQ(kActualUrl, origin_url);
  EXPECT_EQ(kDimsUrl, EncodeUrlAndDimensions(origin_url, dim));
}

TEST_F(ImageUrlEncoderTest, HasDimsWebpLa) {
  const char kLegacyWebpLaNoMobileUrl[] =
      "17x33v,hencoded.url,_with,_various.stuff";
  GoogleString origin_url;
  ImageDim dim;
  EXPECT_TRUE(DecodeUrlAndDimensions(
      kLegacyWebpLaNoMobileUrl, &dim, &origin_url));
  EXPECT_TRUE(ImageUrlEncoder::HasValidDimensions(dim));
  EXPECT_EQ(17, dim.width());
  EXPECT_EQ(33, dim.height());
  EXPECT_EQ(kActualUrl, origin_url);
  EXPECT_EQ(kDimsUrl, EncodeUrlAndDimensions(origin_url, dim));
}

TEST_F(ImageUrlEncoderTest, HasDimsMobile) {
  const char kLegacyMobileUrl[] = "17x33mx,hencoded.url,_with,_various.stuff";
  GoogleString origin_url;
  ImageDim dim;
  EXPECT_TRUE(DecodeUrlAndDimensions(kLegacyMobileUrl, &dim, &origin_url));
  EXPECT_TRUE(ImageUrlEncoder::HasValidDimensions(dim));
  EXPECT_EQ(17, dim.width());
  EXPECT_EQ(33, dim.height());
  EXPECT_EQ(kActualUrl, origin_url);
  EXPECT_EQ(kDimsUrl, EncodeUrlAndDimensions(origin_url, dim));
}

TEST_F(ImageUrlEncoderTest, HasDimsWebpMobile) {
  const char kLegacyWebpMobileUrl[] =
      "17x33mw,hencoded.url,_with,_various.stuff";
  GoogleString origin_url;
  ImageDim dim;
  EXPECT_TRUE(DecodeUrlAndDimensions(kLegacyWebpMobileUrl, &dim, &origin_url));
  EXPECT_TRUE(ImageUrlEncoder::HasValidDimensions(dim));
  EXPECT_EQ(17, dim.width());
  EXPECT_EQ(33, dim.height());
  EXPECT_EQ(kActualUrl, origin_url);
  EXPECT_EQ(kDimsUrl, EncodeUrlAndDimensions(origin_url, dim));
}

TEST_F(ImageUrlEncoderTest, HasDimsWebpLaMobile) {
  const char kLegacyWebpLaMobileUrl[] =
      "17x33mv,hencoded.url,_with,_various.stuff";
  GoogleString origin_url;
  ImageDim dim;
  EXPECT_TRUE(DecodeUrlAndDimensions(
      kLegacyWebpLaMobileUrl, &dim, &origin_url));
  EXPECT_TRUE(ImageUrlEncoder::HasValidDimensions(dim));
  EXPECT_EQ(17, dim.width());
  EXPECT_EQ(33, dim.height());
  EXPECT_EQ(kActualUrl, origin_url);
  EXPECT_EQ(kDimsUrl, EncodeUrlAndDimensions(origin_url, dim));
}

TEST_F(ImageUrlEncoderTest, HasWidth) {
  const char kWidthUrl[] = "17xNx,hencoded.url,_with,_various.stuff";
  GoogleString origin_url;
  ImageDim dim;
  EXPECT_TRUE(DecodeUrlAndDimensions(kWidthUrl, &dim, &origin_url));
  EXPECT_TRUE(ImageUrlEncoder::HasValidDimension(dim));
  EXPECT_EQ(17, dim.width());
  EXPECT_EQ(-1, dim.height());
  EXPECT_EQ(kActualUrl, origin_url);
  EXPECT_EQ(kWidthUrl, EncodeUrlAndDimensions(origin_url, dim));
}

TEST_F(ImageUrlEncoderTest, HasWidthWebp) {
  const char kLegacyWebpWidthUrl[] = "17xNw,hencoded.url,_with,_various.stuff";
  GoogleString origin_url;
  ImageDim dim;
  EXPECT_TRUE(DecodeUrlAndDimensions(kLegacyWebpWidthUrl, &dim, &origin_url));
  EXPECT_TRUE(ImageUrlEncoder::HasValidDimension(dim));
  EXPECT_EQ(17, dim.width());
  EXPECT_EQ(-1, dim.height());
  EXPECT_EQ(kActualUrl, origin_url);
  const char kWidthUrl[] = "17xNx,hencoded.url,_with,_various.stuff";
  EXPECT_EQ(kWidthUrl, EncodeUrlAndDimensions(origin_url, dim));
}

TEST_F(ImageUrlEncoderTest, HasWidthWebpLa) {
  const char kLegacyWebpLaNoMobileUrl[] =
      "17xNv,hencoded.url,_with,_various.stuff";
  GoogleString origin_url;
  ImageDim dim;
  EXPECT_TRUE(DecodeUrlAndDimensions(kLegacyWebpLaNoMobileUrl,
                                     &dim, &origin_url));
  EXPECT_TRUE(ImageUrlEncoder::HasValidDimension(dim));
  EXPECT_EQ(17, dim.width());
  EXPECT_EQ(-1, dim.height());
  EXPECT_EQ(kActualUrl, origin_url);
  const char kWidthUrl[] = "17xNx,hencoded.url,_with,_various.stuff";
  EXPECT_EQ(kWidthUrl, EncodeUrlAndDimensions(origin_url, dim));
}

TEST_F(ImageUrlEncoderTest, HasHeight) {
  const char kHeightUrl[] = "Nx33x,hencoded.url,_with,_various.stuff";
  GoogleString origin_url;
  ImageDim dim;
  EXPECT_TRUE(DecodeUrlAndDimensions(kHeightUrl, &dim, &origin_url));
  EXPECT_TRUE(ImageUrlEncoder::HasValidDimension(dim));
  EXPECT_EQ(-1, dim.width());
  EXPECT_EQ(33, dim.height());
  EXPECT_EQ(kActualUrl, origin_url);
  EXPECT_EQ(kHeightUrl, EncodeUrlAndDimensions(origin_url, dim));
}

TEST_F(ImageUrlEncoderTest, HasHeightWebp) {
  const char kLegacyWebpHeightUrl[] = "Nx33w,hencoded.url,_with,_various.stuff";
  GoogleString origin_url;
  ImageDim dim;
  EXPECT_TRUE(DecodeUrlAndDimensions(kLegacyWebpHeightUrl, &dim, &origin_url));
  EXPECT_TRUE(ImageUrlEncoder::HasValidDimension(dim));
  EXPECT_EQ(-1, dim.width());
  EXPECT_EQ(33, dim.height());
  EXPECT_EQ(kActualUrl, origin_url);
  const char kHeightUrl[] = "Nx33x,hencoded.url,_with,_various.stuff";
  EXPECT_EQ(kHeightUrl, EncodeUrlAndDimensions(origin_url, dim));
}

TEST_F(ImageUrlEncoderTest, HasHeightWebpLa) {
  const char kLegacyWebpLaNoMobileUrl[] =
      "Nx33v,hencoded.url,_with,_various.stuff";
  GoogleString origin_url;
  ImageDim dim;
  EXPECT_TRUE(DecodeUrlAndDimensions(kLegacyWebpLaNoMobileUrl,
                                     &dim, &origin_url));
  EXPECT_TRUE(ImageUrlEncoder::HasValidDimension(dim));
  EXPECT_EQ(-1, dim.width());
  EXPECT_EQ(33, dim.height());
  EXPECT_EQ(kActualUrl, origin_url);
  const char kHeightUrl[] = "Nx33x,hencoded.url,_with,_various.stuff";
  EXPECT_EQ(kHeightUrl,
            EncodeUrlAndDimensions(origin_url, dim));
}


TEST_F(ImageUrlEncoderTest, UserAgentScreenResolution) {
  const int screen_size = 100;
  int width = 0;
  int height = 0;
  EXPECT_TRUE(ImageUrlEncoder::GetNormalizedScreenResolution(
      screen_size, screen_size, &width, &height));
  EXPECT_GT(width, screen_size);

  ResourceContext context;
  ImageDim *dims = context.mutable_user_agent_screen_resolution();
  dims->set_width(width);
  dims->set_height(height);

  GoogleString cache_key =
      ImageUrlEncoder::CacheKeyFromResourceContext(context);
  GoogleString expected_key;
  StrAppend(&expected_key, "sr", IntegerToString(width), "x",
            IntegerToString(height));
  EXPECT_EQ(expected_key, cache_key);
}

TEST_F(ImageUrlEncoderTest, SmallScreen) {
  ResourceContext context;
  context.set_use_small_screen_quality(true);
  GoogleString cache_key =
      ImageUrlEncoder::CacheKeyFromResourceContext(context);
  EXPECT_EQ("ss", cache_key);
}

TEST_F(ImageUrlEncoderTest, BadFirst) {
  const char kBadFirst[] = "badx33x,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBadFirst);
}

TEST_F(ImageUrlEncoderTest, BadFirstWebp) {
  const char kBadFirst[] = "badx33w,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBadFirst);
}

TEST_F(ImageUrlEncoderTest, BadFirstWebpLa) {
  const char kBadFirst[] = "badx33v,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBadFirst);
}

TEST_F(ImageUrlEncoderTest, BadFirstMobile) {
  const char kBadFirst[] = "badx33mx,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBadFirst);
}

TEST_F(ImageUrlEncoderTest, BadFirstWebpMobile) {
  const char kBadFirst[] = "badx33mw,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBadFirst);
}

TEST_F(ImageUrlEncoderTest, BadFirstWebpLaMobile) {
  const char kBadFirst[] = "badx33mv,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBadFirst);
}

TEST_F(ImageUrlEncoderTest, BadSecond) {
  const char kBadSecond[] = "17xbadx,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBadSecond);
}

TEST_F(ImageUrlEncoderTest, BadSecondWebp) {
  const char kBadSecond[] = "17xbadw,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBadSecond);
}

TEST_F(ImageUrlEncoderTest, BadSecondWebpLa) {
  const char kBadSecond[] = "17xbadv,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBadSecond);
}

TEST_F(ImageUrlEncoderTest, BadSecondMobile) {
  const char kBadSecond[] = "17xbadmx,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBadSecond);
}

TEST_F(ImageUrlEncoderTest, BadSecondWebpMobile) {
  const char kBadSecond[] = "17xbadmw,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBadSecond);
}

TEST_F(ImageUrlEncoderTest, BadSecondWebpLaMobile) {
  const char kBadSecond[] = "17xbadmv,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBadSecond);
}

TEST_F(ImageUrlEncoderTest, BadLeadingN) {
  const char kBadLeadingN[] = "Nxw,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBadLeadingN);
}

TEST_F(ImageUrlEncoderTest, BadMiddleN) {
  const char kBadMiddleN[] = "17xN,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBadMiddleN);
}

TEST_F(ImageUrlEncoderTest, NoXs) {
  const char kNoXs[] = ",hencoded.url,_with,_various.stuff";
  ExpectBadDim(kNoXs);
}

TEST_F(ImageUrlEncoderTest, NoXsMobile) {
  const char kNoXs[] = "m,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kNoXs);
}

TEST_F(ImageUrlEncoderTest, BlankSecond) {
  const char kBlankSecond[] = "17xx,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBlankSecond);
}

TEST_F(ImageUrlEncoderTest, BadSizeCheck) {
  // Catch case where url size check was inverted.
  const char kBadSize[] = "17xx";
  ExpectBadDim(kBadSize);
}

TEST_F(ImageUrlEncoderTest, BlankSecondWebp) {
  const char kBlankSecond[] = "17xw,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBlankSecond);
}

TEST_F(ImageUrlEncoderTest, BlankSecondMobile) {
  const char kBlankSecond[] = "17xmx,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBlankSecond);
}

TEST_F(ImageUrlEncoderTest, BlankSecondWebpMobile) {
  const char kBlankSecond[] = "17xmw,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBlankSecond);
}

TEST_F(ImageUrlEncoderTest, BlankSecondWebpLaMobile) {
  const char kBlankSecond[] = "17xmv,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBlankSecond);
}

TEST_F(ImageUrlEncoderTest, BadTrailChar) {
  const char kBadDimsUrl[] = "17x33u,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBadDimsUrl);
}

TEST_F(ImageUrlEncoderTest, BadInitChar) {
  const char kBadNoDimsUrl[] = "u,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kBadNoDimsUrl);
}

TEST_F(ImageUrlEncoderTest, BadWidthChar) {
  const char kWidthUrl[] = "17t,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kWidthUrl);
}

TEST_F(ImageUrlEncoderTest, BadHeightChar) {
  const char kHeightUrl[] = "Nx33t,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kHeightUrl);
}

TEST_F(ImageUrlEncoderTest, ShortBothDims) {
  const char kShortUrl[] = "17x33";
  ExpectBadDim(kShortUrl);
}

TEST_F(ImageUrlEncoderTest, ShortWidth) {
  const char kShortWidth[] = "Nx33";
  ExpectBadDim(kShortWidth);
}

TEST_F(ImageUrlEncoderTest, ShortHeight) {
  const char kShortHeight[] = "17xN";
  ExpectBadDim(kShortHeight);
}

TEST_F(ImageUrlEncoderTest, BothDimsMissing) {
  const char kNeitherUrl[] = "NxNx,hencoded.url,_with,_various.stuff";
  ExpectBadDim(kNeitherUrl);
}

TEST_F(ImageUrlEncoderTest, VeryShortUrl) {
  const char kVeryShortUrl[] = "7x3";
  ExpectBadDim(kVeryShortUrl);
}

TEST_F(ImageUrlEncoderTest, TruncatedAfterFirstDim) {
  const char kTruncatedUrl[] = "175x";
  ExpectBadDim(kTruncatedUrl);
}

TEST_F(ImageUrlEncoderTest, TruncatedBeforeSep) {
  const char kTruncatedUrl[] = "12500";
  ExpectBadDim(kTruncatedUrl);
}

}  // namespace net_instaweb
