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
// Author: mmohabey@google.com (Megha Mohabey)

#include "net/instaweb/rewriter/public/suppress_prehead_filter.h"

#include "net/instaweb/http/http.pb.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/meta_data.h"  // for HttpAttributes, etc
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/flush_early_info_finder_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {
const int64 kOriginTtlSec = 12000;
const char kJsData[] =
    "alert     (    'hello, world!'    ) "
    " /* removed */ <!-- removed --> "
    " // single-line-comment";
}  // namespace

class SuppressPreheadFilterTest : public RewriteTestBase {
 public:
  SuppressPreheadFilterTest() : writer_(&output_) {}

  virtual bool AddHtmlTags() const { return false; }

 protected:
  void InitResources() {
    SetResponseWithDefaultHeaders("http://test.com/a.css", kContentTypeCss,
                                  " a ", kOriginTtlSec);
    SetResponseWithDefaultHeaders("http://test.com/b.js",
                                  kContentTypeJavascript, kJsData,
                                  kOriginTtlSec);
  }

  virtual void SetUp() {
    options()->ClearSignatureForTesting();
    options()->EnableFilter(RewriteOptions::kFlushSubresources);
    // Disable support no script, so that we don't insert the noscript node and
    // the output is simple.
    options()->set_support_noscript_enabled(false);
    options()->ComputeSignature(hasher());
    RewriteTestBase::SetUp();
    rewrite_driver()->AddFilters();
    rewrite_driver()->SetWriter(&writer_);
    headers_.Clear();
    rewrite_driver()->set_response_headers_ptr(&headers_);
    rewrite_driver()->set_user_agent("prefetch_link_rel_subresource");
  }

  GoogleString output_;

  ResponseHeaders* headers() {
    return &headers_;
  }

  void VerifyCharset(GoogleString content_type) {
    FlushEarlyInfo* flush_early_info = rewrite_driver()->flush_early_info();
    HttpResponseHeaders headers = flush_early_info->response_headers();
    GoogleString val;
    for (int i = 0; i < headers.header_size(); ++i) {
      if (headers.header(i).name().compare(HttpAttributes::kContentType) == 0) {
        val = headers.header(i).value();
        break;
      }
    }
    EXPECT_STREQ(val, content_type);
  }

  void CallUpdateFetchLatencyInFlushEarlyProto(double latency) {
    SuppressPreheadFilter::UpdateFetchLatencyInFlushEarlyProto(
        latency, rewrite_driver_);
  }

 private:
  StringWriter writer_;
  ResponseHeaders headers_;

  DISALLOW_COPY_AND_ASSIGN(SuppressPreheadFilterTest);
};

TEST_F(SuppressPreheadFilterTest, UpdateFetchLatencyInFlushEarlyProto) {
  EXPECT_FALSE(
      rewrite_driver_->flush_early_info()->has_last_n_fetch_latencies());
  EXPECT_FALSE(
      rewrite_driver_->flush_early_info()->has_average_fetch_latency_ms());
  // When there is no entry.
  CallUpdateFetchLatencyInFlushEarlyProto(100);
  EXPECT_EQ("100",
            rewrite_driver_->flush_early_info()->last_n_fetch_latencies());
  EXPECT_EQ(100,
            rewrite_driver_->flush_early_info()->average_fetch_latency_ms());

  // When less than 10 entries exists.
  CallUpdateFetchLatencyInFlushEarlyProto(150);
  EXPECT_EQ("150,100",
            rewrite_driver_->flush_early_info()->last_n_fetch_latencies());
  EXPECT_EQ(125,
            rewrite_driver_->flush_early_info()->average_fetch_latency_ms());

  // When there are 10 entries.
  rewrite_driver_->flush_early_info()->set_last_n_fetch_latencies(
      "95,96,97,98,99,101,102,103,104,105");
  rewrite_driver_->flush_early_info()->set_average_fetch_latency_ms(100);
  CallUpdateFetchLatencyInFlushEarlyProto(205);
  EXPECT_EQ("205,95,96,97,98,99,101,102,103,104",
            rewrite_driver_->flush_early_info()->last_n_fetch_latencies());
  EXPECT_EQ(110,
            rewrite_driver_->flush_early_info()->average_fetch_latency_ms());
}

TEST_F(SuppressPreheadFilterTest, FlushEarlyHeadSuppress) {
  InitResources();
  const char pre_head_input[] = "<!DOCTYPE html><html>";
  const char post_head_input[] =
      "<head>"
        "<link type=\"text/css\" rel=\"stylesheet\""
        " href=\"http://test.com/a.css\"/>"
        "<script src=\"http://test.com/b.js\"></script>"
      "</head>"
      "<body></body></html>";
  GoogleString html_input = StrCat(pre_head_input, post_head_input);
  scoped_ptr<LogRecord> log_record(new LogRecord);
  rewrite_driver_->set_log_record(log_record.get());
  rewrite_driver_->log_record()->logging_info()
      ->mutable_timing_info()->set_header_fetch_ms(100);
  rewrite_driver_->flush_early_info()->set_last_n_fetch_latencies("96,98");
  rewrite_driver_->flush_early_info()->set_average_fetch_latency_ms(97);

  Parse("not_flushed_early", html_input);
  EXPECT_EQ(html_input, output_);

  // SuppressPreheadFilter should have populated the flush_early_proto with the
  // appropriate pre head information.
  EXPECT_EQ(pre_head_input,
            rewrite_driver()->flush_early_info()->pre_head());
  EXPECT_EQ("100,96,98",
            rewrite_driver_->flush_early_info()->last_n_fetch_latencies());
  EXPECT_EQ(98,
            rewrite_driver_->flush_early_info()->average_fetch_latency_ms());
  rewrite_driver_->set_log_record(NULL);

  // pre head is suppressed if the dummy head was flushed early.
  output_.clear();
  rewrite_driver()->set_flushed_early(true);
  Parse("flushed_early", html_input);
  EXPECT_EQ(post_head_input, output_);
}

TEST_F(SuppressPreheadFilterTest, FlushEarlyMetaTags) {
  InitResources();
  const char html_input[] =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
      "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
      "<meta charset=\"UTF-8\">"
      "</head>"
      "<body></body></html>";
  const char html_without_prehead[] =
      "<head>"
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
      "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
      "<meta charset=\"UTF-8\">"
      "</head>"
      "<body></body></html>";

  Parse("not_flushed_early", html_input);
  EXPECT_EQ(html_input, output_);

  VerifyCharset("text/html;charset=utf-8");

  // pre head is suppressed if the dummy head was flushed early.
  output_.clear();
  rewrite_driver()->set_flushed_early(true);
  Parse("flushed_early", html_input);
  EXPECT_EQ(html_without_prehead, output_);
}

TEST_F(SuppressPreheadFilterTest, MetaTagsOutsideHead) {
  InitResources();
  const char html_input[] =
      "<!DOCTYPE html>"
      "<html>"
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
      "<head></head>"
      "<body></body></html>";
  const char html_without_prehead_and_meta_tags[] =
      "<head>"
      "</head>"
      "<body></body></html>";

  Parse("not_flushed_early", html_input);
  EXPECT_EQ(html_input, output_);

  VerifyCharset("text/html;charset=utf-8");

  // pre head is suppressed if the dummy head was flushed early.
  output_.clear();
  rewrite_driver()->set_flushed_early(true);
  Parse("flushed_early", html_input);
  EXPECT_EQ(html_without_prehead_and_meta_tags, output_);
}

TEST_F(SuppressPreheadFilterTest, NoHead) {
  InitResources();
  const char html_input[] =
      "<!DOCTYPE html>"
      "<html>"
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
      "<body></body></html>";

  const char html_input_with_head_tag[] =
      "<!DOCTYPE html>"
      "<html>"
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
      "<head/><body></body></html>";

  const char html_input_without_prehead[] =
      "<head/><body></body></html>";

  Parse("not_flushed_early", html_input);
  EXPECT_EQ(html_input_with_head_tag, output_);

  VerifyCharset("text/html;charset=utf-8");

  // pre head is suppressed if the dummy head was flushed early.
  output_.clear();
  rewrite_driver()->set_flushed_early(true);
  Parse("flushed_early", html_input);
  EXPECT_EQ(html_input_without_prehead, output_);
}

TEST_F(SuppressPreheadFilterTest, FlushEarlyCharset) {
  InitResources();
  FlushEarlyRenderInfo* info = new FlushEarlyRenderInfo;
  info->set_charset("utf-8");
  rewrite_driver()->set_flush_early_render_info(info);
  server_context()->set_flush_early_info_finder(
      new MeaningfulFlushEarlyInfoFinder);
  headers()->Add(HttpAttributes::kContentType, "text/html");

  const char html_input[] =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "</head>"
      "<body></body></html>";
  const char html_without_prehead[] =
      "<head>"
      "</head>"
      "<body></body></html>";

  Parse("not_flushed_early", html_input);
  EXPECT_EQ(html_input, output_);

  VerifyCharset("text/html; charset=utf-8");

  // pre head is suppressed if the dummy head was flushed early.
  output_.clear();
  rewrite_driver()->set_flushed_early(true);
  Parse("flushed_early", html_input);
  EXPECT_EQ(html_without_prehead, output_);
}

TEST_F(SuppressPreheadFilterTest, FlushEarlyPreExistingCharset) {
  InitResources();
  FlushEarlyRenderInfo* info = new FlushEarlyRenderInfo;
  info->set_charset("utf-8");
  rewrite_driver()->set_flush_early_render_info(info);
  server_context()->set_flush_early_info_finder(
      new MeaningfulFlushEarlyInfoFinder);
  // The charset returned by FlushEarlyRenderInfo will never be different from
  // what is already set on page. However, for the purpose of testing we have a
  // different charset in response headers to ensure that we do not change the
  // charset if the response headers already have them.
  headers()->Add(HttpAttributes::kContentType, "text/html; charset=ISO-8859-1");

  const char html_input[] =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "</head>"
      "<body></body></html>";
  const char html_without_prehead[] =
      "<head>"
      "</head>"
      "<body></body></html>";

  Parse("not_flushed_early", html_input);
  EXPECT_EQ(html_input, output_);

  VerifyCharset("text/html; charset=ISO-8859-1");

  // pre head is suppressed if the dummy head was flushed early.
  output_.clear();
  rewrite_driver()->set_flushed_early(true);
  Parse("flushed_early", html_input);
  EXPECT_EQ(html_without_prehead, output_);
}

TEST_F(SuppressPreheadFilterTest, FlushEarlyCookies) {
  InitResources();
  headers()->Add(HttpAttributes::kSetCookie, "CG=US:CA:Mountain+View");
  headers()->Add(HttpAttributes::kSetCookie, "UA=chrome");
  headers()->Add(HttpAttributes::kSetCookie, "path=/");

  const char html_input[] =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "</head>"
      "<body></body></html>";
  const char html_with_cookie[] =
      "<head>"
      "<script type=\"text/javascript\" pagespeed_no_defer=\"\">"
      "(function(){"
        "var data = [\"CG=US:CA:Mountain+View\",\"UA=chrome\",\"path=/\"];"
        "for (var i = 0; i < data.length; i++) {"
        "document.cookie = data[i];"
       "}})()"
      "</script>"
      "</head>"
      "<body></body></html>";

  Parse("not_flushed_early", html_input);
  EXPECT_EQ(html_input, output_);

  // Javascript for setting the cookie is also flushed.
  output_.clear();
  rewrite_driver()->set_flushed_early(true);
  Parse("flushed_early_with_cookie", html_input);
  EXPECT_EQ(html_with_cookie, output_);
}

TEST_F(SuppressPreheadFilterTest, FlushEarlyCookies2) {
  InitResources();
  headers()->Add(HttpAttributes::kSetCookie, "RMID=266b56483f6e50519316c48a; "
                 "expires=Friday, 13-Sep-2013 08:02:30 GMT; path=/; "
                 "domain=.example.com");

  const char html_input[] =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "</head>"
      "<body></body></html>";
  const char html_with_cookie[] =
      "<head>"
      "<script type=\"text/javascript\" pagespeed_no_defer=\"\">"
      "(function(){"
        "var data = [\"RMID=266b56483f6e50519316c48a; expires=Friday, "
        "13-Sep-2013 08:02:30 GMT; path=/; domain=.example.com\"];"
        "for (var i = 0; i < data.length; i++) {"
        "document.cookie = data[i];"
       "}})()"
      "</script>"
      "</head>"
      "<body></body></html>";

  Parse("not_flushed_early", html_input);
  EXPECT_EQ(html_input, output_);

  // Javascript for setting the cookie is also flushed.
  output_.clear();
  rewrite_driver()->set_flushed_early(true);
  Parse("flushed_early_with_cookie", html_input);
  EXPECT_EQ(html_with_cookie, output_);
}
}  // namespace net_instaweb
