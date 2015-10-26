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
#include "net/instaweb/http/public/mock_url_fetcher.h"
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
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/http_options.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"
#include "pagespeed/kernel/http/user_agent_matcher_test_base.h"

namespace net_instaweb {
namespace {

const char kExpectedUrl[] = "http://example.com/foo?bar";
const char kRobotsContent[] = "robots.txt content";
const char kFaviconContent[] = "favicon content";

GoogleString ExpectedBody(StringPiece url, bool add_scrolling_attribute) {
  GoogleString scrolling_attribute =
      add_scrolling_attribute ? " scrolling=\"no\"" : "";
  return StrCat(
      "<body class=\"mob-iframe\">"
      "<div id=\"psmob-iframe-container\">"
      "<div id=\"psmob-spacer\"></div>"
      "<iframe id=\"psmob-iframe\" src=\"",
      url, "\"", scrolling_attribute, "></iframe></div></body>");
}

GoogleString ExpectedCanonical(StringPiece url) {
  return StrCat("<link rel=\"canonical\" href=\"", url, "\">");
}

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
    fetcher_.reset(new IframeFetcher(&options_, &matcher_, &mock_url_fetcher_));
  }

  void InitResources() {
    InitResource("http://example.us/robots.txt", kContentTypeText,
                 kRobotsContent);
    InitResource("http://example.us/robots.txt?foo", kContentTypeText,
                 kRobotsContent);
    InitResource("http://example.us/favicon.ico", kContentTypeIco,
                 kFaviconContent);
  }

  ResponseHeaders* FetchPage(StringPiece domain) {
    return FetchUrl(domain, "/foo?bar");
  }

  ResponseHeaders* FetchUrl(StringPiece domain, StringPiece url) {
    fetched_url_ = StrCat("http://", domain, url);
    fetcher_->Fetch(fetched_url_, &handler_, &fetch_);
    return fetch_.response_headers();
  }

  void InitResource(
      StringPiece resource_name,
      const ContentType& content_type,
      StringPiece content) {
    ResponseHeaders response_headers;
    response_headers.set_major_version(1);
    response_headers.set_minor_version(1);
    response_headers.SetStatusAndReason(HttpStatus::kOK);
    response_headers.Add(HttpAttributes::kContentType,
                         content_type.mime_type());
    response_headers.ComputeCaching();
    mock_url_fetcher_.SetResponse(resource_name, response_headers, content);
  }

 protected:
  UserAgentMatcher matcher_;
  RequestContextPtr request_context_;
  StringAsyncFetch fetch_;
  MockMessageHandler handler_;
  RewriteOptions options_;
  MockUrlFetcher mock_url_fetcher_;
  scoped_ptr<IframeFetcher> fetcher_;
  GoogleString fetched_url_;
};

TEST_F(IframeFetcherTest, IframeOnMobileProxySuffix) {
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kProxySuffix, kOnlyOnMobile);
  InitResources();
  ResponseHeaders* response = FetchPage("example.com.suffix");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  // If not explictly configured, there is a default width=device-width
  // viewport.
  EXPECT_THAT(fetch_.buffer(),
              ::testing::HasSubstr(ExpectedBody(
                  kExpectedUrl, false /* add_scrolling_attribute */)));
  EXPECT_THAT(
      fetch_.buffer(),
      ::testing::HasSubstr(ExpectedCanonical(kExpectedUrl)));
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
  InitResources();
  ResponseHeaders* response = FetchPage("example.com.suffix");
  EXPECT_EQ(HttpStatus::kTemporaryRedirect, response->status_code());
  EXPECT_STREQ(kExpectedUrl,
               response->Lookup1(HttpAttributes::kLocation));
}

TEST_F(IframeFetcherTest, IframeOnDesktopProxySuffixWithAlwaysMobilize) {
  InitTest(UserAgentMatcherTestBase::kChrome42UserAgent,
           kProxySuffix, kOnAllDevices);
  InitResources();
  ResponseHeaders* response = FetchPage("example.com.suffix");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(fetch_.buffer(),
              ::testing::HasSubstr(ExpectedBody(
                  kExpectedUrl, false /* add_scrolling_attribute */)));
  EXPECT_THAT(
      fetch_.buffer(),
      ::testing::HasSubstr(ExpectedCanonical(kExpectedUrl)));
}

TEST_F(IframeFetcherTest, ErrorProxySuffix) {
  // Report an error for a configuration problem that results in the
  // domain being unmapped.
  InitTest(UserAgentMatcherTestBase::kOperaMiniMobileUserAgent,
           kProxySuffix, kOnlyOnMobile);
  InitResources();
  ResponseHeaders* response = FetchPage("example.com");
  EXPECT_EQ(HttpStatus::kNotImplemented, response->status_code());
}

TEST_F(IframeFetcherTest, RedirectWhenDisabledProxySuffix) {
  options_.set_mob_iframe_disable(true);
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kProxySuffix, kOnlyOnMobile);
  InitResources();
  ResponseHeaders* response = FetchPage("example.com.suffix");
  EXPECT_EQ(HttpStatus::kTemporaryRedirect, response->status_code());
  EXPECT_STREQ(kExpectedUrl,
               response->Lookup1(HttpAttributes::kLocation));
}

TEST_F(IframeFetcherTest, IframeOnMobileMapOrigin) {
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  InitResources();
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(fetch_.buffer(),
              ::testing::HasSubstr(ExpectedBody(
                  kExpectedUrl, false /* add_scrolling_attribute */)));
  EXPECT_THAT(
      fetch_.buffer(),
      ::testing::HasSubstr(ExpectedCanonical(kExpectedUrl)));
}

TEST_F(IframeFetcherTest, RedirectOnOperaMiniMapOrigin) {
  InitTest(UserAgentMatcherTestBase::kOperaMiniMobileUserAgent,
           kMapOrigin, kOnlyOnMobile);
  InitResources();
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kTemporaryRedirect, response->status_code());
  EXPECT_STREQ(kExpectedUrl,
               response->Lookup1(HttpAttributes::kLocation));
}

TEST_F(IframeFetcherTest, RedirectOnDesktopMapOrigin) {
  InitTest(UserAgentMatcherTestBase::kChrome42UserAgent,
           kMapOrigin, kOnlyOnMobile);
  InitResources();
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kTemporaryRedirect, response->status_code());
  EXPECT_STREQ(kExpectedUrl,
               response->Lookup1(HttpAttributes::kLocation));
}

TEST_F(IframeFetcherTest, IframeOnDesktopMapOriginWithAlwaysMobilize) {
  InitTest(UserAgentMatcherTestBase::kChrome42UserAgent,
           kMapOrigin, kOnAllDevices);
  InitResources();
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(fetch_.buffer(),
              ::testing::HasSubstr(ExpectedBody(
                  kExpectedUrl, false /* add_scrolling_attribute */)));
  EXPECT_THAT(
      fetch_.buffer(),
      ::testing::HasSubstr(ExpectedCanonical(kExpectedUrl)));
}

TEST_F(IframeFetcherTest, RedirectOnNoScript) {
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  InitResources();
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
  InitResources();
  ResponseHeaders* response = FetchPage("example.com");
  EXPECT_EQ(HttpStatus::kNotImplemented, response->status_code());
}

TEST_F(IframeFetcherTest, RedirectWhenDisabledMapOrigin) {
  options_.set_mob_iframe_disable(true);
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  InitResources();
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kTemporaryRedirect, response->status_code());
  EXPECT_STREQ(kExpectedUrl,
               response->Lookup1(HttpAttributes::kLocation));
}

TEST_F(IframeFetcherTest, Viewport) {
  // Verify default viewport
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  InitResources();
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(
      fetch_.buffer(),
      ::testing::HasSubstr("<meta name=\"viewport\" "
                           "content=\"width=device-width,initial-scale=1\">"));
  // Verify that the scrolling attribute is not set on the iframe, since this is
  // an android user agent.
  EXPECT_THAT(fetch_.buffer(),
              ::testing::HasSubstr(ExpectedBody(
                  kExpectedUrl, false /* add_scrolling_attribute */)));
}

TEST_F(IframeFetcherTest, ViewportNone) {
  options_.set_mob_iframe_viewport("none");
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  InitResources();
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(fetch_.buffer(),
              ::testing::Not(::testing::HasSubstr("<meta name=\"viewport\"")));
  // Verify that the scrolling attribute is not set on the iframe.
  EXPECT_THAT(fetch_.buffer(),
              ::testing::Not(::testing::HasSubstr("scrolling=\"no\"")));
}

TEST_F(IframeFetcherTest, ViewportEscaped) {
  options_.set_mob_iframe_viewport("\">");
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  InitResources();
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(
      fetch_.buffer(),
      ::testing::HasSubstr("<meta name=\"viewport\" content=\"&quot;&gt;\">"));
  EXPECT_THAT(fetch_.buffer(),
              ::testing::Not(::testing::HasSubstr("scrolling=\"no\"")));
}

TEST_F(IframeFetcherTest, ViewportIOsSafari) {
  InitTest(UserAgentMatcherTestBase::kIPhone4Safari, kMapOrigin, kOnlyOnMobile);
  InitResources();
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(
      fetch_.buffer(),
      ::testing::HasSubstr("<meta name=\"viewport\" "
                           "content=\"width=device-width,initial-scale=1\">"));
  // Verify that the scrolling attribute is set on the iframe.
  EXPECT_THAT(fetch_.buffer(),
              ::testing::HasSubstr(ExpectedBody(
                  kExpectedUrl, true /* add_scrolling_attribute */)));
}

TEST_F(IframeFetcherTest, ViewportNoneIOsSafari) {
  options_.set_mob_iframe_viewport("none");
  InitTest(UserAgentMatcherTestBase::kIPhone4Safari, kMapOrigin, kOnlyOnMobile);
  InitResources();
  ResponseHeaders* response = FetchPage("example.us");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(fetch_.buffer(),
              ::testing::Not(::testing::HasSubstr("<meta name=\"viewport\"")));
  // Verify that the scrolling attribute is set on the iframe.
  EXPECT_THAT(fetch_.buffer(),
              ::testing::HasSubstr(ExpectedBody(
                  kExpectedUrl, false /* add_scrolling_attribute */)));
}

TEST_F(IframeFetcherTest, Robots) {
  options_.set_mob_iframe_viewport("\">");
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  InitResources();
  ResponseHeaders* response = FetchUrl("example.us", "/robots.txt");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_STREQ(kRobotsContent, fetch_.buffer());
  EXPECT_STREQ("text/plain", response->Lookup1(HttpAttributes::kContentType));
}

TEST_F(IframeFetcherTest, Robots404) {
  options_.set_mob_iframe_viewport("\">");
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  // Do not call InitResources()
  mock_url_fetcher_.set_fail_on_unexpected(false);
  ResponseHeaders* response = FetchUrl("example.us", "/robots.txt");
  EXPECT_EQ(HttpStatus::kNotFound, response->status_code());
}

TEST_F(IframeFetcherTest, RobotsWithQuery) {
  options_.set_mob_iframe_viewport("\">");
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  InitResources();
  ResponseHeaders* response = FetchUrl("example.us", "/robots.txt?foo");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_STREQ(kRobotsContent, fetch_.buffer());
  EXPECT_STREQ("text/plain", response->Lookup1(HttpAttributes::kContentType));
}

// Robots.txt must be at top level.  Requests for robots.txt in a
// subdirectory will be iframed.
TEST_F(IframeFetcherTest, RobotsInSubdir) {
  options_.set_mob_iframe_viewport("\">");
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  InitResources();
  ResponseHeaders* response = FetchUrl("example.us", "/foo/robots.txt");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_THAT(fetch_.buffer(), ::testing::HasSubstr(ExpectedBody(
                                   "http://example.com/foo/robots.txt",
                                   false /* add_scrolling_attribute */)));
  EXPECT_STREQ("text/html", response->Lookup1(HttpAttributes::kContentType));
}

TEST_F(IframeFetcherTest, Favicon) {
  options_.set_mob_iframe_viewport("\">");
  InitTest(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
           kMapOrigin, kOnlyOnMobile);
  InitResources();
  ResponseHeaders* response = FetchUrl("example.us", "/favicon.ico");
  EXPECT_EQ(HttpStatus::kOK, response->status_code());
  EXPECT_STREQ(kFaviconContent, fetch_.buffer());
  EXPECT_STREQ("image/x-icon", response->Lookup1(HttpAttributes::kContentType));
}

}  // namespace

}  // namespace net_instaweb
