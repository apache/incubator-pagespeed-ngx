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

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/flush_early_info_finder_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {
const char kPrefetchScript[] =
    "<script type='text/javascript'>window.mod_pagespeed_prefetch_start"
    " = Number(new Date());window.mod_pagespeed_num_resources_prefetched"
    " = %d</script>";

class FlushEarlyContentWriterFilterTest : public RewriteTestBase {
 public:
  FlushEarlyContentWriterFilterTest() : writer_(&output_) {}

  virtual bool AddHtmlTags() const { return false; }

 protected:
  virtual void SetUp() {
    statistics()->AddTimedVariable(
      FlushEarlyContentWriterFilter::kNumResourcesFlushedEarly,
      ServerContext::kStatisticsGroup);
    options()->EnableFilter(RewriteOptions::kFlushSubresources);
    options()->set_enable_flush_subresources_experimental(true);
    options()->set_flush_more_resources_early_if_time_permits(true);
    options()->set_flush_more_resources_in_ie_and_firefox(true);
    RewriteTestBase::SetUp();
    rewrite_driver()->set_request_headers(&request_headers_);
    rewrite_driver()->set_flushing_early(true);
    rewrite_driver()->SetWriter(&writer_);
    server_context()->set_flush_early_info_finder(
        new MeaningfulFlushEarlyInfoFinder);
  }

  virtual void Clear() {
    ClearRewriteDriver();
    rewrite_driver_->flush_early_info()->set_average_fetch_latency_ms(190);
    rewrite_driver()->set_request_headers(&request_headers_);
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
                                            const int& number_of_resources) {
    return StrCat(html_output,
                  StringPrintf(kPrefetchScript, number_of_resources));
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
  GoogleString html_input =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
        "<link type=\"text/css\" rel=\"stylesheet\" href=\"a.css\" "
        "pagespeed_size=\"1000\"/>"
        "<script src=\"b.js\" pagespeed_size=\"1000\"></script>"
        "<script src=\"http://www.test.com/c.js.pagespeed.jm.0.js\" "
        "pagespeed_size=\"1000\"></script>"
        "<link type=\"text/css\" rel=\"stylesheet\" href="
        "\"d.css.pagespeed.cf.0.css\" pagespeed_size=\"1000\"/>"
        "<img src=\"http://www.test.com/e.jpg.pagespeed.ce.0.jpg\" "
        "pagespeed_size=\"1000\"/>"
        "<img src=\"http://www.test.com/g.jpg.pagespeed.ce.0.jpg\" "
        "pagespeed_size=\"1000000\"/>"
        "<link rel=\"dns-prefetch\" href=\"//test.com\">"
        "<link rel=\"prefetch\" href=\"//test1.com\">"
      "</head>"
      "<body>"
      "<script src=\"d.js.pagespeed.ce.0.js\" "
      "pagespeed_size=\"1000\"></script>"
      "<script src=\"e.js.pagespeed.ce.0.js\" "
      "pagespeed_size=\"100000\"></script>"
      "</body></html>";
  GoogleString html_output;

  // First test with no User-Agent.
  Parse("no_user_agent", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 0), output_);

  // Set the User-Agent to prefetch_link_rel_subresource.
  Clear();
  rewrite_driver()->set_user_agent("prefetch_link_rel_subresource");
  html_output =
      "<link rel=\"subresource\" href="
      "\"http://www.test.com/c.js.pagespeed.jm.0.js\"/>\n"
      "<link rel=\"subresource\" href=\"d.css.pagespeed.cf.0.css\"/>\n"
      "<link rel=\"dns-prefetch\" href=\"//test.com\">"
      "<link rel=\"prefetch\" href=\"//test1.com\">";

  Parse("prefetch_link_rel_subresource", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 2), output_);

  // Set the User-Agent to prefetch_link_script_tag.
  Clear();
  rewrite_driver()->set_user_agent("prefetch_link_script_tag");
  html_output =
      "<script type=\"text/javascript\">(function(){new Image().src=\""
      "http://www.test.com/e.jpg.pagespeed.ce.0.jpg\";})()</script>"
      "<link rel=\"dns-prefetch\" href=\"//test.com\">"
      "<link rel=\"prefetch\" href=\"//test1.com\">"
      "<script type=\"psa_prefetch\" src="
      "\"http://www.test.com/c.js.pagespeed.jm.0.js\"></script>\n"
      "<link rel=\"stylesheet\" href=\"d.css.pagespeed.cf.0.css\" "
      "media=\"print\" disabled=\"true\"/>\n"
      "<script type=\"psa_prefetch\" src=\"d.js.pagespeed.ce.0.js\">"
      "</script>\n";

  Parse("prefetch_link_script_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 4), output_);

  // Set the User-Agent to prefetch_image_tag.
  Clear();
  rewrite_driver()->set_user_agent("prefetch_image_tag");
  html_output =
      "<script type=\"text/javascript\">(function(){"
      "new Image().src=\"http://www.test.com/c.js.pagespeed.jm.0.js\";"
      "new Image().src=\"d.css.pagespeed.cf.0.css\";"
      "new Image().src=\"http://www.test.com/e.jpg.pagespeed.ce.0.jpg\";})()"
      "</script>"
      "<link rel=\"dns-prefetch\" href=\"//test.com\">"
      "<link rel=\"prefetch\" href=\"//test1.com\">"
      "<script type=\"text/javascript\">"
      "(function(){new Image().src=\"d.js.pagespeed.ce.0.js\";})()</script>";

  Parse("prefetch_image_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 4), output_);

  // Enable defer_javasript. We will flush JS resources only if time permits.
  Clear();
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kDeferJavascript);
  server_context()->ComputeSignature(options());

  html_output =
      "<script type=\"text/javascript\">(function(){"
      "new Image().src=\"d.css.pagespeed.cf.0.css\";"
      "new Image().src=\"http://www.test.com/e.jpg.pagespeed.ce.0.jpg\";})()"
      "</script>"
      "<link rel=\"dns-prefetch\" href=\"//test.com\">"
      "<link rel=\"prefetch\" href=\"//test1.com\">"
      "<script type=\"text/javascript\">"
      "(function(){"
      "new Image().src=\"http://www.test.com/c.js.pagespeed.jm.0.js\";"
      "new Image().src=\"d.js.pagespeed.ce.0.js\";})()"
      "</script>";

  Parse("defer_javasript", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 4), output_);

  // Set the User-Agent to prefetch_link_script_tag with defer_javascript
  // enabled.
  Clear();
  rewrite_driver()->set_user_agent("prefetch_link_script_tag");
  html_output =
      "<script type=\"text/javascript\">(function(){new Image().src=\""
      "http://www.test.com/e.jpg.pagespeed.ce.0.jpg\";})()</script>"
      "<link rel=\"dns-prefetch\" href=\"//test.com\">"
      "<link rel=\"prefetch\" href=\"//test1.com\">"
      "<link rel=\"stylesheet\" href=\"d.css.pagespeed.cf.0.css\" "
      "media=\"print\" disabled=\"true\"/>\n";

  Parse("prefetch_link_script_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 2), output_);
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
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 0), output_);

  // Set the User-Agent to prefetch_link_rel_subresource.
  output_.clear();
  rewrite_driver()->set_user_agent("prefetch_link_rel_subresource");

  Parse("prefetch_link_rel_subresource", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 0), output_);

  // Set the User-Agent to prefetch_image_tag.
  output_.clear();
  rewrite_driver()->set_user_agent("prefetch_image_tag");

  Parse("prefetch_image_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 0), output_);
}

TEST_F(FlushEarlyContentWriterFilterTest, FlushDeferJsEarlyIfTimePermits) {
  GoogleString html_input =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "</head>"
      "<body></body></html>";

  // Set fetch latency to 0. DeferJs should not be flushed early.
  EnableDeferJsAndSetFetchLatency(0);
  // User-Agent: prefetch_link_script_tag.
  output_.clear();
  rewrite_driver()->set_user_agent("prefetch_link_script_tag");
  Parse("prefetch_link_script_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources("", 0), output_);


  // Set fetch latency to 200. DeferJs should be flushed early.
  EnableDeferJsAndSetFetchLatency(200);
  // User-Agent: prefetch_link_script_tag.
  output_.clear();
  rewrite_driver()->set_user_agent("prefetch_link_script_tag");
  Parse("prefetch_link_script_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(
      "<script type=\"psa_prefetch\" src=\"/psajs/js_defer.0.js\"></script>\n",
      1), output_);
}

TEST_F(FlushEarlyContentWriterFilterTest, CacheablePrivateResources) {
  FlushEarlyRenderInfo* info =  new FlushEarlyRenderInfo;
  info->add_private_cacheable_url("http://test.com/a.css");
  info->add_private_cacheable_url("http://test.com/c.js");
  info->add_private_cacheable_url("http://test.com/d.css");
  rewrite_driver()->set_flush_early_render_info(info);

  GoogleString html_input =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
        "<link type=\"text/css\" rel=\"stylesheet\" href=\"a.css\"/>"
        "<script src=\"b.js\"></script>"
        "<script src=\"http://www.test.com/c.js.pagespeed.jm.0.js\"></script>"
        "<link type=\"text/css\" rel=\"stylesheet\" href="
        "\"d.css.pagespeed.cf.0.css\"/>"
      "</head>"
      "<body></body></html>";
  GoogleString html_output;

  // First test with no User-Agent.
  Parse("no_user_agent", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 0), output_);

  // Set the User-Agent to prefetch_link_rel_subresource.
  output_.clear();
  rewrite_driver()->set_user_agent("prefetch_link_rel_subresource");
  html_output =
      "<link rel=\"subresource\" href=\"a.css\"/>\n"
      "<link rel=\"subresource\" href="
      "\"http://www.test.com/c.js.pagespeed.jm.0.js\"/>\n"
      "<link rel=\"subresource\" href=\"d.css.pagespeed.cf.0.css\"/>\n";

  Parse("prefetch_link_rel_subresource", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 3), output_);

  // Set the User-Agent to prefetch_image_tag.
  output_.clear();
  rewrite_driver()->set_user_agent("prefetch_image_tag");
  html_output =
      "<script type=\"text/javascript\">(function(){"
      "new Image().src=\"a.css\";"
      "new Image().src=\"http://www.test.com/c.js.pagespeed.jm.0.js\";"
      "new Image().src=\"d.css.pagespeed.cf.0.css\";})()"
      "</script>";

  Parse("prefetch_image_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 3), output_);

  // Enable defer_javasript. We don't flush JS resources now.
  output_.clear();
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kDeferJavascript);
  server_context()->ComputeSignature(options());

  html_output =
      "<script type=\"text/javascript\">(function(){"
      "new Image().src=\"a.css\";"
      "new Image().src=\"d.css.pagespeed.cf.0.css\";})()"
      "</script>";

  Parse("prefetch_image_tag", html_input);
  EXPECT_EQ(RewrittenOutputWithResources(html_output, 2), output_);
}

}  // namespace net_instaweb
