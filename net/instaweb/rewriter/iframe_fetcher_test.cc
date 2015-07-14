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

const char kExpectedUrl[] = "http://example.com/foo?bar";
const char kExpectedIframe[] =
    "<iframe id=\"psmob-iframe\" src=\"http://example.com/foo?bar\">";

// Tests the AsyncFetch class and some of its derivations.
class IframeFetcherTest : public RewriteOptionsTestBase<RewriteOptions> {
 protected:
  enum SuffixMode {kProxySuffix, kMapOrigin};
  enum AlwaysMobilize {kOnAllDevices, kOnlyOnMobile};

  IframeFetcherTest()
      : request_context_(new RequestContext(
            kDefaultHttpOptionsForTests, new NullMutex, NULL)),
        fetch_(request_context_),
        handler_(new NullMutex),
        options_(thread_system_.get()) {
  }

  void InitTest(StringPiece user_agent, SuffixMode suffix_mode,
                AlwaysMobilize always_mobilize) {
    fetch_.request_headers()->Add(HttpAttributes::kUserAgent, user_agent);
    options_.set_mob_always(always_mobilize == kOnAllDevices);
    options_.EnableFilter(RewriteOptions::kMobilize);
    DomainLawyer* lawyer = options_.WriteableDomainLawyer();
    if (suffix_mode == kProxySuffix) {
      lawyer->set_proxy_suffix(".suffix");
    } else {
      lawyer->AddOriginDomainMapping("example.com", "example.us",
                                     "example.com", &handler_);
    }
    fetcher_.reset(new IframeFetcher(&options_, &matcher_));
  }

  ResponseHeaders* FetchPage(StringPiece domain) {
    fetched_url_ = StrCat("http://", domain, "/foo?bar");
    fetcher_->Fetch(fetched_url_, &handler_, &fetch_);
    return fetch_.response_headers();
  }

 protected:
  UserAgentMatcher matcher_;
  RequestContextPtr request_context_;
  StringAsyncFetch fetch_;
  MockMessageHandler handler_;
  RewriteOptions options_;
  scoped_ptr<IframeFetcher> fetcher_;
  GoogleString fetched_url_;
};

TEST_F(IframeFetcherTest, IframeOnMobileProxySuffix) {
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kProxySuffix, kOnlyOnMobile);
  ResponseHeaders* response = FetchPage("example.com.suffix");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(
      fetch_.buffer(),
      ::testing::HasSubstr(kExpectedIframe));
}

TEST_F(IframeFetcherTest, RedirectOnOperaMiniProxySuffix) {
  InitTest(UserAgentMatcherTestBase::kOperaMiniMobileUserAgent,
           kProxySuffix, kOnlyOnMobile);
  ResponseHeaders* response = FetchPage("example.com.suffix");
  EXPECT_EQ(HttpStatus::kTemporaryRedirect, response->status_code());
  EXPECT_STREQ(kExpectedUrl,
               response->Lookup1(HttpAttributes::kLocation));
}

TEST_F(IframeFetcherTest, RedirectOnDesktopProxySuffix) {
  InitTest(UserAgentMatcherTestBase::kChrome42UserAgent,
           kProxySuffix, kOnlyOnMobile);
  ResponseHeaders* response = FetchPage("example.com.suffix");
  EXPECT_EQ(HttpStatus::kTemporaryRedirect, response->status_code());
  EXPECT_STREQ(kExpectedUrl,
               response->Lookup1(HttpAttributes::kLocation));
}

TEST_F(IframeFetcherTest, IframeOnDesktopProxySuffixWithAlwaysMobilize) {
  InitTest(UserAgentMatcherTestBase::kChrome42UserAgent,
           kProxySuffix, kOnAllDevices);
  ResponseHeaders* response = FetchPage("example.com.suffix");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(
      fetch_.buffer(),
      ::testing::HasSubstr(kExpectedIframe));
}

TEST_F(IframeFetcherTest, ErrorProxySuffix) {
  // Report an error for a configuration problem that results in the
  // domain being unmapped.
  InitTest(UserAgentMatcherTestBase::kOperaMiniMobileUserAgent,
           kProxySuffix, kOnlyOnMobile);
  ResponseHeaders* response = FetchPage("example.com");
  EXPECT_EQ(HttpStatus::kNotImplemented, response->status_code());
}

TEST_F(IframeFetcherTest, RedirectWhenDisabledProxySuffix) {
  options_.set_mob_iframe_disable(true);
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kProxySuffix, kOnlyOnMobile);
  ResponseHeaders* response = FetchPage("example.com.suffix");
  EXPECT_EQ(HttpStatus::kTemporaryRedirect, response->status_code());
  EXPECT_STREQ(kExpectedUrl,
               response->Lookup1(HttpAttributes::kLocation));
}

TEST_F(IframeFetcherTest, IframeOnMobileMapOrigin) {
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(
      fetch_.buffer(),
      ::testing::HasSubstr(kExpectedIframe));
}

TEST_F(IframeFetcherTest, RedirectOnOperaMiniMapOrigin) {
  InitTest(UserAgentMatcherTestBase::kOperaMiniMobileUserAgent,
           kMapOrigin, kOnlyOnMobile);
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kTemporaryRedirect, response->status_code());
  EXPECT_STREQ(kExpectedUrl,
               response->Lookup1(HttpAttributes::kLocation));
}

TEST_F(IframeFetcherTest, RedirectOnDesktopMapOrigin) {
  InitTest(UserAgentMatcherTestBase::kChrome42UserAgent,
           kMapOrigin, kOnlyOnMobile);
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kTemporaryRedirect, response->status_code());
  EXPECT_STREQ(kExpectedUrl,
               response->Lookup1(HttpAttributes::kLocation));
}

TEST_F(IframeFetcherTest, IframeOnDesktopMapOriginWithAlwaysMobilize) {
  InitTest(UserAgentMatcherTestBase::kChrome42UserAgent,
           kMapOrigin, kOnAllDevices);
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(
      fetch_.buffer(),
      ::testing::HasSubstr(kExpectedIframe));
}

TEST_F(IframeFetcherTest, RedirectOnNoScript) {
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  options_.DisableFiltersRequiringScriptExecution();
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kTemporaryRedirect, response->status_code());
  EXPECT_STREQ(kExpectedUrl,
               response->Lookup1(HttpAttributes::kLocation));
}

TEST_F(IframeFetcherTest, ErrorMapOrigin) {
  // Report an error for a configuration problem that results in the
  // domain being unmapped.
  InitTest(UserAgentMatcherTestBase::kOperaMiniMobileUserAgent,
           kMapOrigin, kOnlyOnMobile);
  ResponseHeaders* response = FetchPage("example.com");
  EXPECT_EQ(HttpStatus::kNotImplemented, response->status_code());
}

TEST_F(IframeFetcherTest, RedirectWhenDisabledMapOrigin) {
  options_.set_mob_iframe_disable(true);
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kTemporaryRedirect, response->status_code());
  EXPECT_STREQ(kExpectedUrl,
               response->Lookup1(HttpAttributes::kLocation));
}

TEST_F(IframeFetcherTest, Viewport) {
  // Verify default viewport
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(
      fetch_.buffer(),
      ::testing::HasSubstr("<meta name=\"viewport\" "
                           "content=\"width=device-width,initial-scale=1\">"));
}

TEST_F(IframeFetcherTest, ViewportNone) {
  options_.set_mob_iframe_viewport("none");
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(fetch_.buffer(),
              ::testing::Not(::testing::HasSubstr("<meta name=\"viewport\"")));
}

TEST_F(IframeFetcherTest, ViewportEscaped) {
  options_.set_mob_iframe_viewport("\">");
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(
      fetch_.buffer(),
      ::testing::HasSubstr("<meta name=\"viewport\" content=\"&quot;&gt;\">"));
}

}  // namespace

}  // namespace net_instaweb
