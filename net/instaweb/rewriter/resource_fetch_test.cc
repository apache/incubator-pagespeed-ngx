/*
 * Copyright 2012 Google Inc.
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

// Unit-tests for ResourceFetch

#include "net/instaweb/rewriter/public/resource_fetch.h"

#include "net/instaweb/http/public/sync_fetcher_adapter_callback.h"
#include "net/instaweb/http/public/wait_url_async_fetcher.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/thread/mock_scheduler.h"

namespace net_instaweb {

namespace {

const char kCssContent[] = "* { display: none; }";
const char kMinimizedCssContent[] = "*{display:none}";
const char kValue[] = "Value";

class ResourceFetchTest : public RewriteTestBase {
};

TEST_F(ResourceFetchTest, BlockingFetch) {
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCssContent, 100);

  // Make this actually happen asynchronously.
  SetupWaitFetcher();
  mock_scheduler()->AddAlarmAtUs(
      timer()->NowUs() + 100,
      MakeFunction(factory()->wait_url_async_fetcher(),
                   &WaitUrlAsyncFetcher::CallCallbacks));

  // Now fetch stuff.
  GoogleString buffer;
  StringWriter writer(&buffer);
  SyncFetcherAdapterCallback* callback =
      new SyncFetcherAdapterCallback(
          server_context()->thread_system(), &writer,
          CreateRequestContext());
  RewriteOptions* custom_options =
      server_context()->global_options()->Clone();

  GoogleString err;
  // Tell ResourceFetch to add a few response headers to its results.
  // Empty field name gets rejected.
  EXPECT_FALSE(custom_options->ValidateAndAddResourceHeader("", "Bar", &err));

  // Empty field value gets accepted.
  EXPECT_TRUE(custom_options->ValidateAndAddResourceHeader(
      "X-Foo-Empty", "", &err));

  // No control characters allowed in field name
  EXPECT_FALSE(custom_options->ValidateAndAddResourceHeader(
      "X-Foo\ncontinue", "Bar", &err));

  // No control characters allowed in field value
  EXPECT_FALSE(custom_options->ValidateAndAddResourceHeader(
      "X-Foo", "Bar\ncontinue", &err));

  // No separators should be accepted in the field name
  EXPECT_FALSE(custom_options->ValidateAndAddResourceHeader(
      "X-Fo;o", "Bar", &err));

  // Hop by hop headers should be refused.
  EXPECT_FALSE(custom_options->ValidateAndAddResourceHeader(
      "Connection", "close", &err));

  // Cache-control header should be refused.
  EXPECT_FALSE(custom_options->ValidateAndAddResourceHeader(
      "Cache-Control", "private", &err));

  // Request adding a reasonable header, which ResourceFetch should accept:
  EXPECT_TRUE(custom_options->ValidateAndAddResourceHeader(
      "X-Resource-Header", kValue, &err));

  // Separators should be accepted in the field value
  EXPECT_TRUE(custom_options->ValidateAndAddResourceHeader(
      "X-FooSeparator", "B; ar", &err));

  // Values should be trimmed.
  EXPECT_TRUE(custom_options->ValidateAndAddResourceHeader(
      "  X-FooTrim  ", "  Bar   ", &err));

  // Empty field value gets accepted.
  EXPECT_TRUE(custom_options->ValidateAndAddResourceHeader(
      "X-Foo-Spaced-Value", "aa bb", &err));

  RewriteDriver* custom_driver =
      server_context()->NewCustomRewriteDriver(
          custom_options,
          CreateRequestContext());

  GoogleUrl url(Encode(kTestDomain, "cf", "0", "a.css", "css"));
  EXPECT_TRUE(
      ResourceFetch::BlockingFetch(
          url, server_context(), custom_driver, callback));
  EXPECT_TRUE(callback->IsDone());
  EXPECT_TRUE(callback->success());

  // Validate our expectations w/regard to our earlier AddResponseHeader calls.
  EXPECT_FALSE(callback->response_headers()->Has(""));
  EXPECT_FALSE(callback->response_headers()->Has("X-Foo\ncontinue"));
  EXPECT_FALSE(callback->response_headers()->Has("X-Foo"));

  EXPECT_TRUE(callback->response_headers()->Has("X-Foo-Empty"));
  EXPECT_STREQ("", callback->response_headers()->Lookup1("X-Foo-Empty"));

  EXPECT_TRUE(callback->response_headers()->Has("X-Resource-Header"));
  EXPECT_STREQ(
      kValue, callback->response_headers()->Lookup1("X-Resource-Header"));

  EXPECT_TRUE(callback->response_headers()->Has("X-FooTrim"));
  EXPECT_STREQ("Bar", callback->response_headers()->Lookup1("X-FooTrim"));

  EXPECT_TRUE(callback->response_headers()->Has("X-Foo-Spaced-Value"));
  EXPECT_STREQ("aa bb", callback->response_headers()->Lookup1(
      "X-Foo-Spaced-Value"));

  callback->Release();

  EXPECT_EQ(kMinimizedCssContent, buffer);
}

TEST_F(ResourceFetchTest, BlockingFetchOfInvalidUrl) {
  // Fetch stuff.
  GoogleString buffer;
  StringWriter writer(&buffer);
  RewriteOptions* custom_options =
      server_context()->global_options()->Clone();
  custom_options->set_in_place_rewriting_enabled(false);
  RewriteDriver* custom_driver =
      server_context()->NewCustomRewriteDriver(
          custom_options, CreateRequestContext());

  GoogleString err;
  EXPECT_TRUE(custom_options->ValidateAndAddResourceHeader(
      "X-Resource-Header", kValue, &err));
  EXPECT_EQ("", err);

  SyncFetcherAdapterCallback* callback =
      new SyncFetcherAdapterCallback(
          server_context()->thread_system(), &writer, CreateRequestContext());

  // Encode an URL then invalidate it by removing the hash. This will cause
  // RewriteDriver::DecodeOutputResourceNameHelper to reject it, which will
  // cause RewriteDriver::FetchResource to fail to handle it, which will cause
  // StartWithDriver and then BlockingFetch to exit early.
  GoogleUrl url(Encode(kTestDomain, "cf", "deadbeef", "a.css", "css"));
  GoogleString url_str;
  url.Spec().CopyToString(&url_str);
  GlobalReplaceSubstring(".deadbeef.", "..", &url_str);
  url.Reset(url_str);

  // Prior to StartWithDriver checking if the fetch was actually initiated,
  // the call to BlockingFetch would block forever; now it returns immediately.
  EXPECT_FALSE(
      ResourceFetch::BlockingFetch(
          url, server_context(), custom_driver, callback));

  // Validate our expectations w/regard to our earlier AddResponseHeader calls
  // for responses to bad urls.
  EXPECT_TRUE(callback->IsDone());
  EXPECT_FALSE(callback->success());
  EXPECT_TRUE(callback->response_headers()->Has("X-Resource-Header"));
  EXPECT_STREQ(
      kValue, callback->response_headers()->Lookup1("X-Resource-Header"));
  callback->Release();

  EXPECT_EQ("", buffer);
}

}  // namespace

}  // namespace net_instaweb
