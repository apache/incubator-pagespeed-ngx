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

// Author: Huibao Lin

#include "pagespeed/kernel/base/annotated_message_handler.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/message_handler_test_base.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

namespace {

  const int kLineNumber = 123;
  const char kEmptyString[] = "";
  const char kFileName[] = "my_file.cc";
  const char kMessage1[] = "Message No. 1.";
  const char kMessage2[] = "Message No. 2.";
  const char kMessage3[] = "Message No. 3.";
  const char kMessage4[] = "Message No. 4.";
  const char kSeparator[] = ": ";
  const char kURLInfo[] = "http://www.example.com/index.html: ";
  const char kURLPercentS[] = "http://www.example.com/%s.html: ";

class AnnotatedMessageHandlerTest : public testing::Test {
 protected:
  // Consruct message in the following format:
  //   "Type: FileName: LineNumber: Annotation: Message".
  const GoogleString FileMessage(MessageType type,
                                 const char* annotation,
                                 const char* message) {
    return StrCat(test_handler_.MessageTypeToString(type), kSeparator,
                  kFileName, kSeparator, IntegerToString(kLineNumber),
                  kSeparator, annotation, message);
  }

  // Consruct message in the following format:
  //   "Type: Annotation: Message".
  const GoogleString Message(MessageType type,
                             const char* annotation,
                             const char* message) {
    return StrCat(test_handler_.MessageTypeToString(type), kSeparator,
                  annotation, message);
  }

  const GoogleString message(int index) {
    return test_handler_.messages()[index];
  }

  int num_messages() {
    return test_handler_.messages().size();
  }

 protected:
  TestMessageHandler test_handler_;
};

TEST_F(AnnotatedMessageHandlerTest, WithAnnotation) {
  AnnotatedMessageHandler annotated_handler_(kURLInfo, &test_handler_);
  annotated_handler_.Info(kFileName, kLineNumber, kMessage1);
  annotated_handler_.Error(kFileName, kLineNumber, kMessage2);
  annotated_handler_.FatalError(kFileName, kLineNumber, kMessage3);
  annotated_handler_.Message(kError, kMessage4);

  ASSERT_EQ(4, num_messages());
  EXPECT_STREQ(FileMessage(kInfo, kURLInfo, kMessage1), message(0));
  EXPECT_STREQ(FileMessage(kError, kURLInfo, kMessage2), message(1));
  EXPECT_STREQ(FileMessage(kFatal, kURLInfo, kMessage3), message(2));
  EXPECT_STREQ(Message(kError, kURLInfo, kMessage4), message(3));
}

TEST_F(AnnotatedMessageHandlerTest, WithoutAnnotation) {
  AnnotatedMessageHandler annotated_handler_(&test_handler_);
  annotated_handler_.Info(kFileName, kLineNumber, kMessage1);
  annotated_handler_.Error(kFileName, kLineNumber, kMessage2);
  annotated_handler_.Message(kFatal, kMessage3);
  annotated_handler_.Message(kInfo, kMessage4);

  ASSERT_EQ(4, num_messages());
  EXPECT_STREQ(FileMessage(kInfo, kEmptyString, kMessage1), message(0));
  EXPECT_STREQ(FileMessage(kError, kEmptyString, kMessage2), message(1));
  EXPECT_STREQ(Message(kFatal, kEmptyString, kMessage3), message(2));
  EXPECT_STREQ(Message(kInfo, kEmptyString, kMessage4), message(3));
}

// Make sure that the message handler will not crash when there is "%s"
// in the URL, and will still produce correct messages.
TEST_F(AnnotatedMessageHandlerTest, URLHasPercentS) {
  AnnotatedMessageHandler annotated_handler_(kURLPercentS, &test_handler_);
  annotated_handler_.Info(kFileName, kLineNumber, kMessage1);
  annotated_handler_.Error(kFileName, kLineNumber, kMessage2);
  annotated_handler_.FatalError(kFileName, kLineNumber, kMessage3);
  annotated_handler_.Message(kError, kMessage4);

  ASSERT_EQ(4, num_messages());
  EXPECT_STREQ(FileMessage(kInfo, kURLPercentS, kMessage1), message(0));
  EXPECT_STREQ(FileMessage(kError, kURLPercentS, kMessage2), message(1));
  EXPECT_STREQ(FileMessage(kFatal, kURLPercentS, kMessage3), message(2));
  EXPECT_STREQ(Message(kError, kURLPercentS, kMessage4), message(3));
}

}  // namespace

}  // namespace net_instaweb
