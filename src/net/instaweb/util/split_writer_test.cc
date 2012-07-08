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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/util/public/split_writer.h"

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {
class MessageHandler;

namespace {

TEST(SplitWriterTest, SplitsWrite) {
  GoogleString str1, str2;
  StringWriter writer1(&str1), writer2(&str2);
  SplitWriter split_writer(&writer1, &writer2);

  EXPECT_TRUE(str1.empty());
  EXPECT_TRUE(str2.empty());

  EXPECT_TRUE(split_writer.Write("Hello, ", NULL));
  EXPECT_EQ("Hello, ", str1);
  EXPECT_EQ("Hello, ", str2);

  EXPECT_TRUE(writer1.Write("World!", NULL));
  EXPECT_TRUE(writer2.Write("Nobody.", NULL));
  EXPECT_EQ("Hello, World!", str1);
  EXPECT_EQ("Hello, Nobody.", str2);

  EXPECT_TRUE(split_writer.Write(" Goodbye.", NULL));
  EXPECT_EQ("Hello, World! Goodbye.", str1);
  EXPECT_EQ("Hello, Nobody. Goodbye.", str2);

  EXPECT_TRUE(split_writer.Flush(NULL));
}

class FailWriter : public Writer {
 public:
  virtual bool Write(const StringPiece& str, MessageHandler* handler) {
    return false;
  }

  virtual bool Flush(MessageHandler* handler) {
    return false;
  }
};

TEST(SplitWriterTest, WritesToBothEvenOnFailure) {
  FailWriter fail_writer;
  GoogleString str;
  StringWriter string_writer(&str);

  SplitWriter split_fail_first(&fail_writer, &string_writer);
  EXPECT_TRUE(str.empty());
  EXPECT_FALSE(split_fail_first.Write("Hello, World!", NULL));
  EXPECT_EQ("Hello, World!", str);
  EXPECT_FALSE(split_fail_first.Flush(NULL));

  str.clear();

  SplitWriter split_fail_second(&string_writer, &fail_writer);
  EXPECT_TRUE(str.empty());
  EXPECT_FALSE(split_fail_second.Write("Hello, World!", NULL));
  EXPECT_EQ("Hello, World!", str);
  EXPECT_FALSE(split_fail_second.Flush(NULL));
}

}  // namespace

}  // namespace net_instaweb
