// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: sligocki@google.com (Shawn Ligocki)

#include "pagespeed/kernel/base/source_map.h"

#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {

namespace {

class SourceMapTest : public ::testing::Test {
 protected:
};

TEST_F(SourceMapTest, EncodeBase64) {
  EXPECT_EQ('A', source_map::EncodeBase64(0));
  EXPECT_EQ('B', source_map::EncodeBase64(1));
  EXPECT_EQ('Z', source_map::EncodeBase64(25));
  EXPECT_EQ('a', source_map::EncodeBase64(26));
  EXPECT_EQ('z', source_map::EncodeBase64(51));
  EXPECT_EQ('0', source_map::EncodeBase64(52));
  EXPECT_EQ('9', source_map::EncodeBase64(61));
  EXPECT_EQ('+', source_map::EncodeBase64(62));
  EXPECT_EQ('/', source_map::EncodeBase64(63));

  // Error cases
  EXPECT_DEBUG_DEATH(source_map::EncodeBase64(-1), "EncodeBase64");
  EXPECT_DEBUG_DEATH(source_map::EncodeBase64(64), "EncodeBase64");
  EXPECT_DEBUG_DEATH(source_map::EncodeBase64(100), "EncodeBase64");
  EXPECT_DEBUG_DEATH(source_map::EncodeBase64(-12345), "EncodeBase64");
  EXPECT_DEBUG_DEATH(source_map::EncodeBase64(54321), "EncodeBase64");
}

TEST_F(SourceMapTest, EncodeVlq) {
  EXPECT_EQ("A", source_map::EncodeVlq(0));   // 000000  ("A" in base64 binary)
  // Note: Nothing encodes to "B" (-0). AFAICT, it would be OK if 0 did, though.
  EXPECT_EQ("C", source_map::EncodeVlq(1));   // 000010
  EXPECT_EQ("D", source_map::EncodeVlq(-1));  // 000011
  EXPECT_EQ("E", source_map::EncodeVlq(2));   // 000100
  EXPECT_EQ("F", source_map::EncodeVlq(-2));  // 000101
  EXPECT_EQ("G", source_map::EncodeVlq(3));   // 000110
  EXPECT_EQ("H", source_map::EncodeVlq(-3));  // 000111

  EXPECT_EQ(  "R", source_map::EncodeVlq(-8));      // 010001
  EXPECT_EQ(  "a", source_map::EncodeVlq(13));      // 011010
  EXPECT_EQ( "0C", source_map::EncodeVlq(42));      // 110100 000010
  EXPECT_EQ( "0I", source_map::EncodeVlq(138));     // 110100 001000
  EXPECT_EQ( "9d", source_map::EncodeVlq(-478));    // 111101 011101
  EXPECT_EQ("7yB", source_map::EncodeVlq(-813));    // 111011 110010 000001
  EXPECT_EQ("+/X", source_map::EncodeVlq(12287));   // 111110 111111 010111
  EXPECT_EQ("jib", source_map::EncodeVlq(-13857));  // 100011 100010 011011
  // 100000 101000 101101 011110
  EXPECT_EQ("gote", source_map::EncodeVlq(498304));
}

TEST_F(SourceMapTest, VlqExtremeVals) {
  // Test extreme values.
  // From https://docs.google.com/document/d/1U1RGAehQwRypUTovF1KRlpiOFze0b-_2gc6fAH0KY0k/edit:
  // Note: The values that can be represent by the VLQ Base64 encoded are
  // limited to 32 bit quantities until some use case for larger values is
  // presented.

  // 111110 111111 111111 111111 111111 111111 000011
  EXPECT_EQ("+/////D", source_map::EncodeVlq(kint32max));
  // 100001 100000 100000 100000 100000 100000 000100
  EXPECT_EQ("hgggggE", source_map::EncodeVlq(kint32min));
}

TEST_F(SourceMapTest, EncodeMappings) {
  source_map::MappingVector mappings;
  mappings.push_back(source_map::Mapping(1,   1,  0,  10,   0));
  mappings.push_back(source_map::Mapping(1,  21,  1,  11,   0));
  mappings.push_back(source_map::Mapping(1,  25,  1,  11,  81));
  mappings.push_back(source_map::Mapping(2,  13,  2,  11, 105));
  mappings.push_back(source_map::Mapping(5,   8, 13, 132,   7));
  mappings.push_back(source_map::Mapping(5, 472,  0, 436,  13));

  GoogleString result;
  EXPECT_TRUE(source_map::EncodeMappings(mappings, &result));
  EXPECT_EQ(";CAUA,oBCCA,IAAiF;aCAwB;;;QWyHlG,gdbgTM", result);
}

// Trivial example, to make sure code works at all.
TEST_F(SourceMapTest, Encode_Simple) {
  source_map::MappingVector mappings;
  GoogleString result;
  EXPECT_TRUE(source_map::Encode("http://example.com/generated.js",
                                 "http://example.com/original.js",
                                 mappings,
                                 &result));
  // Note: Exact order and amount of whitespace is not important and this may
  // need to be re-golded if the Json::FastWriter changes.
  EXPECT_EQ(")]}'\n"
            "{\"file\":\"http://example.com/generated.js\",\"mappings\":\"\","
            "\"names\":[],\"sources\":[\"http://example.com/original.js\"],"
            "\"version\":3}\n", result);
}

TEST_F(SourceMapTest, Encode) {
  source_map::MappingVector mappings;
  mappings.push_back(source_map::Mapping(0,  0, 0, 4,  0));
  mappings.push_back(source_map::Mapping(0, 21, 0, 4, 22));
  mappings.push_back(source_map::Mapping(0, 22, 0, 5,  2));
  mappings.push_back(source_map::Mapping(0, 44, 0, 6,  0));

  GoogleString result;
  EXPECT_TRUE(source_map::Encode("http://example.com/generated.js",
                                 "http://example.com/original.js",
                                 mappings,
                                 &result));
  // Note: Exact order and amount of whitespace is not important and this may
  // need to be re-golded if the Json::FastWriter changes.
  EXPECT_EQ(")]}'\n"
            "{\"file\":\"http://example.com/generated.js\","
            "\"mappings\":\"AAIA,qBAAsB,CACpB,sBACF\","
            "\"names\":[],\"sources\":[\"http://example.com/original.js\"],"
            "\"version\":3}\n", result);
}

// Make sure chars are escaped correctly in JSON string.
TEST_F(SourceMapTest, Encode_JsonEscaping) {
  source_map::MappingVector mappings;
  GoogleString result;
  EXPECT_TRUE(source_map::Encode("`~!@#$%^&*()-_=+[{]}\\|;:'\",<.>/?",
                                 "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b"
                                 "\x0c\x0d\x10\x13\x1f\x20\x7e\x7f",
                                 mappings,
                                 &result));
  // Note: Exact order and amount of whitespace is not important and this may
  // need to be re-golded if the Json::FastWriter changes.
  EXPECT_EQ(")]}'\n"
            // " and \ are backslash escaped (' isn't).
            "{\"file\":\"`~!@#$%^&*()-_=+[{]}\\\\|;:'\\\",%3C.%3E/?\","
            "\"mappings\":\"\",\"names\":[],\"sources\":[\""
            // Control chars U+00 to U+1F must be escaped as \uXXXX
            // Except for a few special cases: \b \f \n \r \t
            // U+7F is not considered a control char, not escaped.
            "\\u0001\\u0002\\u0003\\u0004\\u0005\\u0006\\u0007\\b\\t\\n"
            "\\u000B\\f\\r\\u0010\\u0013\\u001F ~\x7F\"],"
            "\"version\":3}\n", result);
}

TEST_F(SourceMapTest, Encode_Fail) {
  source_map::MappingVector mappings;
  mappings.push_back(source_map::Mapping(1, 0, 0, 0, 0));
  // Invalid: mappings must be sorted.
  mappings.push_back(source_map::Mapping(0, 0, 0, 0, 0));

  GoogleString result;
  EXPECT_DEBUG_DEATH({
      EXPECT_FALSE(source_map::Encode("http://example.com/generated.js",
                                      "http://example.com/original.js",
                                      mappings,
                                      &result));
    }, "Mappings are not sorted");
}

}  // namespace

}  // namespace net_instaweb
