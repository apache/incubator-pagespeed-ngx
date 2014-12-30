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

// Author: jmarantz@google.com (Joshua Marantz)

#include "pagespeed/apache/apache_config.h"

#include "net/instaweb/rewriter/public/rewrite_options_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/null_thread_system.h"

namespace net_instaweb {

class ApacheConfigTest : public RewriteOptionsTestBase<ApacheConfig> {
 protected:
  ApacheConfigTest() : config_("test", &thread_system_) {
  }

  NullThreadSystem thread_system_;
  ApacheConfig config_;
};

TEST_F(ApacheConfigTest, Auth) {
  StringPiece name, value, redirect;
  EXPECT_FALSE(config_.GetProxyAuth(&name, &value, &redirect));
  config_.set_proxy_auth("cookie=value:http://example.com/url");
  ASSERT_TRUE(config_.GetProxyAuth(&name, &value, &redirect));
  EXPECT_STREQ("cookie", name);
  EXPECT_STREQ("value", value);
  EXPECT_STREQ("http://example.com/url", redirect);
  config_.set_proxy_auth("cookie2=value2");
  ASSERT_TRUE(config_.GetProxyAuth(&name, &value, &redirect));
  EXPECT_STREQ("cookie2", name);
  EXPECT_STREQ("value2", value);
  EXPECT_STREQ("", redirect);
  config_.set_proxy_auth("cookie3");
  ASSERT_TRUE(config_.GetProxyAuth(&name, &value, &redirect));
  EXPECT_STREQ("cookie3", name);
  EXPECT_STREQ("", value);
  EXPECT_STREQ("", redirect);
  config_.set_proxy_auth("cookie4:http://example.com/url2");
  ASSERT_TRUE(config_.GetProxyAuth(&name, &value, &redirect));
  EXPECT_STREQ("cookie4", name);
  EXPECT_STREQ("", value);
  EXPECT_STREQ("http://example.com/url2", redirect);
}

}  // namespace net_instaweb
