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

// Author: bmcquade@google.com (Bryan McQuade)

#include <cstddef>
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/gzip_inflater.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class GzipInflaterTestPeer {
 public:
  static void SetInputBypassFirstByteCheck(
      GzipInflater* inflater, const unsigned char* in, size_t in_size) {
    inflater->SetInputInternal(in, in_size);
  }

  static bool FormatIsZlibStream(const GzipInflater& inflater) {
    return inflater.format_ == GzipInflater::FORMAT_ZLIB_STREAM;
  }
};

namespace {

const char kBasic[] = "Hello\n";

// The above string "Hello\n", gzip compressed.
const unsigned char kCompressed[] = {
  0x1f, 0x8b, 0x08, 0x08, 0x38, 0x18, 0x2e, 0x4c, 0x00, 0x03, 0x63,
  0x6f, 0x6d, 0x70, 0x72, 0x65, 0x73, 0x73, 0x65, 0x64, 0x2e, 0x68,
  0x74, 0x6d, 0x6c, 0x00, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xe7, 0x02,
  0x00, 0x16, 0x35, 0x96, 0x31, 0x06, 0x00, 0x00, 0x00
};

// The above string "Hello\n", zlib stream compressed.
const unsigned char kCompressedZlibStream[] = {
  0x78, 0x9c, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xe7, 0x02, 0x00, 0x07,
  0x8b, 0x01, 0xff
};

// The above string "Hello\n", raw deflate compressed.
const unsigned char kCompressedRawDeflate[] = {
  0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xe7, 0x02, 0x00,
};

const size_t kBufSize = 256;

void AssertInflate(GzipInflater::InflateType type,
                   const unsigned char* in,
                   size_t in_size) {
  std::string buf;
  buf.reserve(kBufSize);
  GzipInflater inflater(type);
  inflater.Init();
  ASSERT_FALSE(inflater.HasUnconsumedInput());
  ASSERT_TRUE(inflater.SetInput(in, in_size));
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

void AssertInflateOneByteAtATime(GzipInflater::InflateType type,
                                 const unsigned char* in,
                                 size_t in_size) {
  std::string buf;
  buf.reserve(kBufSize);
  GzipInflater inflater(type);
  inflater.Init();
  int num_inflated_bytes = 0;
  ASSERT_FALSE(inflater.HasUnconsumedInput());
  for (size_t input_offset = 0;
       input_offset < in_size;
       ++input_offset) {
    ASSERT_TRUE(inflater.SetInput(in + input_offset, 1));
    ASSERT_TRUE(inflater.HasUnconsumedInput());
    num_inflated_bytes +=
        inflater.InflateBytes(&buf[num_inflated_bytes],
                              kBufSize - num_inflated_bytes);
    ASSERT_FALSE(inflater.error());
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

TEST(GzipInflaterTest, Gzip) {
  AssertInflate(GzipInflater::kGzip,
                kCompressed,
                sizeof(kCompressed));
}

TEST(GzipInflaterTest, GzipOneByteAtATime) {
  AssertInflateOneByteAtATime(GzipInflater::kGzip,
                              kCompressed,
                              sizeof(kCompressed));
}

TEST(GzipInflaterTest, ZlibStream) {
  AssertInflate(GzipInflater::kDeflate,
                kCompressedZlibStream,
                sizeof(kCompressedZlibStream));
}

TEST(GzipInflaterTest, ZlibStreamOneByteAtATime) {
  AssertInflateOneByteAtATime(GzipInflater::kDeflate,
                              kCompressedZlibStream,
                              sizeof(kCompressedZlibStream));
}

TEST(GzipInflaterTest, RawDeflate) {
  AssertInflate(GzipInflater::kDeflate,
                kCompressedRawDeflate,
                sizeof(kCompressedRawDeflate));
}

TEST(GzipInflaterTest, RawDeflateOneByteAtATime) {
  AssertInflateOneByteAtATime(GzipInflater::kDeflate,
                              kCompressedRawDeflate,
                              sizeof(kCompressedRawDeflate));
}

// We want to exercise the code path that detects a decompression
// failure inside InflateBytes and attempts to decode as zlib
// stream. However there is also a code path in SetInput() that
// inspects the input to see if the input is a valid zlib stream,
// which we must bypass in order to exercise this code path. It is
// possible for there to exist valid deflate streams that do have a
// valid zlib header byte, so we do need this code path as
// well. Unfortunately I am not able to produce such a deflate stream,
// which is why we need this special case flow with the
// GzipInflaterTest here.
TEST(GzipInflaterTest, RawDeflateBypassFirstByteCheck) {
  std::string buf;
  buf.reserve(kBufSize);
  GzipInflater inflater(GzipInflater::kDeflate);
  inflater.Init();
  ASSERT_FALSE(inflater.HasUnconsumedInput());
  // Normally, calling SetInput() will attempt to do stream type
  // detection on the first byte of input. We want to bypass that so
  // that we can exercise the failure path in InflateBytes that
  // attempts to fall back to raw deflate format.
  GzipInflaterTestPeer::SetInputBypassFirstByteCheck(
      &inflater, kCompressedRawDeflate, sizeof(kCompressedRawDeflate));
  ASSERT_TRUE(inflater.HasUnconsumedInput());
  // We expect the inflater to be in zlib stream format going into the
  // invocation of InflateBytes.
  ASSERT_TRUE(GzipInflaterTestPeer::FormatIsZlibStream(inflater));
  // InflateBytes should have detected that this was not a valid zlib
  // stream and switched the format to raw deflate.
  int num_inflated_bytes = inflater.InflateBytes(&buf[0], kBufSize);
  ASSERT_FALSE(GzipInflaterTestPeer::FormatIsZlibStream(inflater));
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
