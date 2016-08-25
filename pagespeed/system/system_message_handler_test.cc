/*
 * Copyright 2014 Google Inc.
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
//
// Unit tests for SystemMessageHandler

#include "pagespeed/system/system_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

class SystemMessageHandlerTest : public testing::Test {
 protected:
  SystemMessageHandlerTest()
      : thread_system_(Platform::CreateThreadSystem()),
        timer_(thread_system_->NewMutex(), MockTimer::kApr_5_2010_ms),
        system_message_handler_(&timer_, thread_system_->NewMutex()),
        writer_(&buffer_) {
    system_message_handler_.set_buffer(&writer_);
    system_message_handler_.SetPidString(1234);
  }

  void AddMessage(MessageType type, StringPiece msg) {
    system_message_handler_.AddMessageToBuffer(type, msg);
  }

  void AddMessage(MessageType type, const char* file, int line,
                  StringPiece msg) {
    system_message_handler_.AddMessageToBuffer(type, file, line, msg);
  }

  scoped_ptr<ThreadSystem> thread_system_;
  MockTimer timer_;
  SystemMessageHandler system_message_handler_;
  GoogleString buffer_;
  StringWriter writer_;
};

// Tests that multi-line messages are annotated with the type-code for
// each line.
TEST_F(SystemMessageHandlerTest, WrapLongLinesError) {
  AddMessage(kError, "Now is the time\nfor all good men\nto come to the aid");
  EXPECT_STREQ(
      "E[Mon, 05 Apr 2010 18:51:26 GMT] [Error] [1234] Now is the time\n"
      "Efor all good men\n"
      "Eto come to the aid\n",
      buffer_);
}

TEST_F(SystemMessageHandlerTest, WrapLongLinesInfo) {
  AddMessage(kInfo, "Now is the time\nfor all good men\nto come to the aid");
  EXPECT_STREQ(
      "I[Mon, 05 Apr 2010 18:51:26 GMT] [Info] [1234] Now is the time\n"
      "Ifor all good men\n"
      "Ito come to the aid\n",
      buffer_);
}

TEST_F(SystemMessageHandlerTest, WrapLongLinesWarning) {
  AddMessage(kWarning, "Now is the time\nfor all good men\nto come to the aid");
  EXPECT_STREQ(
      "W[Mon, 05 Apr 2010 18:51:26 GMT] [Warning] [1234] Now is the time\n"
      "Wfor all good men\n"
      "Wto come to the aid\n",
      buffer_);
}

TEST_F(SystemMessageHandlerTest, AddsFileLineInfo) {
  AddMessage(kInfo, "test_file.cc", 4321, "Test message");
  EXPECT_STREQ(
      "I[Mon, 05 Apr 2010 18:51:26 GMT] [Info] [1234] "
      "[test_file.cc:4321] Test message\n",
      buffer_);
}

TEST_F(SystemMessageHandlerTest, AddsFileLineInfoWrapLongLines) {
  AddMessage(kInfo, "test_file.cc", 4321, "Test message with\nnew line.");
  EXPECT_STREQ(
      "I[Mon, 05 Apr 2010 18:51:26 GMT] [Info] [1234] "
      "[test_file.cc:4321] Test message with\n"
      "Inew line.\n",
      buffer_);
}

}  // namespace net_instaweb
