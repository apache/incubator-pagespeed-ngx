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

#include "pagespeed/kernel/http/bot_checker.h"

#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {

class BotCheckerTest : public testing::Test {};

TEST_F(BotCheckerTest, DetectUserAgents) {
  // Spider.
  EXPECT_TRUE(BotChecker::Lookup(
      "Baiduspider+(+http://www.baidu.com/search/spider.htm)"));
  EXPECT_TRUE(BotChecker::Lookup(
      "Baiduspider+(+http://help.baidu.jp/system/05.html)"));
  EXPECT_TRUE(BotChecker::Lookup(
      "BaidusSpIDER+(+http://help.baidu.jp/system/05.html)"));

  // Bot.
  EXPECT_TRUE(BotChecker::Lookup("msnbot-UDiscovery/2.0b"));
  EXPECT_TRUE(BotChecker::Lookup(
      "Mozilla/5.0 (compatible; bingbot/2.0;"
      "+http://www.bing.com/bingbot.htm)"));
  EXPECT_TRUE(BotChecker::Lookup("bitlybot"));
  EXPECT_TRUE(BotChecker::Lookup("bitlyBoT"));

  // Crawl.
  EXPECT_TRUE(BotChecker::Lookup("CrAwLER"));

  // In map.
  EXPECT_TRUE(BotChecker::Lookup("Mediapartners-Google"));
  EXPECT_TRUE(BotChecker::Lookup(
      "Mozilla/5.0 (compatible; Yahoo! Slurp;"
      "http://help.yahoo.com/help/us/ysearch/slurp)"));
  EXPECT_TRUE(BotChecker::Lookup(
      "Mozilla/5.0 (compatible; Ask Jeeves/Teoma;"
      "+http://about.ask.com/en/docs/about/webmasters.shtml"));

  // Empty.
  EXPECT_TRUE(BotChecker::Lookup(""));

  // Wget.
  EXPECT_FALSE(BotChecker::Lookup("Wget/1.12 (linux-gnu)"));
  EXPECT_FALSE(BotChecker::Lookup(
      "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/28.0.1500.71 Safari/537.36"));
}

}  // namespace net_instaweb
