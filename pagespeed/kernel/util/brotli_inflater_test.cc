/*
 * Copyright 2016 Google Inc.
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

// Author: jcrowell@google.com (Jeffrey Crowell)

#include "pagespeed/kernel/util/brotli_inflater.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/message_handler_test_base.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/util/simple_random.h"
#include "pagespeed/kernel/base/stack_buffer.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"

namespace net_instaweb {

namespace {

// Generated with command line tool "bro".
static const char kHello[] = "hello\n";
static const char kHelloBrotli[] = "\x8b\x02\x80\x68\x65\x6c\x6c\x6f\x0a\x03";
// Use the highest quality by default (11).
static const int kCompressionLevel = 11;

// Writer that returns false on Write().
class StringWriterReturningFalse : public StringWriter {
 public:
  explicit StringWriterReturningFalse(GoogleString* str) : StringWriter(str) {}
  bool Write(const StringPiece& str,
             net_instaweb::MessageHandler* message_handler) {
    StringWriter::Write(str, message_handler);
    return false;
  }
};

TEST(BrotliInflater, TestBrotliDecompress) {
  // Compress and decompress a simple string.
  GoogleString decompressed;
  TestMessageHandler handler;
  StringWriter decompress_writer(&decompressed);
  StringPiece compressed(kHelloBrotli, sizeof(kHelloBrotli));
  EXPECT_TRUE(
      BrotliInflater::Decompress(compressed, &handler, &decompress_writer));
  EXPECT_STREQ(kHello, decompressed);
  EXPECT_EQ(0, handler.messages().size());
}

TEST(BrotliInflater, TestFailedWriteBrotliDecompress) {
  // Use a writer that returns failure on write.
  GoogleString decompressed;
  TestMessageHandler handler;
  StringWriterReturningFalse decompress_writer(&decompressed);
  StringPiece compressed(kHelloBrotli, sizeof(kHelloBrotli));
  EXPECT_FALSE(
      BrotliInflater::Decompress(compressed, &handler, &decompress_writer));
  EXPECT_EQ(0, handler.messages().size());
}

TEST(BrotliInflater, TestCorruptInputBrotliDecompress) {
  // Take "hello\n" but replace the first 2 bytes with "AB", so it will not be
  // valid brotli.
  const char kHelloBrotliCorrupt[] = "AB\x80\x68\x65\x6c\x6c\x6f\x0a\x03";
  GoogleString decompressed;
  TestMessageHandler handler;
  StringWriterReturningFalse decompress_writer(&decompressed);
  StringPiece compressed(kHelloBrotliCorrupt, sizeof(kHelloBrotliCorrupt));
  EXPECT_FALSE(
      BrotliInflater::Decompress(compressed, &handler, &decompress_writer));
  ASSERT_GE(handler.messages().size(), 1);
  const GoogleString& message = handler.messages()[0];
  EXPECT_TRUE(message.find("PADDING_1") != GoogleString::npos ||
              message == "Error: BROTLI_DECODER_RESULT_ERROR")
      << message;
}

TEST(BrotliInflater, TestTruncatedInputBrotliDecompress) {
  // Take "hello\n" but truncate the string, so it will not be valid brotli.
  const char kHelloBrotliCorrupt[] = "\x8b\x02\x80\x68\x65\x6c\x6c\x6f";
  GoogleString decompressed;
  TestMessageHandler handler;
  StringWriterReturningFalse decompress_writer(&decompressed);
  StringPiece compressed(kHelloBrotliCorrupt, sizeof(kHelloBrotliCorrupt));
  EXPECT_FALSE(
      BrotliInflater::Decompress(compressed, &handler, &decompress_writer));
  EXPECT_STREQ("Warning: BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT",
               handler.messages()[0]);
}

TEST(BrotliInflater, TestCompressDecompressSmallString) {
  // Compress and decompress a simple string, ensure that it remains unchanged.
  StringPiece payload(kHello);
  TestMessageHandler handler;
  GoogleString compressed, decompressed;
  StringWriter compressed_writer(&compressed);
  EXPECT_TRUE(BrotliInflater::Compress(payload, kCompressionLevel, &handler,
                                       &compressed_writer));
  StringWriter decompressed_writer(&decompressed);
  EXPECT_TRUE(
      BrotliInflater::Decompress(compressed, &handler, &decompressed_writer));
  EXPECT_STREQ(payload, decompressed);
  EXPECT_EQ(0, handler.messages().size());
}

TEST(BrotliInflater, TestCompressDecompressLargeString) {
  // Compress and decompress a long string of repeated characters that will
  // exceed the buffer size.
  TestMessageHandler handler;
  GoogleString value(5 * kStackBufferSize, 'A');
  StringPiece payload(value);
  GoogleString compressed, decompressed;
  StringWriter compressed_writer(&compressed);
  EXPECT_TRUE(BrotliInflater::Compress(payload, kCompressionLevel, &handler,
                                       &compressed_writer));
  StringWriter decompressed_writer(&decompressed);
  EXPECT_TRUE(
      BrotliInflater::Decompress(compressed, &handler, &decompressed_writer));
  EXPECT_STREQ(payload, decompressed);
  EXPECT_EQ(0, handler.messages().size());
}

TEST(BrotliInflater, TestCompressDecompressLargeStringWithPoorCompression) {
  // Compress and decompress a long string of random characters.
  TestMessageHandler handler;
  SimpleRandom random(new NullMutex);
  GoogleString value = random.GenerateHighEntropyString(5 * kStackBufferSize);
  StringPiece payload(value);
  GoogleString compressed, decompressed;
  StringWriter compressed_writer(&compressed);
  EXPECT_TRUE(BrotliInflater::Compress(payload, kCompressionLevel, &handler,
                                       &compressed_writer));
  StringWriter decompressed_writer(&decompressed);
  EXPECT_TRUE(
      BrotliInflater::Decompress(compressed, &handler, &decompressed_writer));
  EXPECT_STREQ(payload, decompressed);
  EXPECT_EQ(0, handler.messages().size());
}

}  // namespace

}  // namespace net_instaweb
