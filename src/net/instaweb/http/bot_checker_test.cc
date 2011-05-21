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

// Author: fangfei@google.com (Fangfei Zhou)

// Unit-test the caching fetcher, using a mock fetcher, and an async
// wrapper around that.

#include "net/instaweb/http/public/bot_checker.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class BotCheckerTest : public testing::Test {};

// Case for bot user-agent string contains no separator
TEST_F(BotCheckerTest, DetectUserAgentWithNoSeparator) {
  const char user_agent[] = "Baiduspider+(+http://www.baidu.com/search/spider.htm)";
  EXPECT_TRUE(BotChecker::Lookup(user_agent));
}

// Case for bot user-agent with no version
TEST_F(BotCheckerTest, DetectUserAgentWithNoVersion) {
  const char user_agent[] = "Mozilla/5.0 (compatible; Yahoo! Slurp;"
               "http://help.yahoo.com/help/us/ysearch/slurp)";
  EXPECT_TRUE(BotChecker::Lookup(user_agent));
}

// Case for bot user-agent "application/version"
TEST_F(BotCheckerTest, DetectUserAgentWithVersion) {
  const char user_agent[] = "Mozilla/5.0 (compatible; bingbot/2.0;"
               "+http://www.bing.com/bingbot.htm)";
  EXPECT_TRUE(BotChecker::Lookup(user_agent));
}

// Case for bot user-agent "http://domain/version"
TEST_F(BotCheckerTest, DetectUserAgentWithDomain) {
  const char user_agent[] = "Mozilla/5.0 (compatible; Ask Jeeves/Teoma;"
               "+http://about.ask.com/en/docs/about/webmasters.shtml";
  EXPECT_TRUE(BotChecker::Lookup(user_agent));
}

// Case for non-bot
TEST_F(BotCheckerTest, DetectUserAgentWithNoBot) {
  const char user_agent[] = "Wget/1.12 (linux-gnu)";
  EXPECT_FALSE(BotChecker::Lookup(user_agent));
}

}  // namespace net_instaweb
