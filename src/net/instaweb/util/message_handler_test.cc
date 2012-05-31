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

#include "net/instaweb/util/public/message_handler.h"

#include <cstdarg>
#include <vector>
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

class TestMessageHandler : public net_instaweb::MessageHandler {
 public:
  typedef std::vector<GoogleString> MessageVector;

  const MessageVector& messages() { return messages_; }

 protected:
  virtual void MessageVImpl(MessageType type, const char* msg,
                            va_list args) {
    GoogleString message;
    StringAppendF(&message, "%s: ", MessageTypeToString(type));
    StringAppendV(&message, msg, args);
    messages_.push_back(message);
  }

  virtual void FileMessageVImpl(MessageType type, const char* filename,
                                int line, const char* msg, va_list args) {
    GoogleString message;
    StringAppendF(&message, "%s: %s: %d: ", MessageTypeToString(type),
                  filename, line);
    StringAppendV(&message, msg, args);
    messages_.push_back(message);
  }

 private:
  MessageVector messages_;
};

class MessageHandlerTest : public testing::Test {
 protected:
  const TestMessageHandler::MessageVector& messages() {
    return handler_.messages();
  }

  TestMessageHandler handler_;
};


TEST_F(MessageHandlerTest, Simple) {
  handler_.Message(kWarning, "here is a message");
  handler_.Info("filename.cc", 1, "here is another message");
  ASSERT_EQ(2U, messages().size());
  ASSERT_EQ(messages()[0], "Warning: here is a message");
  ASSERT_EQ(messages()[1], "Info: filename.cc: 1: here is another message");
  ASSERT_EQ(kWarning, MessageHandler::StringToMessageType("Warning"));
  ASSERT_EQ(kFatal, MessageHandler::StringToMessageType("Fatal"));
  //
  // ASSERT_DEATH_IF_SUPPORTED is a cool idea, but it prints:
  //   [WARNING] testing/gtest/src/gtest-death-test.cc:789:: Death tests use
  //   fork(), which is unsafe particularly in a threaded context. For this
  //   test, Google Test couldn't detect the number of threads.
  // and seems to core-dump sporadically.
  //
  // ASSERT_DEATH_IF_SUPPORTED(MessageHandler::StringToMessageType("Random"),
  //                           "Invalid msg level: Random");
}

TEST_F(MessageHandlerTest, MinMessageType) {
  handler_.set_min_message_type(kError);
  handler_.Info("filename.cc", 1, "here is a message");
  handler_.Warning("filename.cc", 1, "here is a message");
  ASSERT_EQ(0U, messages().size());
  handler_.Error("filename.cc", 1, "here is another message");
  ASSERT_EQ(1U, messages().size());
  ASSERT_EQ(messages()[0], "Error: filename.cc: 1: here is another message");
}

}  // namespace

}  // namespace net_instaweb
