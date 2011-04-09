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

// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test the base64 encoder.

#include "base/basictypes.h"
#include "base/logging.h"
#include "net/instaweb/util/public/base64_util.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace {

const char chinese_data[] = "中华网,中华,中国,中文网,中国新闻,香港新闻,"
    "国际新闻,中文新闻,新闻,港台新闻,两会,嫦娥一号";
const int chinese_size = STATIC_STRLEN(chinese_data);

// Also test some binary data, including embedded nulls, 2^7-1, 2^8-1
const char binary_data[] = "\0\1\2\3\4\5\6\7\10\0\177\176\175\377\376";
const int binary_size = STATIC_STRLEN(binary_data);

class Codec {
 public:
  virtual ~Codec() {}
  virtual void encode(const GoogleString& in, GoogleString* out) const = 0;
  virtual bool decode(const GoogleString& in, GoogleString* out) const = 0;
};

class WebSafeBase64Codec : public Codec {
 public:
  virtual void encode(const GoogleString& in, GoogleString* out) const {
    net_instaweb::Web64Encode(in, out);
  }
  virtual bool decode(const GoogleString& in, GoogleString* out) const {
    return net_instaweb::Web64Decode(in, out);
  }
};

class MimeBase64Codec : public Codec {
 public:
  virtual void encode(const GoogleString& in, GoogleString* out) const {
    net_instaweb::Mime64Encode(in, out);
  }
  virtual bool decode(const GoogleString& in, GoogleString* out) const {
    return net_instaweb::Mime64Decode(in, out);
  }
};

}  // namespace

namespace net_instaweb {

class Base64Test : public testing::Test {
 protected:
  Base64Test()
      : chinese_(chinese_data, chinese_size),
        binary_(binary_data, binary_size),
        web64_codec_(),
        mime64_codec_() {
  }

  void TestWeb64(const Codec &codec, const GoogleString& input) {
    GoogleString encoded, decoded;
    codec.encode(input, &encoded);
    ASSERT_TRUE(codec.decode(encoded, &decoded));
    EXPECT_EQ(input, decoded);
  }

  // Tests that attempts to decode a string that is not properly base64
  // encoded will gracefully fail (Web64Decode returns false) rather than
  // crash or produce invalid output.  corrupt_char must be a character
  // that is not in the base64 char-set.
  //
  // If the 'index' is specified as a negative number, it will be taken
  // as an offset from the end of the string.
  void TestCorrupt(const Codec &codec,
                   const GoogleString& input, char corrupt_char, int index) {
    GoogleString encoded, decoded;
    codec.encode(input, &encoded);
    if (index < 0) {
      index = encoded.size() + index;
    }
    encoded[index] = corrupt_char;
    ASSERT_FALSE(codec.decode(encoded, &decoded));
  }

  GoogleString chinese_;
  GoogleString binary_;
  WebSafeBase64Codec web64_codec_;
  MimeBase64Codec mime64_codec_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Base64Test);
};

TEST_F(Base64Test, Chinese) {
  TestWeb64(web64_codec_, chinese_);
  TestWeb64(mime64_codec_, chinese_);
}

TEST_F(Base64Test, Binary) {
  TestWeb64(web64_codec_, binary_);
  TestWeb64(mime64_codec_, binary_);
}

TEST_F(Base64Test, CorruptFirst) {
  TestCorrupt(web64_codec_, chinese_, '@', 0);
  TestCorrupt(mime64_codec_, chinese_, '@', 0);
}

TEST_F(Base64Test, CorruptMiddle) {
  TestCorrupt(web64_codec_, chinese_, ':', chinese_.size() / 2);
  TestCorrupt(mime64_codec_, chinese_, ':', chinese_.size() / 2);
}

TEST_F(Base64Test, CorruptEnd) {
  // I wanted to put the '/' as the last character, but it turns out
  // that encoders may put '=' characters in to pad to a multiple of
  // 4 bytes, and the decoder stops decoding when it gets to the first
  // pad character, so changing "==" to "=/" has no effect.
  TestCorrupt(web64_codec_, chinese_, '/', -4);
  TestCorrupt(mime64_codec_, chinese_, '_', -4);
}

}  // namespace net_instaweb
