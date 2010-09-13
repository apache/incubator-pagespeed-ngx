// Copyright 2010 Google Inc. All Rights Reserved.
// Author: bmcquade@google.com (Bryan McQuade)

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

class TestMessageHandler : public net_instaweb::MessageHandler {
 public:
  typedef std::vector<std::string> MessageVector;

  const MessageVector& messages() { return messages_; }

 protected:
  virtual void MessageVImpl(MessageType type, const char* msg,
                            va_list args) {
    std::string message;
    StringAppendF(&message, "%s: ", MessageTypeToString(type));
    StringAppendV(&message, msg, args);
    messages_.push_back(message);
  }

  virtual void FileMessageVImpl(MessageType type, const char* filename,
                                int line, const char* msg, va_list args) {
    std::string message;
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
