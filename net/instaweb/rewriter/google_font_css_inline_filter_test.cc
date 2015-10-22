/*
 * Copyright 2013 Google Inc.
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
// Unit tests for GoogleFontCssInlineFilter

#include "net/instaweb/http/public/ua_sensitive_test_fetcher.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/opt/http/request_context.h"

namespace net_instaweb {

namespace {

const char kRoboto[] = "http://fonts.googleapis.com/css?family=Roboto";

class GoogleFontCssInlineFilterTestBase : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
  }

  void SetUpForFontFilterTest(RewriteOptions::Filter filter_to_enable) {
    AddFilter(filter_to_enable);

    // We want to enable debug after AddFilters, since we don't want the
    // standard debug filter output, just that from the font filter.
    options()->ClearSignatureForTesting();
    options()->EnableFilter(RewriteOptions::kDebug);
    server_context()->ComputeSignature(options());
  }

  void ResetUserAgent(StringPiece user_agent) {
    ClearRewriteDriver();
    rewrite_driver()->SetSessionFetcher(
        new UserAgentSensitiveTestFetcher(rewrite_driver()->async_fetcher()));

    // Font loader CSS gets Cache-Control:private, max-age=86400
    ResponseHeaders response_headers;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &response_headers);
    response_headers.SetDateAndCaching(
        timer()->NowMs(), 86400 * Timer::kSecondMs, ", private");

    // Now upload some UA-specific CSS where UaSensitiveFetcher will find it,
    // for two fake UAs: Chromezilla and Safieri, used since they are short,
    // unlike real UA strings.
    SetFetchResponse(StrCat(kRoboto, "&UA=Chromezilla"),
                     response_headers, "font_chromezilla");

    SetFetchResponse(StrCat(kRoboto, "&UA=Safieri"),
                     response_headers, "font_safieri");

    // If other filters will try to fetch this, they won't have a UA.
    SetFetchResponse(StrCat(kRoboto, "&UA=unknown"),
                     response_headers, "font_huh");
    SetCurrentUserAgent(user_agent);
  }
};

class GoogleFontCssInlineFilterTest : public GoogleFontCssInlineFilterTestBase {
 protected:
  virtual void SetUp() {
    GoogleFontCssInlineFilterTestBase::SetUp();
    SetUpForFontFilterTest(RewriteOptions::kInlineGoogleFontCss);
  }
};

TEST_F(GoogleFontCssInlineFilterTest, BasicOperation) {
  ResetUserAgent("Chromezilla");
  ValidateExpected("simple",
                   CssLinkHref(kRoboto),
                   "<style>font_chromezilla</style>");

  // Different UAs get different cache entries
  ResetUserAgent("Safieri");
  ValidateExpected("simple2",
                   CssLinkHref(kRoboto),
                   "<style>font_safieri</style>");
}

TEST_F(GoogleFontCssInlineFilterTest, UsageRestrictions) {
  ResetUserAgent("Chromezilla");

  options()->ClearSignatureForTesting();
  options()->set_modify_caching_headers(false);
  server_context()->ComputeSignature(options());
  ValidateExpected("incompat1",
                   CssLinkHref(kRoboto),
                   StrCat(CssLinkHref(kRoboto),
                          "<!--Cannot inline font loader CSS when "
                          "ModifyCachingHeaders is off-->"));

  options()->ClearSignatureForTesting();
  options()->set_modify_caching_headers(true);
  options()->set_downstream_cache_purge_location_prefix("foo");
  server_context()->ComputeSignature(options());
  ValidateExpected("incompat2",
                   CssLinkHref(kRoboto),
                   StrCat(CssLinkHref(kRoboto),
                          "<!--Cannot inline font loader CSS when "
                          "using downstream cache-->"));
}

TEST_F(GoogleFontCssInlineFilterTest, ProtocolRelative) {
  ResetUserAgent("Chromezilla");
  ValidateExpected("proto_rel",
                   CssLinkHref("//fonts.googleapis.com/css?family=Roboto"),
                   "<style>font_chromezilla</style>");
}

class GoogleFontCssInlineFilterSizeLimitTest
    : public GoogleFontCssInlineFilterTestBase {
 protected:
  virtual void SetUp() {
    GoogleFontCssInlineFilterTestBase::SetUp();
    // GoogleFontCssInlineFilter uses google_font_css_inline_max_bytes.
    // Set a threshold at font_safieri, which should prevent longer
    // font_chromezilla from inlining.
    options()->set_google_font_css_inline_max_bytes(
        STATIC_STRLEN("font_safieri"));
    SetUpForFontFilterTest(RewriteOptions::kInlineGoogleFontCss);
  }
};

TEST_F(GoogleFontCssInlineFilterSizeLimitTest, SizeLimit) {
  ResetUserAgent("Chromezilla");
  ValidateExpected(
      "slightly_long",
      CssLinkHref(kRoboto),
      StrCat(CssLinkHref(kRoboto),
             "<!--CSS not inlined since it&#39;s bigger than 12 bytes-->"));

  ResetUserAgent("Safieri");
  ValidateExpected("short",
                   CssLinkHref(kRoboto),
                   "<style>font_safieri</style>");
}

class GoogleFontCssInlineFilterAndImportTest
    : public GoogleFontCssInlineFilterTest {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();  // skipped base on purpose
    options()->EnableFilter(
        RewriteOptions::RewriteOptions::kInlineImportToLink);
    SetUpForFontFilterTest(RewriteOptions::kInlineGoogleFontCss);
  }
};

TEST_F(GoogleFontCssInlineFilterAndImportTest, ViaInlineImport) {
  // Tests to make sure that if InlineImportToLink filter is on we
  // convert a <style>@import'ing </style> this as well.
  ResetUserAgent("Chromezilla");
  ValidateExpected("import",
                   StringPrintf("<style>@import \"%s\";</style>", kRoboto),
                   "<style>font_chromezilla</style>");

  ResetUserAgent("Safieri");
  ValidateExpected("import",
                   StringPrintf("<style>@import \"%s\";</style>", kRoboto),
                   "<style>font_safieri</style>");
}

class GoogleFontCssInlineFilterAndWidePermissionsTest
    : public GoogleFontCssInlineFilterTestBase {
  virtual void SetUp() {
    GoogleFontCssInlineFilterTestBase::SetUp();
    // Check that we don't rely solely on authorization to properly
    // dispatch the URL to us.
    options()->WriteableDomainLawyer()->AddDomain("*", message_handler());
    rewrite_driver()->request_context()->AddSessionAuthorizedFetchOrigin(
        "http://fonts.googleapis.com");
    options()->EnableFilter(RewriteOptions::kInlineCss);
    SetUpForFontFilterTest(RewriteOptions::kInlineGoogleFontCss);
  }
};

TEST_F(GoogleFontCssInlineFilterAndWidePermissionsTest, WithWideAuthorization) {
  ResetUserAgent("Chromezilla");
  ValidateExpected("with_domain_*",
                   CssLinkHref(kRoboto),
                   "<style>font_chromezilla</style>");
}

// Negative test for the above, with font filter off, to make sure
// it's not inline_css doing stuff.
class NoGoogleFontCssInlineFilterAndWidePermissionsTest
    : public GoogleFontCssInlineFilterTestBase {
 protected:
  void SetupForAuthorizedFetchOrigin() {
    // Check that we don't rely solely on authorization to properly
    // dispatch the URL to us. Note that we can't only use DomainLawyer here
    // since UserAgentSensitiveTestFetcher is at http layer so is simply
    // unaware of it.
    options()->WriteableDomainLawyer()->AddDomain("*", message_handler());
    rewrite_driver()->request_context()->AddSessionAuthorizedFetchOrigin(
        "http://fonts.googleapis.com");
    SetUpForFontFilterTest(RewriteOptions::kInlineCss);
  }
};

TEST_F(NoGoogleFontCssInlineFilterAndWidePermissionsTest,
       WithWideAuthorization) {
  // Since font inlining isn't on, the regular inliner complains. This isn't
  // ideal, but doing otherwise requires inline_css to know about
  // inline_google_font_css, which also seems suboptimal.
  ResetUserAgent("Chromezilla");
  SetupForAuthorizedFetchOrigin();
  ValidateExpected("with_domain_*_without_font_filter", CssLinkHref(kRoboto),
                   StrCat(CssLinkHref(kRoboto),
                          "<!--Uncacheable content, preventing rewriting of "
                          "http://fonts.googleapis.com/css?family=Roboto-->"));
}

}  // namespace

}  // namespace net_instaweb
