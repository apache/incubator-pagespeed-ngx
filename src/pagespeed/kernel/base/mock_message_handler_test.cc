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

// Author: morlovich@google.com (Maksim Orlovich)

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"

namespace net_instaweb {

namespace {

const char kMessageAnotherInfo[] = "text another info message";
const char kMessageError[] = "text error message";
const char kMessageInfo[] = "text info message";
const char kMessageNotUsed[] = "text message not used";
const char kMessageWarning[] = "text warn message";

class MockMessageHandlerTest : public testing::Test {
 protected:
  MockMessageHandlerTest() : handler_(new NullMutex) {}

  void CheckCounts(int expectInfo, int expectWarn, int expectError,
                   int expectFatal) {
    EXPECT_EQ(expectInfo, handler_.MessagesOfType(kInfo));
    EXPECT_EQ(expectWarn, handler_.MessagesOfType(kWarning));
    EXPECT_EQ(expectError, handler_.MessagesOfType(kError));
    EXPECT_EQ(expectFatal, handler_.MessagesOfType(kFatal));
  }

  void CheckSkippedCounts(int expectSkippedInfo,
                          int expectSkippedWarn,
                          int expectSkippedError) {
    EXPECT_EQ(expectSkippedInfo, handler_.SkippedMessagesOfType(kInfo));
    EXPECT_EQ(expectSkippedWarn, handler_.SkippedMessagesOfType(kWarning));
    EXPECT_EQ(expectSkippedError, handler_.SkippedMessagesOfType(kError));
  }

  void ApplyAllMessages() {
    handler_.Message(kInfo, kMessageInfo);
    handler_.Message(kWarning, kMessageWarning);
    handler_.Message(kError, kMessageError);
    handler_.Message(kInfo, kMessageAnotherInfo);
  }

  MockMessageHandler handler_;
};


TEST_F(MockMessageHandlerTest, Simple) {
  EXPECT_EQ(0, handler_.TotalMessages());
  EXPECT_EQ(0, handler_.SeriousMessages());

  handler_.Message(kInfo, kMessageInfo);
  EXPECT_EQ(1, handler_.TotalMessages());
  EXPECT_EQ(0, handler_.SeriousMessages());
  CheckCounts(1, 0, 0, 0);

  handler_.Message(kWarning, kMessageWarning);
  EXPECT_EQ(2, handler_.TotalMessages());
  EXPECT_EQ(1, handler_.SeriousMessages());
  CheckCounts(1, 1, 0, 0);

  handler_.Message(kError, kMessageError);
  EXPECT_EQ(3, handler_.TotalMessages());
  EXPECT_EQ(2, handler_.SeriousMessages());
  CheckCounts(1, 1, 1, 0);

  // We can't actually test fatal, as it aborts
  // TODO(morlovich) mock the fatal behavior so the test does not crash

  handler_.Message(kInfo, kMessageAnotherInfo);
  EXPECT_EQ(4, handler_.TotalMessages());
  EXPECT_EQ(2, handler_.SeriousMessages());
  CheckCounts(2, 1, 1, 0);
}

TEST_F(MockMessageHandlerTest, SkippedMessage) {
  ApplyAllMessages();
  CheckCounts(2, 1, 1, 0);
  EXPECT_EQ(4, handler_.TotalMessages());
  CheckSkippedCounts(0, 0, 0);
  EXPECT_EQ(0, handler_.TotalSkippedMessages());

  handler_.AddPatternToSkipPrinting(kMessageInfo);
  ApplyAllMessages();
  CheckCounts(4, 2, 2, 0);
  EXPECT_EQ(8, handler_.TotalMessages());
  CheckSkippedCounts(1, 0, 0);
  EXPECT_EQ(1, handler_.TotalSkippedMessages());

  handler_.AddPatternToSkipPrinting(kMessageWarning);
  ApplyAllMessages();
  CheckCounts(6, 3, 3, 0);
  EXPECT_EQ(12, handler_.TotalMessages());
  CheckSkippedCounts(2, 1, 0);
  EXPECT_EQ(3, handler_.TotalSkippedMessages());

  handler_.AddPatternToSkipPrinting(kMessageError);
  ApplyAllMessages();
  CheckCounts(8, 4, 4, 0);
  EXPECT_EQ(16, handler_.TotalMessages());
  CheckSkippedCounts(3, 2, 1);
  EXPECT_EQ(6, handler_.TotalSkippedMessages());

  handler_.AddPatternToSkipPrinting(kMessageNotUsed);
  ApplyAllMessages();
  CheckCounts(10, 5, 5, 0);
  EXPECT_EQ(20, handler_.TotalMessages());
  CheckSkippedCounts(4, 3, 2);
  EXPECT_EQ(9, handler_.TotalSkippedMessages());
}

}  // namespace

}  // namespace net_instaweb
