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
// Author: poojatandon@google.com (Pooja Verlani)

// Unit test for css_url_encoder.

#include "net/instaweb/rewriter/cached_result.pb.h"
#include "testing/base/public/gunit.h"
#include "net/instaweb/rewriter/public/css_url_encoder.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/util/public/scoped_ptr.h"            // for scoped_ptr


namespace net_instaweb {
namespace {

class CssUrlEncoderTest : public ::testing::Test {
 protected:
  scoped_ptr<RequestProperties> request_properties;
  CssUrlEncoder encoder_;
  GoogleMessageHandler handler_;
};

TEST_F(CssUrlEncoderTest, TestEncodingAndDecoding) {
  GoogleString kOriginalUrl = "a.css";
  StringVector url_vector;
  url_vector.push_back(kOriginalUrl);

  GoogleString encoded_url;

  ResourceContext context;
  context.set_libwebp_level(ResourceContext::LIBWEBP_LOSSY_ONLY);
  context.set_inline_images(true);

  encoder_.Encode(url_vector, &context, &encoded_url);
  EXPECT_EQ("A.a.css", encoded_url);

  encoder_.Decode(encoded_url, &url_vector, &context, &handler_);

  EXPECT_EQ(context.libwebp_level(), ResourceContext::LIBWEBP_LOSSY_ONLY);
  EXPECT_TRUE(context.inline_images());

  GoogleString decoded_url = url_vector.back();
  EXPECT_EQ(kOriginalUrl, decoded_url);
}

TEST_F(CssUrlEncoderTest, TestEncodingAndDecodingWithoutWebpAndInlineImagesUA) {
  GoogleString kOriginalUrl = "a.css";
  StringVector url_vector;
  url_vector.push_back(kOriginalUrl);

  GoogleString encoded_url;

  ResourceContext context;
  context.set_libwebp_level(ResourceContext::LIBWEBP_NONE);
  context.set_inline_images(false);

  encoder_.Encode(url_vector, &context, &encoded_url);
  EXPECT_EQ("A.a.css", encoded_url);

  encoder_.Decode(encoded_url, &url_vector, &context, &handler_);

  // Check the resource context returned.
  EXPECT_EQ(context.libwebp_level(), ResourceContext::LIBWEBP_NONE);
  EXPECT_FALSE(context.inline_images());

  // Check the decoded url after encoding is the same as original.
  GoogleString decoded_url = url_vector.back();
  EXPECT_EQ(kOriginalUrl, decoded_url);
}

TEST_F(CssUrlEncoderTest, TestLegacyInlineWebpLossyOnlyDecoding) {
  StringPiece kEncodedUrl = "W.a.css";
  StringVector url_vector;
  ResourceContext context;

  context.set_libwebp_level(ResourceContext::LIBWEBP_LOSSY_ONLY);
  context.set_inline_images(true);
  encoder_.Decode(kEncodedUrl, &url_vector, &context, &handler_);

  EXPECT_EQ(context.libwebp_level(), ResourceContext::LIBWEBP_LOSSY_ONLY);
  EXPECT_TRUE(context.inline_images());

  GoogleString decoded_url = url_vector.back();
  EXPECT_EQ("a.css", decoded_url);
}

TEST_F(CssUrlEncoderTest, TestLegacyInlineWebpLossyLosslessAlphaDecoding) {
  StringPiece kEncodedUrl = "V.a.css";
  StringVector url_vector;
  ResourceContext context;

  context.set_libwebp_level(ResourceContext::LIBWEBP_LOSSY_LOSSLESS_ALPHA);
  context.set_inline_images(true);

  encoder_.Decode(kEncodedUrl, &url_vector, &context, &handler_);

  EXPECT_EQ(context.libwebp_level(),
            ResourceContext::LIBWEBP_LOSSY_LOSSLESS_ALPHA);
  EXPECT_TRUE(context.inline_images());

  GoogleString decoded_url = url_vector.back();
  EXPECT_EQ("a.css", decoded_url);
}

TEST_F(CssUrlEncoderTest, TestSetInliningImages) {
  GoogleString user_agent_string = "Chrome/";
  ResourceContext resource_context;
  UserAgentMatcher user_agent_matcher;
  request_properties.reset(new RequestProperties(&user_agent_matcher));
  request_properties->SetUserAgent(user_agent_string);

  encoder_.SetInliningImages(*request_properties, &resource_context);

  EXPECT_TRUE(resource_context.inline_images());

  user_agent_string = "MSIE 6.0";  // An older UA to check inlining is not set.
  request_properties.reset(new RequestProperties(&user_agent_matcher));
  request_properties->SetUserAgent(user_agent_string);

  encoder_.SetInliningImages(*request_properties, &resource_context);

  EXPECT_FALSE(resource_context.inline_images());
}

}  // namespace
}  // namespace net_instaweb
