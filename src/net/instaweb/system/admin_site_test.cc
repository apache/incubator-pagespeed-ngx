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

// Author: xqyin@google.com (XiaoQian Yin)
// Unit tests for AdminSite.

#include "net/instaweb/system/public/admin_site.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/rewriter/public/custom_rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/system/public/system_rewrite_options.h"
#include "net/instaweb/system/public/system_server_context.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/platform.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/thread_system.h"
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

class RewriteDriverFactory;

namespace {

class SystemServerContextNoProxyHtml : public SystemServerContext {
 public:
  explicit SystemServerContextNoProxyHtml(RewriteDriverFactory* factory)
      : SystemServerContext(factory, "fake_hostname", 80 /* fake port */) {
  }

  virtual bool ProxiesHtml() const { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemServerContextNoProxyHtml);
};

class AdminSiteTest : public CustomRewriteTestBase<SystemRewriteOptions> {
 protected:
  AdminSiteTest()
      : thread_system_(Platform::CreateThreadSystem()),
        options_(new SystemRewriteOptions(thread_system_.get())),
        admin_site_(new AdminSite(factory()->static_asset_manager(), timer(),
                                  message_handler())) {
  }

  virtual void SetUp() {
    CustomRewriteTestBase<SystemRewriteOptions>::SetUp();
    server_context_.reset(SetupServerContext(options_.release()));
  }

  virtual void TearDown() {
    RewriteTestBase::TearDown();
  }

  // Set up the ServerContext. The ServerContext is only used for
  // PrintCaches method. If we remove this dependency later,
  // we don't need to set up ServerContext in this unit test.
  ServerContext* SetupServerContext(SystemRewriteOptions* config) {
    scoped_ptr<SystemServerContext> server_context(
        new SystemServerContextNoProxyHtml(factory()));
    server_context->reset_global_options(config);
    server_context->set_statistics(factory()->statistics());
    return server_context.release();
  }

  scoped_ptr<ThreadSystem> thread_system_;
  scoped_ptr<ServerContext> server_context_;
  scoped_ptr<SystemRewriteOptions> options_;
  scoped_ptr<AdminSite> admin_site_;
};

TEST_F(AdminSiteTest, ColorMessagesInHistoryPage) {
  EXPECT_EQ(message_handler(), admin_site_->MessageHandlerForTesting());
  // Due to the size limit to the SharedCircularBuffer, the earliest message
  // in the buffer may be incomplete. In order to always display complete
  // messages on the history page, we simply ignore all the things before the
  // first new line. So here we inject a useless line at the beginning to show
  // that we throw out the first (possibly) incomplete line.
  message_handler()->Message(kInfo, "Ignore the first line.");
  message_handler()->Message(kError, "Test for %s", "Errors");
  message_handler()->Message(kWarning, "Test for %s", "Warnings");
  message_handler()->Message(kInfo, "Test for %s", "Infos");
  GoogleString buffer;
  StringAsyncFetch fetch(rewrite_driver()->request_context(), &buffer);
  static const char kColorTemplate[] = "color:%s; margin:0;";
  // The value of the first argument AdminSite::AdminSource
  // does not matter in this test. So we just test for kPageSpeedAdmin here.
  admin_site_->MessageHistoryHandler(*(rewrite_driver()->options()),
                                     AdminSite::kPageSpeedAdmin, &fetch);
  EXPECT_THAT(
      buffer, ::testing::HasSubstr(StringPrintf(kColorTemplate, "red")));
  EXPECT_THAT(
      buffer, ::testing::HasSubstr(StringPrintf(kColorTemplate, "brown")));
  EXPECT_THAT(buffer, ::testing::HasSubstr("style=\"margin:0;\""));
}
// TODO(xqyin): Add unit tests for other methods in AdminSite.

}  // namespace

}  // namespace net_instaweb
