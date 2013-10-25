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
// Unit tests for UserAgentSensitiveTestFetcher

#include "net/instaweb/http/public/ua_sensitive_test_fetcher.h"

#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

namespace {

const char kRoboto[] = "http://fonts.googleapis.com/css?family=Roboto";
const char kRobotoSsl[] = "https://fonts.googleapis.com/css?family=Roboto";

class UserAgentSensitiveTestFetcherTest : public ::testing::Test {
 protected:
  UserAgentSensitiveTestFetcherTest()
      : timer_(MockTimer::kApr_5_2010_ms),
        ua_sensitive_fetcher_(&mock_fetcher_) {
  }

  virtual void SetUp() {
    // Font loader CSS gets Cache-Control:private, max-age=86400
    ResponseHeaders response_headers;
    response_headers.SetStatusAndReason(HttpStatus::kOK);
    response_headers.Add(HttpAttributes::kContentType,
                         kContentTypeCss.mime_type());
    response_headers.SetDateAndCaching(
        timer_.NowMs(), 86400 * Timer::kSecondMs, ", private");

    // Set them up in spots where UaSensitiveFetcher would direct them.
    mock_fetcher_.SetResponse(StrCat(kRoboto, "&UA=Chromezilla"),
                              response_headers, "font_chromezilla");

    mock_fetcher_.SetResponse(StrCat(kRoboto, "&UA=Safieri"),
                              response_headers, "font_safieri");

    mock_fetcher_.SetResponse(StrCat(kRobotoSsl, "&UA=Chromezilla"),
                              response_headers, "sfont_chromezilla");

    mock_fetcher_.SetResponse(StrCat(kRobotoSsl, "&UA=Safieri"),
                              response_headers, "sfont_safieri");
  }

  MockTimer timer_;
  GoogleMessageHandler handler_;
  MockUrlFetcher mock_fetcher_;
  UserAgentSensitiveTestFetcher ua_sensitive_fetcher_;
};

TEST_F(UserAgentSensitiveTestFetcherTest, BasicOperation) {
  scoped_ptr<ThreadSystem> ts(Platform::CreateThreadSystem());

  RequestContextPtr request_context(
      RequestContext::NewTestRequestContext(ts.get()));
  // First attempts to fetch should fail due to lack of domain authorization.
  ExpectStringAsyncFetch evil_chromezilla_fetch(false, request_context);
  evil_chromezilla_fetch.request_headers()->Add(
      HttpAttributes::kUserAgent, "Chromezilla");

  ua_sensitive_fetcher_.Fetch(kRoboto, &handler_, &evil_chromezilla_fetch);
  EXPECT_TRUE(evil_chromezilla_fetch.done());
  EXPECT_FALSE(evil_chromezilla_fetch.success());

  // Now authorized both fonts hosts.
  request_context->AddSessionAuthorizedFetchOrigin(
      "http://fonts.googleapis.com");
  request_context->AddSessionAuthorizedFetchOrigin(
      "https://fonts.googleapis.com");

  ExpectStringAsyncFetch chromezilla_fetch(true, request_context);
  chromezilla_fetch.request_headers()->Add(
      HttpAttributes::kUserAgent, "Chromezilla");

  ua_sensitive_fetcher_.Fetch(kRoboto, &handler_, &chromezilla_fetch);
  EXPECT_TRUE(chromezilla_fetch.done());
  EXPECT_EQ("font_chromezilla", chromezilla_fetch.buffer());

  // Now over "SSL"
  chromezilla_fetch.Reset();
  chromezilla_fetch.request_headers()->Add(
      HttpAttributes::kUserAgent, "Chromezilla");
  ua_sensitive_fetcher_.Fetch(kRobotoSsl, &handler_, &chromezilla_fetch);
  EXPECT_TRUE(chromezilla_fetch.done());
  EXPECT_EQ("sfont_chromezilla", chromezilla_fetch.buffer());

  // Same for the other "UA"
  ExpectStringAsyncFetch safieri_fetch(true, request_context);
  safieri_fetch.request_headers()->Add(
      HttpAttributes::kUserAgent, "Safieri");

  ua_sensitive_fetcher_.Fetch(kRoboto, &handler_, &safieri_fetch);
  EXPECT_TRUE(safieri_fetch.done());
  EXPECT_EQ("font_safieri", safieri_fetch.buffer());

  // Now over "SSL"
  safieri_fetch.Reset();
  safieri_fetch.request_headers()->Add(
      HttpAttributes::kUserAgent, "Safieri");
  ua_sensitive_fetcher_.Fetch(kRobotoSsl, &handler_, &safieri_fetch);
  EXPECT_TRUE(safieri_fetch.done());
  EXPECT_EQ("sfont_safieri", safieri_fetch.buffer());
}

}  // namespace

}  // namespace net_instaweb
