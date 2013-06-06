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
#include "net/instaweb/util/public/simple_random.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/stack_buffer.h"
#include "pagespeed/kernel/base/null_mutex.h"

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

class GzipInflaterTest : public testing::Test {
 protected:
  void TestInflateDeflate(StringPiece payload) {
    GoogleString deflated, inflated;
    StringWriter deflate_writer(&deflated);
    EXPECT_TRUE(GzipInflater::Deflate(payload, &deflate_writer));
    StringWriter inflate_writer(&inflated);
    EXPECT_TRUE(GzipInflater::Inflate(deflated, &inflate_writer));
    EXPECT_STREQ(payload, inflated);
  }
};

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
  buf.resize(kBufSize);
  GzipInflater inflater(type);
  inflater.Init();
  EXPECT_FALSE(inflater.HasUnconsumedInput());
  EXPECT_TRUE(inflater.SetInput(in, in_size));
  EXPECT_TRUE(inflater.HasUnconsumedInput());
  int num_inflated_bytes = inflater.InflateBytes(&buf[0], kBufSize);
  ASSERT_EQ(strlen(kBasic), static_cast<size_t>(num_inflated_bytes));
  // null-terminate the buffer
  buf[num_inflated_bytes] = '\0';
  EXPECT_FALSE(inflater.HasUnconsumedInput());
  EXPECT_TRUE(inflater.finished());
  EXPECT_FALSE(inflater.error());
  inflater.ShutDown();
  EXPECT_STREQ(kBasic, buf.c_str());
}

void AssertInflateOneByteAtATime(GzipInflater::InflateType type,
                                 const unsigned char* in,
                                 size_t in_size) {
  std::string buf;
  buf.resize(kBufSize);
  GzipInflater inflater(type);
  inflater.Init();
  int num_inflated_bytes = 0;
  EXPECT_FALSE(inflater.HasUnconsumedInput());
  for (size_t input_offset = 0;
       input_offset < in_size;
       ++input_offset) {
    EXPECT_TRUE(inflater.SetInput(in + input_offset, 1));
    EXPECT_TRUE(inflater.HasUnconsumedInput());
    num_inflated_bytes +=
        inflater.InflateBytes(&buf[num_inflated_bytes],
                              kBufSize - num_inflated_bytes);
    EXPECT_FALSE(inflater.error());
  }
  ASSERT_EQ(strlen(kBasic), static_cast<size_t>(num_inflated_bytes));
  // null-terminate the buffer
  buf[num_inflated_bytes] = '\0';
  EXPECT_FALSE(inflater.HasUnconsumedInput());
  EXPECT_TRUE(inflater.finished());
  EXPECT_FALSE(inflater.error());
  inflater.ShutDown();
  EXPECT_STREQ(kBasic, buf.c_str());
}

TEST_F(GzipInflaterTest, Gzip) {
  AssertInflate(GzipInflater::kGzip,
                kCompressed,
                sizeof(kCompressed));
}

TEST_F(GzipInflaterTest, GzipOneByteAtATime) {
  AssertInflateOneByteAtATime(GzipInflater::kGzip,
                              kCompressed,
                              sizeof(kCompressed));
}

TEST_F(GzipInflaterTest, ZlibStream) {
  AssertInflate(GzipInflater::kDeflate,
                kCompressedZlibStream,
                sizeof(kCompressedZlibStream));
}

TEST_F(GzipInflaterTest, ZlibStreamOneByteAtATime) {
  AssertInflateOneByteAtATime(GzipInflater::kDeflate,
                              kCompressedZlibStream,
                              sizeof(kCompressedZlibStream));
}

TEST_F(GzipInflaterTest, RawDeflate) {
  AssertInflate(GzipInflater::kDeflate,
                kCompressedRawDeflate,
                sizeof(kCompressedRawDeflate));
}

TEST_F(GzipInflaterTest, RawDeflateOneByteAtATime) {
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
TEST_F(GzipInflaterTest, RawDeflateBypassFirstByteCheck) {
  std::string buf;
  buf.resize(kBufSize);
  GzipInflater inflater(GzipInflater::kDeflate);
  inflater.Init();
  EXPECT_FALSE(inflater.HasUnconsumedInput());
  // Normally, calling SetInput() will attempt to do stream type
  // detection on the first byte of input. We want to bypass that so
  // that we can exercise the failure path in InflateBytes that
  // attempts to fall back to raw deflate format.
  GzipInflaterTestPeer::SetInputBypassFirstByteCheck(
      &inflater, kCompressedRawDeflate, sizeof(kCompressedRawDeflate));
  EXPECT_TRUE(inflater.HasUnconsumedInput());
  // We expect the inflater to be in zlib stream format going into the
  // invocation of InflateBytes.
  EXPECT_TRUE(GzipInflaterTestPeer::FormatIsZlibStream(inflater));
  // InflateBytes should have detected that this was not a valid zlib
  // stream and switched the format to raw deflate.
  int num_inflated_bytes = inflater.InflateBytes(&buf[0], kBufSize);
  EXPECT_FALSE(GzipInflaterTestPeer::FormatIsZlibStream(inflater));
  ASSERT_EQ(strlen(kBasic), static_cast<size_t>(num_inflated_bytes));
  // null-terminate the buffer
  buf[num_inflated_bytes] = '\0';
  EXPECT_FALSE(inflater.HasUnconsumedInput());
  EXPECT_TRUE(inflater.finished());
  EXPECT_FALSE(inflater.error());
  inflater.ShutDown();
  EXPECT_STREQ(kBasic, buf.c_str());  // buf.size() is 0 here, so we use c_str
}

TEST_F(GzipInflaterTest, InflateDeflate) {
  TestInflateDeflate("The quick brown fox jumps over the lazy dog");
}

TEST_F(GzipInflaterTest, InflateDeflateLargeDataHighEntropy) {
  SimpleRandom random(new NullMutex);
  GoogleString value = random.GenerateHighEntropyString(5 * kStackBufferSize);
  TestInflateDeflate(value);
}

TEST_F(GzipInflaterTest, IncrementalInflateOfOneShotDeflate) {
  const char kPayload[] = "The quick brown fox jumps over the lazy dog";
  GoogleString deflated, inflated;
  StringWriter deflate_writer(&deflated);
  EXPECT_TRUE(GzipInflater::Deflate(kPayload, &deflate_writer));

  char buf[STATIC_STRLEN(kPayload) + 1];
  const char kDontTouchMarker = 0xf;
  buf[STATIC_STRLEN(kPayload)] = kDontTouchMarker;
  GzipInflater inflater(GzipInflater::kDeflate);
  inflater.Init();
  EXPECT_FALSE(inflater.HasUnconsumedInput());
  EXPECT_TRUE(inflater.SetInput(deflated.data(), deflated.size()));
  EXPECT_TRUE(inflater.HasUnconsumedInput());
  int num_inflated_bytes = inflater.InflateBytes(buf, sizeof(buf));
  EXPECT_EQ(STATIC_STRLEN(kPayload), static_cast<size_t>(num_inflated_bytes));
  EXPECT_EQ(kDontTouchMarker, buf[STATIC_STRLEN(kPayload)]);
  EXPECT_FALSE(inflater.HasUnconsumedInput());
  EXPECT_TRUE(inflater.finished());
  EXPECT_FALSE(inflater.error());
  inflater.ShutDown();
  EXPECT_STREQ(kPayload, StringPiece(buf, num_inflated_bytes));
}

}  // namespace

}  // namespace net_instaweb
