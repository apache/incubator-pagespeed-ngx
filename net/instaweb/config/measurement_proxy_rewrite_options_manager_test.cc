/*
 * Copyright 2016 Google Inc.
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

#include "net/instaweb/config/measurement_proxy_rewrite_options_manager.h"

#include <memory>

#include "net/instaweb/rewriter/public/measurement_proxy_url_namer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "pagespeed/automatic/proxy_interface_test_base.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {
namespace {

const char kJsUrl[] = "script.js";
const char kHtmlUrl[] = "page.html";

class MeasurementProxyRewriteOptionsManagerTest
    : public ProxyInterfaceTestBase {
 protected:
  void SetUp() override {
    ProxyInterfaceTestBase::SetUp();
    options_manager_ = new MeasurementProxyRewriteOptionsManager(
        server_context(), "https://www.example.com", "secret");
    url_namer_.reset(new MeasurementProxyUrlNamer(
        "https://www.example.com", "secret"));
    server_context()->SetRewriteOptionsManager(options_manager_);
    server_context()->set_url_namer(url_namer_.get());
  }

  void RunGetRewriteOptionsTest(StringPiece url_string) {
    // Cleanup between runs.
    reported_options_.reset();

    url_string.CopyToString(&test_url_);
    GoogleUrl url(url_string);
    ASSERT_TRUE(url.IsWebValid());
    RequestHeaders request_headers;
    options_manager_->GetRewriteOptions(
        url, request_headers,
        NewCallback(this,
                    &MeasurementProxyRewriteOptionsManagerTest::GotOptions));
    ASSERT_NE(reported_options_.get(), nullptr);
  }

  void GotOptions(RewriteOptions* options) {
    reported_options_.reset(options);
  }

  void Expect403(StringPiece base_url) {
    EXPECT_TRUE(reported_options_->reject_blacklisted())
        << test_url_;
    EXPECT_FALSE(reported_options_->IsAllowed("http://www.modpagespeed.com"))
        << test_url_;
    SetupResources(base_url);
    Fetch(StrCat(test_url_, kHtmlUrl), false);
  }

  void ExpectPermitted() {
    EXPECT_FALSE(reported_options_->reject_blacklisted())
        << test_url_;
    EXPECT_TRUE(reported_options_->IsAllowed("http://www.modpagespeed.com"))
        << test_url_;
  }

  void ExpectBlocking() {
    EXPECT_EQ(-1, reported_options_->rewrite_deadline_ms())
        << test_url_;
    EXPECT_TRUE(reported_options_->in_place_wait_for_optimized())
        << test_url_;
    EXPECT_EQ(-1, reported_options_->in_place_rewrite_deadline_ms())
        << test_url_;
  }

  void ExpectNonBlocking() {
    EXPECT_LT(0, reported_options_->rewrite_deadline_ms())
        << test_url_;
    EXPECT_FALSE(reported_options_->in_place_wait_for_optimized())
        << test_url_;
    EXPECT_LT(0, reported_options_->in_place_rewrite_deadline_ms())
        << test_url_;
  }

  // base_url is where the resources are located, e.g. in the decoded form.
  // test_url_ has the external one, which is used for fetch.
  void ExpectRewrites(StringPiece base_url) {
    EXPECT_EQ(RewriteOptions::kCoreFilters, reported_options_->level())
        << test_url_;

    SetupResources(base_url);
    GoogleString out = Fetch(StrCat(test_url_, kHtmlUrl), true);
    // Inlined and minified.
    EXPECT_EQ("<head/><script>var a=42</script>", out)
        << test_url_;
  }

  void ExpectPassthrough(StringPiece base_url) {
    EXPECT_EQ(RewriteOptions::kPassThrough, reported_options_->level())
        << test_url_;

    SetupResources(base_url);
    GoogleString out = Fetch(StrCat(test_url_, kHtmlUrl), true);
    // Unchanged.
    EXPECT_EQ(StrCat("<script src=", kJsUrl, "></script>"), out)
        << test_url_;
  }

  void SetupResources(StringPiece base_url) {
    GoogleString normalized_base_url = base_url.as_string();
    LowerString(&normalized_base_url);
    SetResponseWithDefaultHeaders(
        StrCat(normalized_base_url, kJsUrl), kContentTypeJavascript,
        " var a  =   42", 100);
    SetResponseWithDefaultHeaders(
        StrCat(normalized_base_url, kHtmlUrl), kContentTypeHtml,
        StrCat("<script src=", kJsUrl, "></script>"), 100);
  }

  GoogleString Fetch(const GoogleString& url, bool expect_200) {
    GoogleString out;
    ResponseHeaders headers_out;
    RequestHeaders request_headers;

    FetchFromProxy(url, request_headers, true, &out, &headers_out,
                   false /* not doing pcache stuff */);
    EXPECT_EQ(headers_out.status_code(),
              expect_200 ? HttpStatus::kOK : HttpStatus::kForbidden);
    return out;
  }

  GoogleString test_url_;
  std::unique_ptr<RewriteOptions> reported_options_;

  // Owned by proxy_interface_->server_context()
  MeasurementProxyRewriteOptionsManager* options_manager_;
  std::unique_ptr<UrlNamer> url_namer_;
};

TEST_F(MeasurementProxyRewriteOptionsManagerTest, GetRewriteOptions) {
  // Not in the right form -- rejected.
  RunGetRewriteOptionsTest(
      "https://www.example.com/_/b/secret/www.google.com/");
  Expect403("http://www.google.com/");

  // Not on our origin (wrong schema, in this case)
  RunGetRewriteOptionsTest("http://www.example.com/h/b/secret/www.google.com/");
  Expect403("http://www.google.com/");

  // Wrong password, rejected.
  RunGetRewriteOptionsTest("https://www.example.com/h/b/tea/www.google.com/");
  Expect403("http://www.google.com/");

  // Reasonable one, blocking, same domain.
  RunGetRewriteOptionsTest(
      "https://www.example.com/h/b/secret/www.google.com/");
  ExpectPermitted();
  ExpectBlocking();
  ExpectRewrites("http://www.google.com/");

  // Cross-domain, but related domain, which is also allowing rewriting.
  // (Also make sure we properly handle these as case-insensitive).
  RunGetRewriteOptionsTest(
      "https://www.example.com/x/b/secret/www.google.com/foo.Google.com/");
  ExpectPermitted();
  ExpectBlocking();
  ExpectRewrites("http://foo.Google.com/");

  // No rewriting on domain-unrelated stuff.
  RunGetRewriteOptionsTest(
      "https://www.example.com/x/b/secret/www.google.com/foo.example.com/");
  ExpectPermitted();
  ExpectBlocking();
  ExpectPassthrough("http://foo.example.com/");

  // Now non-blocking versions.
  RunGetRewriteOptionsTest(
      "https://www.example.com/h/_/secret/www.google.com/");
  ExpectPermitted();
  ExpectNonBlocking();
  ExpectRewrites("http://www.google.com/");

  // With empty config, too.
  RunGetRewriteOptionsTest(
      "https://www.example.com/h//secret/www.google.com/");
  ExpectPermitted();
  ExpectNonBlocking();
  ExpectRewrites("http://www.google.com/");

  RunGetRewriteOptionsTest(
      "https://www.example.com/x/_/secret/www.google.com/foo.google.com/");
  ExpectPermitted();
  ExpectNonBlocking();
  ExpectRewrites("http://foo.google.com/");

  RunGetRewriteOptionsTest(
      "https://www.example.com/x/_/secret/www.google.com/foo.example.com/");
  ExpectPermitted();
  ExpectNonBlocking();
  ExpectPassthrough("http://foo.example.com/");
}

}  // namespace
}  // namespace net_instaweb
