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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/util/public/key_value_codec.h"

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class KeyValueCodecTest : public testing::Test {
 protected:
  void CodecTest(const StringPiece& key, const StringPiece& value) {
    SharedString val(value);
    ASSERT_TRUE(key_value_codec::Encode(key, &val, &key_value_));
    EXPECT_EQ(val.data(), key_value_.data()) << "shared storage";

    ASSERT_TRUE(key_value_codec::Decode(&key_value_, &decoded_key_,
                                        &decoded_value_));
    EXPECT_EQ(key, decoded_key_);
    EXPECT_EQ(value, decoded_value_.Value());
    EXPECT_EQ(val.data(), key_value_.data()) << "shared storage";
    EXPECT_EQ(decoded_value_.data(), key_value_.data()) << "shared storage";
  }

  SharedString key_value_;
  GoogleString decoded_key_;
  SharedString decoded_value_;
};

TEST_F(KeyValueCodecTest, SmallKey) {
  CodecTest("key", "value");
}

TEST_F(KeyValueCodecTest, TestLargeKey) {
  // This requires two bytes to represent the length of the key.
  CodecTest(GoogleString(10000, 'a'), "value");
}

TEST_F(KeyValueCodecTest, TestHugeKey) {
  // This key's length won't fit in two bytes, so the encoding will not work.
  SharedString val("value"), key_value;
  EXPECT_FALSE(key_value_codec::Encode(GoogleString(100000, 'a'), &val,
                                       &key_value));
}

TEST_F(KeyValueCodecTest, TestKey65536) {  // one byte too big.
  // This key's length won't fit in two bytes, so the encoding will not work.
  SharedString val("value"), key_value;
  EXPECT_FALSE(key_value_codec::Encode(GoogleString(65536, 'a'), &val,
                                       &key_value));
}

TEST_F(KeyValueCodecTest, TestKey0) {
  CodecTest("", "value");
}

TEST_F(KeyValueCodecTest, TestKey65534) {
  CodecTest(GoogleString(65534, 'a'), "value");
}

TEST_F(KeyValueCodecTest, TestKey65535) {
  CodecTest(GoogleString(65534, 'a'), "value");
}

TEST_F(KeyValueCodecTest, TestKeyHighBitsInTwoSizeBytes) {
  CodecTest(GoogleString(0x8080, 'a'), "value");
}

TEST_F(KeyValueCodecTest, DecodeEmptyKeyValue) {
  GoogleString decoded_key;
  SharedString decoded_value;
  EXPECT_FALSE(key_value_codec::Decode(&key_value_, &decoded_key,
                                       &decoded_value));
}

TEST_F(KeyValueCodecTest, CorruptKeyValue) {
  CodecTest("key", "value");
  char big = 0xff;
  key_value_.Append(&big, 1);
  key_value_.Append(&big, 1);
  EXPECT_FALSE(key_value_codec::Decode(&key_value_, &decoded_key_,
                                       &decoded_value_));
}

}  // namespace net_instaweb
