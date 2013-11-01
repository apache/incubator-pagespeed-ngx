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

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/ua_sensitive_test_fetcher.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/timer.h"

namespace net_instaweb {

namespace {

const char kRoboto[] = "http://fonts.googleapis.com/css?family=Roboto";

class GoogleFontCssInlineFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    SetUpForFontFilterTest();
  }

  void SetUpForFontFilterTest() {
    AddFilter(RewriteOptions::kInlineGoogleFontCss);

    // We want to enable debug after AddFilters, since we don't want the
    // standard debug filter output, just that from the font filter.
    options()->ClearSignatureForTesting();
    options()->EnableFilter(RewriteOptions::kDebug);
    server_context()->ComputeSignature(options());

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
  }
};

TEST_F(GoogleFontCssInlineFilterTest, BasicOperation) {
  rewrite_driver()->SetUserAgent("Chromezilla");
  ValidateExpected("simple",
                   CssLinkHref(kRoboto),
                   "<style>font_chromezilla</style>");

  // Different UAs get different cache entries
  rewrite_driver()->SetUserAgent("Safieri");
  ValidateExpected("simple2",
                   CssLinkHref(kRoboto),
                   "<style>font_safieri</style>");
}

TEST_F(GoogleFontCssInlineFilterTest, UsageRestrictions) {
  rewrite_driver()->SetUserAgent("Chromezilla");

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

class GoogleFontCssInlineFilterSizeLimitTest
    : public GoogleFontCssInlineFilterTest {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();  // skipped base on purpose
    // GoogleFontCssInlineFilter honors css_inline_max_bytes.
    // Set a threshold at font_safieri, which should prevent longer
    // font_chromezilla from inlining.
    options()->set_css_inline_max_bytes(STATIC_STRLEN("font_safieri"));
    SetUpForFontFilterTest();
  }
};

TEST_F(GoogleFontCssInlineFilterSizeLimitTest, SizeLimit) {
  rewrite_driver()->SetUserAgent("Chromezilla");
  ValidateNoChanges("slightly_long", CssLinkHref(kRoboto));

  rewrite_driver()->SetUserAgent("Safieri");
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
    SetUpForFontFilterTest();
  }
};

TEST_F(GoogleFontCssInlineFilterAndImportTest, ViaInlineImport) {
  // Tests to make sure that if InlineImportToLink filter is on we
  // convert a <style>@import'ing </style> this as well.
  rewrite_driver()->SetUserAgent("Chromezilla");
  ValidateExpected("import",
                   StringPrintf("<style>@import \"%s\";</style>", kRoboto),
                   "<style>font_chromezilla</style>");

  rewrite_driver()->SetUserAgent("Safieri");
  ValidateExpected("import",
                   StringPrintf("<style>@import \"%s\";</style>", kRoboto),
                   "<style>font_safieri</style>");
}

}  // namespace

}  // namespace net_instaweb
