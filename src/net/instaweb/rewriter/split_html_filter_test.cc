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

// Author: rahulbansal@google.com (Rahul Bansal)

#include "net/instaweb/rewriter/public/split_html_filter.h"

#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/critical_line_info.pb.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class AbstractMutex;

namespace {

const char kRequestUrl[] = "http://www.test.com";

const char kHtmlInput[] =
    "<html>"
    "<head>\n"
    "<script>blah</script>"
    "</head>\n"
    "<body>\n"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
      "<div id=\"item\">"
         "<img src=\"image1\">"
         "<img src=\"image2\">"
      "</div>"
      "<span id=\"between\"> This is in between </span>"
      "<div id=\"inspiration\">"
         "<img src=\"image11\">"
      "</div>"
      "<h3 id=\"afterInspirations\"> This is after Inspirations </h3>"
    "</div>"
    "<img id=\"image\" src=\"image_panel.1\">"
    "<h1 id=\"footer\" name style>"
      "This is the footer"
    "</h1>"
    "</body></html>";

const char kSplitHtml[] =
    "<html><head>"
    "\n<script>blah</script><script src=\"/psajs/blink.js\"></script>"
    "<script>pagespeed.deferInit();</script></head>\n"
    "<body>\n"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
      "<div id=\"item\">"
        "<img src=\"image1\">"
        "<img src=\"image2\">"
      "</div>"
      "<span id=\"between\"> This is in between </span>"
      "<!--GooglePanel begin panel-id.0--><!--GooglePanel end panel-id.0-->"
    "</div>"
    "<!--GooglePanel begin panel-id.1--><!--GooglePanel end panel-id.1-->"
    "<h1 id=\"footer\" name style>"
      "This is the footer"
    "</h1>"
    "</body></html>"
     "<script>pagespeed.panelLoader.bufferNonCriticalData([{"
       "\"panel-id.0\":[{\"instance_html\":\"__psa_lt;div id=\\\"inspiration\\\" panel-id=\\\"panel-id.0\\\"__psa_gt;__psa_lt;img src=\\\"image11\\\"__psa_gt;__psa_lt;/div__psa_gt;__psa_lt;h3 id=\\\"afterInspirations\\\" panel-id=\\\"panel-id.0\\\"__psa_gt; This is after Inspirations __psa_lt;/h3__psa_gt;\"}],"
       "\"panel-id.1\":[{\"instance_html\":\"__psa_lt;img id=\\\"image\\\" src=\\\"image_panel.1\\\" panel-id=\\\"panel-id.1\\\"__psa_gt;\"}]}]);"
     "</script>\n"
     "</body></html>\n";

class MockPage : public PropertyPage {
 public:
  MockPage(AbstractMutex* mutex, const StringPiece& key)
      : PropertyPage(mutex, key) {}
  virtual ~MockPage() {}
  virtual void Done(bool valid) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPage);
};


class SplitHtmlFilterTest : public RewriteTestBase {
 public:
  SplitHtmlFilterTest(): writer_(&output_) {}

  virtual bool AddHtmlTags() const { return false; }

 protected:
  virtual void SetUp() {
    delete options_;
    options_ = new RewriteOptions();
    options_->DisableFilter(RewriteOptions::kHtmlWriterFilter);
    RewriteTestBase::SetUp();

    rewrite_driver()->SetWriter(&writer_);
    SplitHtmlFilter* filter = new SplitHtmlFilter(rewrite_driver());
    html_writer_filter_.reset(filter);
    rewrite_driver()->AddFilter(html_writer_filter_.get());

    response_headers_.set_status_code(HttpStatus::kOK);
    response_headers_.SetDateAndCaching(MockTimer::kApr_5_2010_ms, 0);
    rewrite_driver_->set_response_headers_ptr(&response_headers_);
    output_.clear();
  }

  GoogleString output_;

 private:
  StringWriter writer_;
  ResponseHeaders response_headers_;
  SplitHtmlFilter* split_html_filter_;
};

TEST_F(SplitHtmlFilterTest, SplitHtmlWithPropertyCache) {
  PropertyCache* property_cache = server_context_->page_property_cache();
  property_cache->set_enabled(true);
  property_cache->AddCohort(SplitHtmlFilter::kRenderCohort);
  property_cache->AddCohort(RewriteDriver::kDomCohort);

  MockPage* page = new MockPage(factory_->thread_system()->NewMutex(),
                                kRequestUrl);
  rewrite_driver()->set_property_page(page);
  property_cache->Read(page);

  CriticalLineInfo config;
  Panel* panel = config.add_panels();
  panel->set_start_xpath("//div[@id = \"container\"]/div[4]");
  panel = config.add_panels();
  panel->set_start_xpath("//img[3]");
  panel->set_end_marker_xpath("//h1[@id = \"footer\"]");

  GoogleString buf;
  config.SerializeToString(&buf);
  const PropertyCache::Cohort* cohort =
      property_cache->GetCohort(SplitHtmlFilter::kRenderCohort);
  PropertyValue* property_value = page->GetProperty(
      cohort, SplitHtmlFilter::kCriticalLineInfoPropertyName);
  property_cache->UpdateValue(buf, property_value);
  property_cache->WriteCohort(cohort, page);
  Parse("split_with_pcache", kHtmlInput);
  EXPECT_EQ(kSplitHtml, output_);
}

TEST_F(SplitHtmlFilterTest, SplitHtmlWithOptions) {
  options_->set_critical_line_config(
      "//div[@id = \"container\"]/div[4],"
      "//img[3]://h1[@id = \"footer\"]");
  Parse("split_with_options", kHtmlInput);
  EXPECT_EQ(kSplitHtml, output_);
}

TEST_F(SplitHtmlFilterTest, FlushEarlyHeadSuppress) {
  options_->ForceEnableFilter(
      RewriteOptions::RewriteOptions::kFlushSubresources);
  options_->set_critical_line_config(
      "//div[@id = \"container\"]/div[4],"
      "//img[3]://h1[@id = \"footer\"]");

  const char pre_head_input[] = "<!DOCTYPE html><html>";
  const char post_head_input[] =
      "<head>"
        "<link type=\"text/css\" rel=\"stylesheet\" href=\"a.css\"/>"
        "<script src=\"b.js\"></script>"
      "</head>"
      "<body></body></html>";
  const char post_head_output[] =
      "<head>"
      "<link type=\"text/css\" rel=\"stylesheet\" href=\"a.css\"/>"
      "<script src=\"b.js\"></script>"
      "<script src=\"/psajs/blink.js\"></script>"
      "<script>pagespeed.deferInit();</script>"
      "</head><body></body></html>"
      "<script>pagespeed.panelLoader.bufferNonCriticalData([{}]);"
      "</script>\n</body></html>\n";
  GoogleString html_input = StrCat(pre_head_input, post_head_input);

  Parse("not_flushed_early", html_input);
  EXPECT_EQ(StrCat(pre_head_input, post_head_output), output_);

  // SuppressPreheadFilter should have populated the flush_early_proto with the
  // appropriate pre head information.
  EXPECT_EQ(pre_head_input,
            rewrite_driver()->flush_early_info()->pre_head());

  // pre head is suppressed if the dummy head was flushed early.
  output_.clear();
  rewrite_driver()->set_flushed_early(true);
  Parse("flushed_early", html_input);
  EXPECT_EQ(post_head_output, output_);
}

TEST_F(SplitHtmlFilterTest, FlushEarlyDisabled) {
  options_->set_critical_line_config(
      "//div[@id = \"container\"]/div[4],"
      "//img[3]://h1[@id = \"footer\"]");

  const char pre_head_input[] = "<!DOCTYPE html><html>";
  const char post_head_input[] =
      "<head>"
        "<link type=\"text/css\" rel=\"stylesheet\" href=\"a.css\"/>"
        "<script src=\"b.js\"></script>"
      "</head>"
      "<body></body></html>";
  GoogleString html_input = StrCat(pre_head_input, post_head_input);

  Parse("not_flushed_early", html_input);

  // SuppressPreheadFilter should not have populated the flush_early_proto.
  EXPECT_EQ("", rewrite_driver()->flush_early_info()->pre_head());
}

}  // namespace

}  // namespace net_instaweb
