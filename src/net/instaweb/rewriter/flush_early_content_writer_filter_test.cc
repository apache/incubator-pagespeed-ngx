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
// Author: nikhilmadan@google.com (Nikhil Madan)

#include "net/instaweb/rewriter/public/flush_early_content_writer_filter.h"

#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/flush_early_info_finder_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "testing/base/public/gunit.h"
#include "pagespeed/kernel/util/wildcard.h"

namespace net_instaweb {

const char kMockHashValue[] = "MDAwMD";

const char kPrefetchScript[] =
    "<script type='text/javascript'>window.mod_pagespeed_prefetch_start"
    " = Number(new Date());window.mod_pagespeed_num_resources_prefetched"
    " = %d</script>";

const char kHtmlInputPublicCacheableResources[] =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
      "<link type=\"text/css\" rel=\"stylesheet\" href=\"f.css\"/>"
      "<script src=\"g.js\"></script>"
      "<script src=\"http://www.test.com/h.js.pagespeed.jm.%s.js\"></script>"
    "</head>"
    "<body></body></html>";

const char kHtmlInputPrivateCacheableResources[] =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
      "<link type=\"text/css\" rel=\"stylesheet\" href=\"a.css\"/>"
      "<script src=\"b.js\"></script>"
      "<script src=\"http://www.test.com/c.js.pagespeed.jm.%s.js\"></script>"
      "<link type=\"text/css\" rel=\"stylesheet\" href="
      "\"d.css.pagespeed.cf.%s.css\"/>"
    "</head>"
    "<body></body></html>";

class FlushEarlyContentWriterFilterTest : public RewriteTestBase {
 public:
  FlushEarlyContentWriterFilterTest() : writer_(&output_) {}

  virtual bool AddHtmlTags() const { return false; }

 protected:
  virtual void SetUp() {
    SetMockHashValue("00000");  // Base64 encodes to kMockHashValue.
    statistics()->AddTimedVariable(
      FlushEarlyContentWriterFilter::kNumResourcesFlushedEarly,
      ServerContext::kStatisticsGroup);
    options()->EnableFilter(RewriteOptions::kFlushSubresources);
    options()->set_flush_more_resources_early_if_time_permits(true);
    options()->set_flush_more_resources_in_ie_and_firefox(true);
    RewriteTestBase::SetUp();
    rewrite_driver()->SetRequestHeaders(request_headers_);
    rewrite_driver()->set_flushing_early(true);
    rewrite_driver()->SetWriter(&writer_);
    server_context()->set_flush_early_info_finder(
        new MeaningfulFlushEarlyInfoFinder);
    rewrite_driver_->log_record()->SetLogUrlIndices(true);
  }

  virtual void Clear() {
    ClearRewriteDriver();
    rewrite_driver_->flush_early_info()->set_average_fetch_latency_ms(190);
    rewrite_driver_->log_record()->SetLogUrlIndices(true);
    rewrite_driver()->SetRequestHeaders(request_headers_);
    output_.clear();
  }

  void EnableDeferJsAndSetFetchLatency(int latency) {
    Clear();
    options()->ClearSignatureForTesting();
    options()->EnableFilter(RewriteOptions::kDeferJavascript);
    server_context()->ComputeSignature(options());
    rewrite_driver_->flush_early_info()->set_average_fetch_latency_ms(latency);
  }

  GoogleString RewrittenOutputWithResources(const GoogleString& html_output,
                                            const int& number_of_resources,
                                            bool links_flushed) {
    return StrCat(html_output,
                  (links_flushed ?
                   FlushEarlyContentWriterFilter::kDisableLinkTag :
                   ""),
                  StringPrintf(kPrefetchScript, number_of_resources));
  }

  void ExpectNumLogRecords(int expected_records) {
    ScopedMutex lock(rewrite_driver_->log_record()->mutex());
    EXPECT_EQ(expected_records,
              rewrite_driver_->log_record()->logging_info()
              ->rewriter_info_size());
  }

  void ExpectAvailableTimeMs(int64 expected_available_time_ms) {
    ScopedMutex lock(rewrite_driver_->log_record()->mutex());
    EXPECT_EQ(expected_available_time_ms,
              rewrite_driver_->log_record()->logging_info()
              ->flush_early_flow_info().available_time_ms());
  }

  void ExpectLogRecord(int index,
                       RewriterApplication::Status status,
                       int resource_index,
                       FlushEarlyResourceInfo::ContentType content_type,
                       FlushEarlyResourceInfo::ResourceType resource_type,
                       bool is_bandwidth_affected,
                       bool in_head) {
    const RewriterInfo& rewriter_info =
        rewrite_driver_->log_record()->logging_info()->rewriter_info(index);
    const FlushEarlyResourceInfo& resource_info =
        rewriter_info.flush_early_resource_info();
    EXPECT_EQ("fs", rewriter_info.id());
    EXPECT_EQ(status, rewriter_info.status());
    if (resource_index >= 0) {
      EXPECT_TRUE(rewriter_info.has_rewrite_resource_info());
      EXPECT_EQ(resource_index,
          rewriter_info.rewrite_resource_info().original_resource_url_index());
    } else {
      EXPECT_FALSE(rewriter_info.has_rewrite_resource_info());
    }
    EXPECT_EQ(content_type, resource_info.content_type());
    EXPECT_EQ(resource_type, resource_info.resource_type());
    EXPECT_EQ(is_bandwidth_affected, resource_info.is_bandwidth_affected());
    EXPECT_EQ(in_head, resource_info.in_head());
  }

  void SetPrivateCacheableUrls() {
    FlushEarlyRenderInfo* info =  new FlushEarlyRenderInfo;
    info->add_private_cacheable_url("http://test.com/a.css");
    info->add_private_cacheable_url("http://test.com/c.js");
    info->add_private_cacheable_url("http://test.com/d.css");
    rewrite_driver()->set_flush_early_render_info(info);
  }

  void SetPublicCacheableUrls() {
    FlushEarlyRenderInfo* info =  new FlushEarlyRenderInfo;
    info->add_public_cacheable_url("http://test.com/f.css");
    info->add_public_cacheable_url("http://test.com/g.js");
    rewrite_driver()->set_flush_early_render_info(info);
  }

  GoogleString GetOutputWithHash(StringPiece format) {
    GoogleString output(format.data(), format.size());
    GlobalReplaceSubstring("%s", kMockHashValue, &output);
    return output;
  }

  void VerifyJsNotFlushed() {
    SetPrivateCacheableUrls();
    GoogleString html_input = GetOutputWithHash(
        kHtmlInputPrivateCacheableResources);
    GoogleString html_output;

    rewrite_driver()->SetUserAgent("prefetch_image_tag");
    html_output = GetOutputWithHash(
        "<link rel=\"stylesheet\" href=\"a.css\"/>\n"
        "<link rel=\"stylesheet\" href=\"d.css.pagespeed.cf.%s.css\"/>\n");

    Parse("prefetch_image_tag", html_input);
    EXPECT_EQ(RewrittenOutputWithResources(html_output, 2, true), output_);
  }

  GoogleString output_;

 private:
  scoped_ptr<FlushEarlyContentWriterFilter> filter_;
  StringWriter writer_;
  RequestHeaders request_headers_;

  DISALLOW_COPY_AND_ASSIGN(FlushEarlyContentWriterFilterTest);
};

TEST_F(FlushEarlyContentWriterFilterTest, TestDifferentBrowsers) {
  Clear();
  GoogleString html_input = GetOutputWithHash(
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
        "<link type=\"text/css\" rel=\"stylesheet\" href=\"a.css\" "
        "pagespeed_size=\"1000\"/>"
        "<script src=\"b.js\" pagespeed_size=\"1000\"></script>"
        "<script src=\"http://www.test.com/c.js.pagespeed.jm.%s.js\" "
        "pagespeed_size=\"1000\"></script>"
        "<link type=\"text/css\" rel=\"stylesheet\" href="
        "\"d.css.pagespeed.cf.%s.css\" pagespeed_size=\"1000\"/>"
        "<img src=\"http://www.test.com/e.jpg.pagespeed.ce.%s.jpg\" "
        "pagespeed_size=\"1000\"/>"
        "<img src=\"http://www.test.com/g.jpg.pagespeed.ce.%s.jpg\" "
        "pagespeed_size=\"1000000\"/>"
        "<link rel=\"dns-prefetch\" href=\"//test.com\">"
        "<link rel=\"prefetch\" href=\"//test1.com\">"
      "</head>"
      "<body>"
      "<script src=\"d.js.pagespeed.ce.%s.js\" "
      "pagespeed_size=\"1000\"></script>"
      "<script src=\"e.js.pagespeed.ce.%s.js\" "
      "pagespeed_size=\"100000\"></script>"
      "</body></html>");
  GoogleString html_output;

  // First test with no User-Agent.
  Parse("no_user_agent", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 0, false), output_);
  // TODO(mmohabey): If the browser is not supported by flush subresources
  // filter, we should avoid all the code in StartDocument/EndDocument.
  // Otherwise we will be creating spurious log records like below.
  ExpectNumLogRecords(1);
  ExpectAvailableTimeMs(190);
  // DeferJs script is not flushed since it does not support the empty
  // user-agent.
  ExpectLogRecord(0,
                  RewriterApplication::NOT_APPLIED,
                  -1,
                  FlushEarlyResourceInfo::JS,
                  FlushEarlyResourceInfo::DEFERJS_SCRIPT,
                  false /* not affected by bandwidth */,
                  false /* not in HEAD */);

  // Set the User-Agent to prefetch_link_script_tag.
  Clear();
  rewrite_driver()->SetUserAgent("prefetch_link_script_tag");
  html_output = GetOutputWithHash(
      "<script type=\"text/javascript\">(function(){new Image().src=\""
      "http://www.test.com/e.jpg.pagespeed.ce.%s.jpg\";})()</script>"
      "<link rel=\"dns-prefetch\" href=\"//test.com\">"
      "<link rel=\"prefetch\" href=\"//test1.com\">"
      "<script type=\"psa_prefetch\" src="
      "\"http://www.test.com/c.js.pagespeed.jm.%s.js\"></script>\n"
      "<link rel=\"stylesheet\" href=\"d.css.pagespeed.cf.%s.css\"/>\n"
      "<script type=\"psa_prefetch\" src=\"d.js.pagespeed.ce.%s.js\">"
      "</script>\n");

  Parse("prefetch_link_script_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 4, true), output_);

  ExpectNumLogRecords(9);
  ExpectAvailableTimeMs(190);
  // a.css is non-rewritten CSS.
  ExpectLogRecord(0,
                  RewriterApplication::NOT_APPLIED,
                  0,
                  FlushEarlyResourceInfo::CSS,
                  FlushEarlyResourceInfo::NON_PAGESPEED,
                  false /* not affected by bandwidth */,
                  true /* in HEAD */);
  // b.js is non-rewritten JS.
  ExpectLogRecord(1,
                  RewriterApplication::NOT_APPLIED,
                  1,
                  FlushEarlyResourceInfo::JS,
                  FlushEarlyResourceInfo::NON_PAGESPEED,
                  false /* not affected by bandwidth */,
                  true /* in HEAD */);
  // c.js is rewritten JS.
  ExpectLogRecord(2,
                  RewriterApplication::APPLIED_OK,
                  2,
                  FlushEarlyResourceInfo::JS,
                  FlushEarlyResourceInfo::PAGESPEED,
                  false /* not affected by bandwidth */,
                  true /* in HEAD */);
  // d.css is rewritten CSS.
  ExpectLogRecord(3,
                  RewriterApplication::APPLIED_OK,
                  3,
                  FlushEarlyResourceInfo::CSS,
                  FlushEarlyResourceInfo::PAGESPEED,
                  false /* not affected by bandwidth */,
                  true /* in HEAD */);
  // e.jpg is an image and the prefetch mechanism does not allow flushing
  // images.
  ExpectLogRecord(4,
                  RewriterApplication::APPLIED_OK,
                  4,
                  FlushEarlyResourceInfo::IMAGE,
                  FlushEarlyResourceInfo::PAGESPEED,
                  true /* not affected by bandwidth */,
                  true /* in HEAD */);
  // g.jpg is an image but size does not allow it to flushing.
  ExpectLogRecord(5,
                  RewriterApplication::NOT_APPLIED,
                  5,
                  FlushEarlyResourceInfo::IMAGE,
                  FlushEarlyResourceInfo::PAGESPEED,
                  true /* not affected by bandwidth */,
                  true /* in HEAD */);
  // d.js is rewritten JS.
  ExpectLogRecord(6,
                  RewriterApplication::APPLIED_OK,
                  6,
                  FlushEarlyResourceInfo::JS,
                  FlushEarlyResourceInfo::PAGESPEED,
                  true /* not affected by bandwidth */,
                  false /* not in HEAD */);
  // e.js is rewritten JS but size exceeds threshold.
  ExpectLogRecord(7,
                  RewriterApplication::NOT_APPLIED,
                  7,
                  FlushEarlyResourceInfo::JS,
                  FlushEarlyResourceInfo::PAGESPEED,
                  true /* not affected by bandwidth */,
                  false /* not in HEAD */);
  // DeferJs script is not flushed since it is not enabled. SetUp is not called
  // again with the test case, so many of the filters/options are actually
  // disabled.
  ExpectLogRecord(8,
                  RewriterApplication::NOT_APPLIED,
                  -1,
                  FlushEarlyResourceInfo::JS,
                  FlushEarlyResourceInfo::DEFERJS_SCRIPT,
                  false /* not affected by bandwidth */,
                  false /* not in HEAD */);

  // Set the User-Agent to prefetch_image_tag.
  Clear();
  rewrite_driver()->SetUserAgent("prefetch_image_tag");
  html_output = GetOutputWithHash(
      "<script type=\"text/javascript\">(function(){"
      "new Image().src=\"http://www.test.com/c.js.pagespeed.jm.%s.js\";"
      "new Image().src=\"http://www.test.com/e.jpg.pagespeed.ce.%s.jpg\";})()"
      "</script>"
      "<link rel=\"dns-prefetch\" href=\"//test.com\">"
      "<link rel=\"prefetch\" href=\"//test1.com\">"
      "<script type=\"text/javascript\">"
      "(function(){new Image().src=\"d.js.pagespeed.ce.%s.js\";})()</script>"
      "<link rel=\"stylesheet\" href=\"d.css.pagespeed.cf.%s.css\"/>\n");

  Parse("prefetch_image_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 4, true), output_);

  // Enable defer_javasript. We will flush JS resources only if time permits.
  Clear();
  rewrite_driver()->SetUserAgent("prefetch_image_tag");
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kDeferJavascript);
  server_context()->ComputeSignature(options());

  html_output = GetOutputWithHash(
      "<script type=\"text/javascript\">(function(){"
      "new Image().src=\"http://www.test.com/e.jpg.pagespeed.ce.%s.jpg\";})()"
      "</script>"
      "<link rel=\"dns-prefetch\" href=\"//test.com\">"
      "<link rel=\"prefetch\" href=\"//test1.com\">"
      "<script type=\"text/javascript\">"
      "(function(){"
      "new Image().src=\"http://www.test.com/c.js.pagespeed.jm.%s.js\";"
      "new Image().src=\"d.js.pagespeed.ce.%s.js\";})()</script>"
      "<link rel=\"stylesheet\" href=\"d.css.pagespeed.cf.%s.css\"/>\n");

  Parse("defer_javasript", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 4, true), output_);

  // Set the User-Agent to prefetch_link_script_tag with defer_javascript
  // enabled.
  Clear();
  rewrite_driver()->SetUserAgent("prefetch_link_script_tag");
  html_output = GetOutputWithHash(
      "<script type=\"text/javascript\">(function(){new Image().src=\""
      "http://www.test.com/e.jpg.pagespeed.ce.%s.jpg\";})()</script>"
      "<link rel=\"dns-prefetch\" href=\"//test.com\">"
      "<link rel=\"prefetch\" href=\"//test1.com\">"
      "<link rel=\"stylesheet\" href=\"d.css.pagespeed.cf.%s.css\"/>\n"
      "<script type=\"psa_prefetch\" src=\"/psajs/js_defer.0.js\">"
      "</script>\n");

  Parse("prefetch_link_script_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 3, true), output_);
}

TEST_F(FlushEarlyContentWriterFilterTest, NoResourcesToFlush) {
  GoogleString html_input =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
        "<link type=\"text/css\" rel=\"stylesheet\" href=\"a.css\"/>"
        "<script src=\"b.js\"></script>"
      "</head>"
      "<body></body></html>";
  GoogleString html_output;

  // First test with no User-Agent.
  Parse("no_user_agent", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 0, false), output_);

  // Set the User-Agent to prefetch_link_script_tag.
  output_.clear();
  rewrite_driver()->SetUserAgent("prefetch_link_script_tag");

  Parse("prefetch_link_script_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 0, false), output_);

  // Set the User-Agent to prefetch_image_tag.
  output_.clear();
  rewrite_driver()->SetUserAgent("prefetch_image_tag");

  Parse("prefetch_image_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 0, false), output_);
}

TEST_F(FlushEarlyContentWriterFilterTest, TooManyRewriterInfoRecords) {
  Clear();
  GoogleString html_input = GetOutputWithHash(
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
        "<link type=\"text/css\" rel=\"stylesheet\" "
          "href=\"a.css.pagespeed.cf.%s.css\">"
        "<link type=\"text/css\" rel=\"stylesheet\" "
          "href=\"b.css.pagespeed.cf.%s.css\">"
        "<link type=\"text/css\" rel=\"stylesheet\" "
          "href=\"c.css.pagespeed.cf.%s.css\">"
      "</head><body></body></html>");
  GoogleString html_output = GetOutputWithHash(
      "<link rel=\"stylesheet\" href=\"a.css.pagespeed.cf.%s.css\"/>\n"
      "<link rel=\"stylesheet\" href=\"b.css.pagespeed.cf.%s.css\"/>\n"
      "<link rel=\"stylesheet\" href=\"c.css.pagespeed.cf.%s.css\"/>\n");

  rewrite_driver()->SetUserAgent("prefetch_link_script_tag");
  rewrite_driver_->log_record()->SetRewriterInfoMaxSize(2);
  Parse("prefetch_link_script_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 3, true), output_);
  ExpectNumLogRecords(2);
  EXPECT_TRUE(logging_info()->rewriter_info_size_limit_exceeded());
}

TEST_F(FlushEarlyContentWriterFilterTest, FlushDeferJsEarly) {
  GoogleString html_input =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "</head>"
      "<body></body></html>";

  // Set fetch latency to 0.
  // Irrespective of AvailableTimeMs, DeferJs should be flushed early always.
  EnableDeferJsAndSetFetchLatency(0);
  // User-Agent: prefetch_link_script_tag.
  output_.clear();
  rewrite_driver()->SetUserAgent("prefetch_link_script_tag");
  Parse("prefetch_link_script_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(
      "<script type=\"psa_prefetch\" src=\"/psajs/js_defer.0.js\"></script>\n",
      1, false), output_);

  ExpectNumLogRecords(1);
  ExpectAvailableTimeMs(0);
  ExpectLogRecord(0,
                  RewriterApplication::APPLIED_OK,
                  -1,
                  FlushEarlyResourceInfo::JS,
                  FlushEarlyResourceInfo::DEFERJS_SCRIPT,
                  true /* affected by bandwidth */,
                  false /* not in HEAD */);
}

TEST_F(FlushEarlyContentWriterFilterTest, CacheablePrivateResources1) {
  SetPrivateCacheableUrls();
  GoogleString html_input = GetOutputWithHash(
      kHtmlInputPrivateCacheableResources);
  GoogleString html_output;

  // First test with no User-Agent.
  Parse("no_user_agent", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 0, false), output_);
}

TEST_F(FlushEarlyContentWriterFilterTest, CacheablePrivateResources2) {
  Clear();
  SetPrivateCacheableUrls();
  GoogleString html_input = GetOutputWithHash(
      kHtmlInputPrivateCacheableResources);
  GoogleString html_output;

  // Set the User-Agent to prefetch_link_script_tag.
  rewrite_driver()->SetUserAgent("prefetch_link_script_tag");
  html_output = GetOutputWithHash(
      "<link rel=\"stylesheet\" href=\"a.css\"/>\n"
      "<script type=\"psa_prefetch\" src="
      "\"http://www.test.com/c.js.pagespeed.jm.%s.js\"></script>\n"
      "<link rel=\"stylesheet\" href=\"d.css.pagespeed.cf.%s.css\"/>\n");

  Parse("prefetch_link_script_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 3, true), output_);
  ExpectNumLogRecords(5);
  // a.css is private cacheable CSS.
  ExpectLogRecord(0,
                  RewriterApplication::APPLIED_OK,
                  0,
                  FlushEarlyResourceInfo::CSS,
                  FlushEarlyResourceInfo::PRIVATE_CACHEABLE,
                  false /* not affected by bandwidth */,
                  true /* in HEAD */);
  // b.js is non-rewritten JS.
  ExpectLogRecord(1,
                  RewriterApplication::NOT_APPLIED,
                  1,
                  FlushEarlyResourceInfo::JS,
                  FlushEarlyResourceInfo::NON_PAGESPEED,
                  false /* not affected by bandwidth */,
                  true /* in HEAD */);
  // c.js is rewritten JS.
  ExpectLogRecord(2,
                  RewriterApplication::APPLIED_OK,
                  2,
                  FlushEarlyResourceInfo::JS,
                  FlushEarlyResourceInfo::PAGESPEED,
                  false /* not affected by bandwidth */,
                  true /* in HEAD */);
  // d.css is rewritten CSS.
  ExpectLogRecord(3,
                  RewriterApplication::APPLIED_OK,
                  3,
                  FlushEarlyResourceInfo::CSS,
                  FlushEarlyResourceInfo::PAGESPEED,
                  false /* not affected by bandwidth */,
                  true /* in HEAD */);
  // defer_javascript is not enabled.
  ExpectLogRecord(4,
                  RewriterApplication::NOT_APPLIED,
                  -1,
                  FlushEarlyResourceInfo::JS,
                  FlushEarlyResourceInfo::DEFERJS_SCRIPT,
                  false /* not affected by bandwidth */,
                  false /* not in HEAD */);
}

TEST_F(FlushEarlyContentWriterFilterTest, CacheablePrivateResources3) {
  SetPrivateCacheableUrls();
  GoogleString html_input = GetOutputWithHash(
      kHtmlInputPrivateCacheableResources);
  GoogleString html_output;

  // Set the User-Agent to prefetch_image_tag.
  rewrite_driver()->SetUserAgent("prefetch_image_tag");
  html_output = GetOutputWithHash(
      "<script type=\"text/javascript\">(function(){"
      "new Image().src=\"http://www.test.com/c.js.pagespeed.jm.%s.js\";"
      "})()</script>"
      "<link rel=\"stylesheet\" href=\"a.css\"/>\n"
      "<link rel=\"stylesheet\" href=\"d.css.pagespeed.cf.%s.css\"/>\n");

  Parse("prefetch_image_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 3, true), output_);
}

TEST_F(FlushEarlyContentWriterFilterTest, CacheablePrivateResources4) {
  // Enable defer_javasript. We don't flush JS resources now.
  rewrite_driver()->SetUserAgent("prefetch_image_tag");
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kDeferJavascript);
  options()->DisableFilter(RewriteOptions::kSplitHtml);
  server_context()->ComputeSignature(options());
  VerifyJsNotFlushed();
}

TEST_F(FlushEarlyContentWriterFilterTest, CacheablePrivateResources5) {
  // Enable split_html. We don't flush JS resources now.
  rewrite_driver()->SetUserAgent("prefetch_image_tag");
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kSplitHtml);
  options()->DisableFilter(RewriteOptions::kDeferJavascript);
  server_context()->ComputeSignature(options());
  VerifyJsNotFlushed();
}

TEST_F(FlushEarlyContentWriterFilterTest, CacheablePublicResources1) {
  SetPublicCacheableUrls();
  GoogleString html_input = GetOutputWithHash(
      kHtmlInputPublicCacheableResources);
  GoogleString html_output;

  // First test with no User-Agent.
  Parse("no_user_agent", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 0, false), output_);
}

TEST_F(FlushEarlyContentWriterFilterTest,
       CacheablePublicResourcesBlacklistedUA) {
  Clear();
  SetPublicCacheableUrls();
  GoogleString html_input = GetOutputWithHash(
      kHtmlInputPublicCacheableResources);
  GoogleString html_output;

  // Disallow one of the public cacheable resources.
  options()->Disallow("*f.css*");
  rewrite_driver()->SetUserAgent("prefetch_image_tag");
  html_output = GetOutputWithHash(
      "<script type=\"text/javascript\">(function(){"
      "new Image().src=\"http://www.test.com/h.js.pagespeed.jm.%s.js\";"
      "})()</script>"
      "<link rel=\"stylesheet\" href=\"f.css\"/>\n");

  Parse("prefetch_image_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 2, true), output_);
  ExpectNumLogRecords(4);
  // f.css is public cacheable CSS and flush early is applied.
  ExpectLogRecord(0,
                  RewriterApplication::APPLIED_OK,
                  0,
                  FlushEarlyResourceInfo::CSS,
                  FlushEarlyResourceInfo::PUBLIC_CACHEABLE,
                  false /* not affected by bandwidth */,
                  true /* in HEAD */);
  // g.js is non-rewritten JS.
  ExpectLogRecord(1,
                  RewriterApplication::NOT_APPLIED,
                  1,
                  FlushEarlyResourceInfo::JS,
                  FlushEarlyResourceInfo::PUBLIC_CACHEABLE,
                  false /* not affected by bandwidth */,
                  true /* in HEAD */);
  // h.js is rewritten JS.
  ExpectLogRecord(2,
                  RewriterApplication::APPLIED_OK,
                  2,
                  FlushEarlyResourceInfo::JS,
                  FlushEarlyResourceInfo::PAGESPEED,
                  false /* not affected by bandwidth */,
                  true /* in HEAD */);
  // defer_javascript is not enabled.
  ExpectLogRecord(3,
                  RewriterApplication::NOT_APPLIED,
                  -1,
                  FlushEarlyResourceInfo::JS,
                  FlushEarlyResourceInfo::DEFERJS_SCRIPT,
                  false /* not affected by bandwidth */,
                  false /* not in HEAD */);
}
TEST_F(FlushEarlyContentWriterFilterTest,
       CacheablePublicResourcesNotBlacklistedUA) {
  Clear();
  SetPublicCacheableUrls();
  GoogleString html_input = GetOutputWithHash(
      kHtmlInputPublicCacheableResources);
  GoogleString html_output;

  // Set the User-Agent to prefetch_image_tag.
  rewrite_driver()->SetUserAgent("prefetch_image_tag");
  html_output = GetOutputWithHash(
      "<script type=\"text/javascript\">(function(){"
      "new Image().src=\"http://www.test.com/h.js.pagespeed.jm.%s.js\";"
      "})()</script>");

  Parse("prefetch_image_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 1, false), output_);
  ExpectNumLogRecords(4);
  // f.css is public cacheable CSS.
  ExpectLogRecord(0,
                  RewriterApplication::NOT_APPLIED,
                  0,
                  FlushEarlyResourceInfo::CSS,
                  FlushEarlyResourceInfo::PUBLIC_CACHEABLE,
                  false /* not affected by bandwidth */,
                  true /* in HEAD */);
  // g.js is non-rewritten JS.
  ExpectLogRecord(1,
                  RewriterApplication::NOT_APPLIED,
                  1,
                  FlushEarlyResourceInfo::JS,
                  FlushEarlyResourceInfo::PUBLIC_CACHEABLE,
                  false /* not affected by bandwidth */,
                  true /* in HEAD */);
  // h.js is rewritten JS.
  ExpectLogRecord(2,
                  RewriterApplication::APPLIED_OK,
                  2,
                  FlushEarlyResourceInfo::JS,
                  FlushEarlyResourceInfo::PAGESPEED,
                  false /* not affected by bandwidth */,
                  true /* in HEAD */);
  // defer_javascript is not enabled.
  ExpectLogRecord(3,
                  RewriterApplication::NOT_APPLIED,
                  -1,
                  FlushEarlyResourceInfo::JS,
                  FlushEarlyResourceInfo::DEFERJS_SCRIPT,
                  false /* not affected by bandwidth */,
                  false /* not in HEAD */);
}

TEST_F(FlushEarlyContentWriterFilterTest, FlushEarlyStyleAsScript) {
  GoogleString html_input =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "<link type=\"text/css\" rel=\"stylesheet\" "
      "href=\"a.css.pagespeed.cf.xxxx.css\">"
      "<style data-pagespeed-flush-style=\"123\">b_used {color: blue }"
      "</style>\n"
      "<link type=\"text/css\" rel=\"stylesheet\""
      "href=\"d.css.pagespeed.cf.xxxx.css\" pagespeed_size=\"1000\"/>"
      "</head>"
      "<body>"
      "<style data-pagespeed-flush-style=\"345\">c_used {color: cyan }"
      "</style>\n"
      "</body></html>";

  const char kCssLinkPrefetchTag[] =
      "<link rel=\"stylesheet\" href=\"%s*\"/>\n";

  GoogleString html_output = StrCat(
      StringPrintf(kCssLinkPrefetchTag, "a.css"),
      "<script type=\"text/psa_flush_style\" id=\"123\">"
      "b_used {color: blue }</script>",
      StringPrintf(kCssLinkPrefetchTag, "d.css"),
      "<script type=\"text/psa_flush_style\" id=\"345\">"
      "c_used {color: cyan }</script>",
      FlushEarlyContentWriterFilter::kDisableLinkTag,
      StringPrintf(kPrefetchScript, 4));

  Clear();
  rewrite_driver()->SetUserAgent("prefetch_image_tag");
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kPrioritizeCriticalCss);
  options()->set_enable_flush_early_critical_css(true);
  server_context()->ComputeSignature(options());

  Parse("prefetch_link_script_tag", html_input);
  EXPECT_TRUE(Wildcard(html_output).Match(output_)) <<
      "Expected:\n" << html_output << "\nGot:\n" << output_;
}

TEST_F(FlushEarlyContentWriterFilterTest, DontFlushEarlyStyleIfFlagDisabled) {
  GoogleString html_input =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "<style data-pagespeed-flush-style=\"123\">b_used {color: blue }"
      "</style>\n"
      "</head>"
      "<body>"
      "<style data-pagespeed-flush-style=\"345\">c_used {color: cyan }"
      "</style>\n"
      "</body></html>";

  GoogleString html_output = StringPrintf(kPrefetchScript, 0);

  Clear();
  rewrite_driver()->SetUserAgent("prefetch_image_tag");
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kPrioritizeCriticalCss);
  options()->set_enable_flush_early_critical_css(false);
  server_context()->ComputeSignature(options());

  Parse("prefetch_link_script_tag", html_input);
  EXPECT_TRUE(Wildcard(html_output).Match(output_)) <<
      "Expected:\n" << html_output << "\nGot:\n" << output_;
}

}  // namespace net_instaweb
