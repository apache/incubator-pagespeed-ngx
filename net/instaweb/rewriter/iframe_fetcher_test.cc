/*
 * Copyright 2015 Google Inc.
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

#include "net/instaweb/rewriter/public/iframe_fetcher.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_options_test_base.h"
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/http_options.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"
#include "pagespeed/kernel/http/user_agent_matcher_test_base.h"

namespace net_instaweb {
namespace {

const char kUrl[] = "http://www.example.com/";

// Tests the AsyncFetch class and some of its derivations.
class IframeFetcherTest : public RewriteOptionsTestBase<RewriteOptions> {
 protected:
  IframeFetcherTest()
      : request_context_(new RequestContext(
            kDefaultHttpOptionsForTests, new NullMutex, NULL)),
        fetch_(request_context_),
        handler_(new NullMutex),
        options_(thread_system_.get()) {
  }

  void InitTest(StringPiece user_agent, bool suffix_mode) {
    fetch_.request_headers()->Add(HttpAttributes::kUserAgent, user_agent);
    DomainLawyer* lawyer = options_.WriteableDomainLawyer();
    if (suffix_mode) {
      lawyer->set_proxy_suffix(".suffix");
    } else {
      lawyer->AddOriginDomainMapping("example.com", "example.us",
                                     "example.com", &handler_);
    }
    fetcher_.reset(new IframeFetcher(&options_, &matcher_));
  }

 protected:
  UserAgentMatcher matcher_;
  RequestContextPtr request_context_;
  StringAsyncFetch fetch_;
  MockMessageHandler handler_;
  RewriteOptions options_;
  scoped_ptr<IframeFetcher> fetcher_;
};

TEST_F(IframeFetcherTest, IframeOnMobileProxySuffix) {
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent, true);
  fetcher_->Fetch("http://example.com.suffix/foo?bar", &handler_, &fetch_);
  ResponseHeaders* response = fetch_.response_headers();
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(fetch_.buffer(), ::testing::HasSubstr(
      "iframe.src = 'http://example.com/foo?bar'"));
}

TEST_F(IframeFetcherTest, RedirectOnDesktopProxySuffix) {
  InitTest(UserAgentMatcherTestBase::kOperaMiniMobileUserAgent, true);
  fetcher_->Fetch("http://example.com.suffix/foo?bar", &handler_, &fetch_);
  ResponseHeaders* response = fetch_.response_headers();
  EXPECT_EQ(HttpStatus::kTemporaryRedirect, response->status_code());
  EXPECT_STREQ("http://example.com/foo?bar",
               response->Lookup1(HttpAttributes::kLocation));
}

TEST_F(IframeFetcherTest, ErrorProxySuffix) {
  // Report an error for a configuration problem that results in the
  // domain being unmapped.
  InitTest(UserAgentMatcherTestBase::kOperaMiniMobileUserAgent, true);
  fetcher_->Fetch("http://example.com/foo?bar", &handler_, &fetch_);
  ResponseHeaders* response = fetch_.response_headers();
  EXPECT_EQ(HttpStatus::kNotImplemented, response->status_code());
}

TEST_F(IframeFetcherTest, IframeOnMobileMapOrigin) {
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent, false);
  fetcher_->Fetch("http://example.us/foo?bar", &handler_, &fetch_);
  ResponseHeaders* response = fetch_.response_headers();
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(fetch_.buffer(), ::testing::HasSubstr(
      "iframe.src = 'http://example.com/foo?bar'"));
}

TEST_F(IframeFetcherTest, RedirectOnDesktopMapOrigin) {
  InitTest(UserAgentMatcherTestBase::kOperaMiniMobileUserAgent, false);
  fetcher_->Fetch("http://example.us/foo?bar", &handler_, &fetch_);
  ResponseHeaders* response = fetch_.response_headers();
  EXPECT_EQ(HttpStatus::kTemporaryRedirect, response->status_code());
  EXPECT_STREQ("http://example.com/foo?bar",
               response->Lookup1(HttpAttributes::kLocation));
}

TEST_F(IframeFetcherTest, ErrorMapOrigin) {
  // Report an error for a configuration problem that results in the
  // domain being unmapped.
  InitTest(UserAgentMatcherTestBase::kOperaMiniMobileUserAgent, false);
  fetcher_->Fetch("http://example.com/foo?bar", &handler_, &fetch_);
  ResponseHeaders* response = fetch_.response_headers();
  EXPECT_EQ(HttpStatus::kNotImplemented, response->status_code());
}

}  // namespace

}  // namespace net_instaweb
