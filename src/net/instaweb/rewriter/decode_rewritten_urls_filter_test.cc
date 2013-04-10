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

#include "net/instaweb/rewriter/public/decode_rewritten_urls_filter.h"

#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/enums.pb.h"  // for RewriterApplication_Status, etc
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class DecodeRewrittenUrlsFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    options()->EnableFilter(RewriteOptions::kDecodeRewrittenUrls);
    RewriteTestBase::SetUp();
    rewrite_driver()->AddFilters();
  }

  void ExpectLogRecord(int index, int status) {
    AbstractLogRecord* log_record = rewrite_driver_->log_record();
    const RewriterInfo& rewriter_info =
        log_record->logging_info()->rewriter_info(index);
    EXPECT_EQ("du", rewriter_info.id());
    EXPECT_EQ(status, rewriter_info.status());
  }
};

TEST_F(DecodeRewrittenUrlsFilterTest, TestAll) {
  GoogleString input_html =
      "<html><head>"
      "<link type=\"text/css\" rel=\"stylesheet\" "
      "href=\"http://test.com/a.css\"/>"
      "<link type=\"text/css\" rel=\"stylesheet\" "
      "href=\"b.css.pagespeed.ce.0.css\" media=\"print\"/>"
      "<link type=\"text/css\" rel=\"stylesheet\" "
      "href=\"http://www.test.com/I.e.css+f.css.pagespeed.cc.0.css\" "
      "media=\"print\"/>"
      "</head><body>"
      "<script src=\"http://test.com/c.js.pagespeed.jm.555.js\"></script>"
      "<script src=\"http://test.com/d.js.pagespeed.b.jm.0.js\"></script>"
      "</body></html>";
  GoogleString output_html =
      "<html><head>"
      "<link type=\"text/css\" rel=\"stylesheet\" "
      "href=\"http://test.com/a.css\"/>"
      "<link type=\"text/css\" rel=\"stylesheet\" "
      "href=\"http://test.com/b.css\" media=\"print\"/>"
      "<link type=\"text/css\" rel=\"stylesheet\" "
      "href=\"http://www.test.com/I.e.css+f.css.pagespeed.cc.0.css\" "
      "media=\"print\"/>"
      "</head><body>"
      "<script src=\"http://test.com/c.js\"></script>"
      "<script src=\"http://test.com/d.js\"></script>"
      "</body></html>";
  ValidateExpected("different_urls", input_html, output_html);
  EXPECT_EQ(4, rewrite_driver()->log_record()->logging_info()->
            rewriter_info().size());
  ExpectLogRecord(0, RewriterApplication::APPLIED_OK);
  ExpectLogRecord(1, RewriterApplication::NOT_APPLIED);
  ExpectLogRecord(2, RewriterApplication::APPLIED_OK);
  ExpectLogRecord(3, RewriterApplication::APPLIED_OK);
}

}  // namespace net_instaweb
