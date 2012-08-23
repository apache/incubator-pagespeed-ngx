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

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/timer.h"

namespace {
const int64 kOriginTtlS = 12 * net_instaweb::Timer::kMinuteMs * 1000;
const char kJsData[] =
    "alert     (    'hello, world!'    ) "
    " /* removed */ <!-- removed --> "
    " // single-line-comment";
}

namespace net_instaweb {

class SuppressPreheadFilterTest : public ResourceManagerTestBase {
 public:
  SuppressPreheadFilterTest() : writer_(&output_) {}

  virtual bool AddHtmlTags() const { return false; }

 protected:
  void InitResources() {
    SetResponseWithDefaultHeaders("http://test.com/a.css", kContentTypeCss,
                                  " a ", kOriginTtlS);
    SetResponseWithDefaultHeaders("http://test.com/b.js",
                                  kContentTypeJavascript, kJsData,
                                  kOriginTtlS);
  }

  virtual void SetUp() {
    options()->ClearSignatureForTesting();
    options()->EnableFilter(RewriteOptions::kFlushSubresources);
    options()->ComputeSignature(hasher());
    ResourceManagerTestBase::SetUp();
    rewrite_driver()->AddFilters();
    rewrite_driver()->SetWriter(&writer_);
  }

  GoogleString output_;

 private:
  StringWriter writer_;

  DISALLOW_COPY_AND_ASSIGN(SuppressPreheadFilterTest);
};

TEST_F(SuppressPreheadFilterTest, FlushEarlyHeadSuppress) {
  InitResources();
  const char html_input[] =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
        "<link type=\"text/css\" rel=\"stylesheet\" href=\"a.css\"/>"
        "<script src=\"b.js\"></script>"
      "</head>"
      "<body></body></html>";
  const char html_without_prehead[] =
      "<head>"
        "<link type=\"text/css\" rel=\"stylesheet\" href=\"a.css\"/>"
        "<script src=\"b.js\"></script>"
      "</head>"
      "<body></body></html>";

  Parse("not_flushed_early", html_input);
  EXPECT_EQ(output_buffer_, html_input);


  // SuppressPreheadFilter should have populated the flush_early_proto with the
  // appropriate pre head information.
  EXPECT_EQ("<!DOCTYPE html><html>",
            rewrite_driver()->flush_early_info()->pre_head());

  // pre head is suppressed if the dummy head was flushed early.
  output_.clear();
  rewrite_driver()->set_flushed_early(true);
  Parse("flushed_early", html_input);
  EXPECT_EQ(output_, html_without_prehead);
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
  EXPECT_EQ(output_buffer_, html_input);

  EXPECT_EQ(
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
      "<meta charset=\"UTF-8\">",
      rewrite_driver()->flush_early_info()->content_type_meta_tag());

  // pre head is suppressed if the dummy head was flushed early.
  output_.clear();
  rewrite_driver()->set_flushed_early(true);
  Parse("flushed_early", html_input);
  EXPECT_EQ(output_, html_without_prehead);
}

TEST_F(SuppressPreheadFilterTest, MetaTagsOutsideHead) {
  InitResources();
  const char html_input[] =
      "<!DOCTYPE html>"
      "<html>"
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
      "<head></head>"
      "<body></body></html>";
  const char html_without_prehead[] =
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
      "<head>"
      "</head>"
      "<body></body></html>";

  Parse("not_flushed_early", html_input);
  EXPECT_EQ(output_buffer_, html_input);

  EXPECT_EQ(
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>",
      rewrite_driver()->flush_early_info()->content_type_meta_tag());

  // pre head is suppressed if the dummy head was flushed early.
  output_.clear();
  rewrite_driver()->set_flushed_early(true);
  Parse("flushed_early", html_input);
  EXPECT_EQ(output_, html_without_prehead);
}

TEST_F(SuppressPreheadFilterTest, NoHead) {
  InitResources();
  const char html_input[] =
      "<!DOCTYPE html>"
      "<html>"
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
      "<body></body></html>";
  const char html_input_with_head[] =
      "<!DOCTYPE html>"
      "<html>"
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
      "<head/>"
      "<body></body></html>";
  const char html_without_prehead[] =
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
      "<head/>"
      "<body></body></html>";

  Parse("not_flushed_early", html_input);
  EXPECT_EQ(output_buffer_, html_input_with_head);

  EXPECT_EQ(
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>",
      rewrite_driver()->flush_early_info()->content_type_meta_tag());

  // pre head is suppressed if the dummy head was flushed early.
  output_.clear();
  rewrite_driver()->set_flushed_early(true);
  Parse("flushed_early", html_input);
  EXPECT_EQ(output_, html_without_prehead);
}

}  // namespace net_instaweb
