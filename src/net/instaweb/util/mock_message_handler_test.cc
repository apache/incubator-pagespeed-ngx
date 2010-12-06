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

// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"

namespace net_instaweb {

namespace {

class MockMessageHandlerTest : public testing::Test {
 protected:
  void CheckCounts(int expectInfo, int expectWarn, int expectError,
                   int expectFatal) {
    EXPECT_EQ(expectInfo, handler_.MessagesOfType(kInfo));
    EXPECT_EQ(expectWarn, handler_.MessagesOfType(kWarning));
    EXPECT_EQ(expectError, handler_.MessagesOfType(kError));
    EXPECT_EQ(expectFatal, handler_.MessagesOfType(kFatal));
  }

  MockMessageHandler handler_;
};


TEST_F(MockMessageHandlerTest, Simple) {
  EXPECT_EQ(0, handler_.TotalMessages());
  EXPECT_EQ(0, handler_.SeriousMessages());

  handler_.Message(kInfo, "test info message");
  EXPECT_EQ(1, handler_.TotalMessages());
  EXPECT_EQ(0, handler_.SeriousMessages());
  CheckCounts(1, 0, 0, 0);

  handler_.Message(kWarning, "text warning message");
  EXPECT_EQ(2, handler_.TotalMessages());
  EXPECT_EQ(1, handler_.SeriousMessages());
  CheckCounts(1, 1, 0, 0);

  handler_.Message(kError, "text Error message");
  EXPECT_EQ(3, handler_.TotalMessages());
  EXPECT_EQ(2, handler_.SeriousMessages());
  CheckCounts(1, 1, 1, 0);

  // We can't actually test fatal, as it aborts
  // TODO(morlovich) mock the fatal behavior so the test does not crash

  handler_.Message(kInfo, "another test info message");
  EXPECT_EQ(4, handler_.TotalMessages());
  EXPECT_EQ(2, handler_.SeriousMessages());
  CheckCounts(2, 1, 1, 0);
}

}  // namespace

}  // namespace net_instaweb
