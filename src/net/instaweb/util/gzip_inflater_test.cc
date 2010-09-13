// Copyright 2010 Google Inc.
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

#include <string>
#include "net/instaweb/util/public/gzip_inflater.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net_instaweb {

namespace {

const char kBasic[] = "Hello\n";

// The above string "Hello\n", gzip compressed.
const unsigned char kCompressed[] = {
  0x1f, 0x8b, 0x08, 0x08, 0x38, 0x18, 0x2e, 0x4c, 0x00, 0x03, 0x63,
  0x6f, 0x6d, 0x70, 0x72, 0x65, 0x73, 0x73, 0x65, 0x64, 0x2e, 0x68,
  0x74, 0x6d, 0x6c, 0x00, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xe7, 0x02,
  0x00, 0x16, 0x35, 0x96, 0x31, 0x06, 0x00, 0x00, 0x00
};

const size_t kBufSize = 256;

TEST(GzipInflaterTest, Simple) {
  std::string buf;
  buf.reserve(kBufSize);
  GzipInflater inflater;
  inflater.Init();
  ASSERT_FALSE(inflater.HasUnconsumedInput());
  ASSERT_TRUE(inflater.SetInput(kCompressed, sizeof(kCompressed)));
  ASSERT_TRUE(inflater.HasUnconsumedInput());
  int num_inflated_bytes = inflater.InflateBytes(&buf[0], kBufSize);
  ASSERT_EQ(strlen(kBasic), static_cast<size_t>(num_inflated_bytes));
  // null-terminate the buffer
  buf[num_inflated_bytes] = '\0';
  ASSERT_FALSE(inflater.HasUnconsumedInput());
  ASSERT_TRUE(inflater.finished());
  ASSERT_FALSE(inflater.error());
  inflater.ShutDown();
  ASSERT_STREQ(kBasic, buf.c_str());
}

TEST(GzipInflaterTest, OneByteAtATime) {
  std::string buf;
  buf.reserve(kBufSize);
  GzipInflater inflater;
  inflater.Init();
  int num_inflated_bytes = 0;
  ASSERT_FALSE(inflater.HasUnconsumedInput());
  for (size_t input_offset = 0;
       input_offset < sizeof(kCompressed);
       ++input_offset) {
    ASSERT_TRUE(inflater.SetInput(&kCompressed[input_offset], 1));
    ASSERT_TRUE(inflater.HasUnconsumedInput());
    num_inflated_bytes +=
        inflater.InflateBytes(&buf[num_inflated_bytes],
                              kBufSize - num_inflated_bytes);
  }
  ASSERT_EQ(strlen(kBasic), static_cast<size_t>(num_inflated_bytes));
  // null-terminate the buffer
  buf[num_inflated_bytes] = '\0';
  ASSERT_FALSE(inflater.HasUnconsumedInput());
  ASSERT_TRUE(inflater.finished());
  ASSERT_FALSE(inflater.error());
  inflater.ShutDown();
  ASSERT_STREQ(kBasic, buf.c_str());
}

}  // namespace

}  // namespace net_instaweb
