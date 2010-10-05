/**
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

// Unit-test the string-splitter.

#include "net/instaweb/util/public/string_buffer.h"

#include "base/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

namespace {

struct CollectSizes : public Writer {
 public:
  CollectSizes() { }

  virtual bool Write(const StringPiece& piece, MessageHandler* handler) {
    sizes.push_back(piece.size());
    return true;
  }
  virtual bool Flush(MessageHandler* handler) {
    return true;
  }
  std::vector<int> sizes;

 private:
  DISALLOW_COPY_AND_ASSIGN(CollectSizes);
};

}  // namespace

class StringBufferTest : public testing::Test {
 protected:
  // Makes two big string buffers with the same content, but with
  // different alignments in the substrings.
  void MakeTwoBigStringBuffers(StringBuffer* buffer1, StringBuffer* buffer2) {
    buffer1->Append(std::string(min_string_size() - 2, ' '));
    buffer2->Append(std::string(min_string_size() - 2, ' '));
    buffer1->Append("xyzzy");
    buffer2->Append("xy");
    buffer2->Append("zzy");
  }

  int min_string_size() const { return StringBuffer::kMinStringSize; }
};

TEST_F(StringBufferTest, TestAppend) {
  StringBuffer buffer;
  buffer.Append("Hello, ");
  buffer.Append("World!");
  EXPECT_EQ("Hello, World!", buffer.ToString());
}

TEST_F(StringBufferTest, TestEq) {
  StringBuffer buffer1, buffer2;
  MakeTwoBigStringBuffers(&buffer1, &buffer2);

  // We should now have the two buffers each with the same content,
  // but split differently.  Let's just check that assumption
  CollectSizes size_vector1;
  CollectSizes size_vector2;
  buffer1.Write(&size_vector1, NULL);
  buffer2.Write(&size_vector2, NULL);

  // We can't use ASSERT_NE because gunit requires the types
  // to that macro to implement operator<<(stream...), which is
  // not worth it IMO for one assert.
  ASSERT_FALSE(size_vector1.sizes == size_vector2.sizes);

  EXPECT_TRUE(buffer1 == buffer2);
  EXPECT_TRUE(buffer2 == buffer1);
  EXPECT_FALSE(buffer1 != buffer2);
  EXPECT_FALSE(buffer2 != buffer1);

  buffer1.Append("1");
  buffer2.Append("2");
  EXPECT_FALSE(buffer1 == buffer2);
  EXPECT_FALSE(buffer2 == buffer1);
  EXPECT_TRUE(buffer1 != buffer2);
  EXPECT_TRUE(buffer2 != buffer1);
}

TEST_F(StringBufferTest, TestSmallSubString) {
  StringBuffer buffer("Hello, World!");
  EXPECT_EQ("Hello, ", buffer.SubString(0, 7));
  EXPECT_EQ("ello, ", buffer.SubString(1, 6));
  EXPECT_EQ("World!", buffer.SubString(7, std::string::npos));
  EXPECT_EQ("World!", buffer.SubString(7, 20));
}

TEST_F(StringBufferTest, TestBigSubString) {
  StringBuffer buffer1, buffer2;
  MakeTwoBigStringBuffers(&buffer1, &buffer2);
  EXPECT_EQ(std::string("     "), buffer1.SubString(0, 5));
  EXPECT_EQ(std::string("xyzzy"), buffer1.SubString(buffer1.size() - 5, 5));
  EXPECT_EQ(std::string("xyzzy"), buffer2.SubString(buffer2.size() - 5, 5));
  EXPECT_EQ(std::string("xyzzy"), buffer2.SubString(buffer2.size() - 5,
                                                 std::string::npos));
}

TEST_F(StringBufferTest, TestEmptySubString) {
  EXPECT_EQ("", StringBuffer("Hello").SubString(5, 0));
  EXPECT_EQ("", StringBuffer("Hello").SubString(5, 1));
  EXPECT_EQ("", StringBuffer("Hello").SubString(5, StringBuffer::npos));
}

}  // namespace net_instaweb
