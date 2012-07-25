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
// Author: bharathbhushan@google.com (Bharath Bhushan)

#include "net/instaweb/rewriter/public/insert_dns_prefetch_filter.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/timer.h"

// TODO(bharathbhushan): Test interaction with the flush early flow and related
// filters.
// TODO(bharathbhushan): Have a test to ensure that this is the last post render
// filter.
// TODO(bharathbhushan): Add a test for noscript.

namespace {
const int64 kOriginTtlMs = 12 * net_instaweb::Timer::kMinuteMs;
const char kJsData[] =
    "alert     (    'hello, world!'    ) "
    " /* removed */ <!-- removed --> "
    " // single-line-comment";
}

namespace net_instaweb {

class InsertDnsPrefetchFilterTest : public ResourceManagerTestBase {
 public:
  InsertDnsPrefetchFilterTest()
      : writer_(&output_), filter_(NULL) {
  }

 protected:
  virtual void SetUp() {
    options()->EnableFilter(RewriteOptions::kInsertDnsPrefetch);
    ResourceManagerTestBase::SetUp();
    rewrite_driver()->AddFilters();
    rewrite_driver()->SetWriter(&writer_);
  }

  virtual void TearDown() {
    filter_.reset(NULL);
    ResourceManagerTestBase::TearDown();
  }

  void CheckPrefetchInfo(int num_domains_in_current_rewrite,
             int num_domains_in_previous_rewrite,
             int num_domains_to_store, const GoogleString& stored_domains_str) {
    StringPieceVector stored_domains;
    SplitStringPieceToVector(stored_domains_str, ",", &stored_domains, true);
    ASSERT_EQ(num_domains_to_store, stored_domains.size());
    FlushEarlyInfo* info = rewrite_driver()->flush_early_info();
    EXPECT_EQ(num_domains_in_current_rewrite,
              info->total_dns_prefetch_domains());
    EXPECT_EQ(num_domains_in_previous_rewrite,
              info->total_dns_prefetch_domains_previous());
    EXPECT_EQ(num_domains_to_store, info->dns_prefetch_domains_size());
    for (int i = 0; i < info->dns_prefetch_domains_size(); ++i) {
      EXPECT_EQ(stored_domains[i], info->dns_prefetch_domains(i));
    }
  }

  GoogleString CreateHtml(int num_scripts) {
    GoogleString html = "<head><script></script></head><body>";
    for (int i = 1; i <= num_scripts; ++i) {
      StrAppend(&html, "<script src=\"http://", IntegerToString(i),
                ".com/\"/>");
    }
    StrAppend(&html, "</body>");
    return html;
  }

  GoogleString CreateHtmlWithPrefetchTags(int num_scripts, int num_tags) {
    GoogleString html = "<head><script></script>";
    for (int i = 1; i <= num_tags; ++i) {
      StrAppend(&html, "<link rel=\"dns-prefetch\" href=\"//",
                IntegerToString(i), ".com\">");
    }
    StrAppend(&html, "</head><body>");
    for (int i = 1; i <= num_scripts; ++i) {
      StrAppend(&html, "<script src=\"http://", IntegerToString(i),
                ".com/\"/>");
    }
    html += "</body>";
    return html;
  }

  GoogleString CreateDomainsVector(int num_domains) {
    GoogleString result;
    for (int i = 1; i <= num_domains; ++i) {
      StrAppend(&result, IntegerToString(i), ".com,");
    }
    LOG(INFO) << "CreateDomainsVector: " << result;
    return result;
  }

  GoogleString output_;

 private:
  StringWriter writer_;
  ResponseHeaders headers_;
  scoped_ptr<InsertDnsPrefetchFilter> filter_;

  DISALLOW_COPY_AND_ASSIGN(InsertDnsPrefetchFilterTest);
};

TEST_F(InsertDnsPrefetchFilterTest, IgnoreDomainsInHead) {
  GoogleString html =
      "<head>"
        "<link type=\"text/css\" rel=\"stylesheet\" href=\"http://a.com/\">"
        "<script src=\"http://b.com/\"/>"
        "<link rel=\"dns-prefetch\" href=\"http://c.com\">"
      "</head><body></body>";
  Parse("ignore_domains_in_head", html);
  EXPECT_EQ(StrCat("<html>\n", html, "\n</html>"), output_);
  CheckPrefetchInfo(0, 0, 0, "");
}

TEST_F(InsertDnsPrefetchFilterTest, StoreDomainsInBody) {
  GoogleString html =
      "<head></head>"
      "<body>"
        "<link type=\"text/css\" rel=\"stylesheet\" href=\"http://a.com/\">"
        "<script src=\"http://b.com/\"/>"
        "<img src=\"http://c.com/\"/>"
      "</body>";
  Parse("store_domains_in_body", html);
  EXPECT_EQ(StrCat("<html>\n", html, "\n</html>"), output_);
  CheckPrefetchInfo(3, 0, 3, "a.com,b.com,c.com");
}

TEST_F(InsertDnsPrefetchFilterTest, StoreDomainsOnlyInBody) {
  GoogleString html =
      "<head>"
        "<script src=\"http://b.com/\"/>"
      "</head>"
      "<body>"
        "<link type=\"text/css\" rel=\"stylesheet\" href=\"http://a.com/\">"
        "<script src=\"http://b.com/\"/>"
        "<img src=\"http://c.com/\"/>"
      "</body>";
  Parse("store_domains_in_body", html);
  EXPECT_EQ(StrCat("<html>\n", html, "\n</html>"), output_);
  // b.com is not stored since it is already in HEAD.
  CheckPrefetchInfo(2, 0, 2, "a.com,c.com");
}

TEST_F(InsertDnsPrefetchFilterTest, StoreDomainsInBodyMax) {
  GoogleString html(CreateHtml(10));
  Parse("store_domains_in_body_max", html);
  EXPECT_EQ(StrCat("<html>\n", html, "\n</html>"), output_);
  // Only 8/10 domains get stored.
  CheckPrefetchInfo(10, 0, 8, CreateDomainsVector(8));
}

// TODO(bharathbhushan): Add tests for all the html tags which can have URI
// attributes.
TEST_F(InsertDnsPrefetchFilterTest, LinkTagTest) {
  GoogleString html =
      "<head>"
        "<script></script>"
        "<link rel=\"alternate\" href=\"http://a.com\">"
        "<link rel=\"author\" href=\"http://b.com\">"
        "<link rel=\"dns-prefetch\" href=\"http://c.com\">"
        "<link rel=\"help\" href=\"http://d.com\">"
        "<link rel=\"icon\" href=\"http://e.com\">"
        "<link rel=\"license\" href=\"http://f.com\">"
        "<link rel=\"next\" href=\"http://g.com\">"
        "<link rel=\"prefetch\" href=\"http://h.com\">"
        "<link rel=\"prev\" href=\"http://i.com\">"
        "<link rel=\"search\" href=\"http://j.com\">"
        "<link rel=\"stylesheet\" href=\"http://k.com\">"
      "</head>"
      "<body>"
        "<script src=\"http://a.com/\"/>"
        "<script src=\"http://b.com/\"/>"
        "<script src=\"http://c.com/\"/>"
        "<script src=\"http://d.com/\"/>"
        "<script src=\"http://e.com/\"/>"
        "<script src=\"http://f.com/\"/>"
        "<script src=\"http://g.com/\"/>"
        "<script src=\"http://h.com/\"/>"
        "<script src=\"http://i.com/\"/>"
        "<script src=\"http://j.com/\"/>"
        "<script src=\"http://k.com/\"/>"
      "</body>";
  Parse("test_different_link_tags", html);
  EXPECT_EQ(StrCat("<html>\n", html, "\n</html>"), output_);
  // The following link types are for resources or relevant to DNS prefetch
  // tags: dns-prefetch, icon, prefetch, stylesheet. The domains in those tags
  // are not stored. The rest of link types have hyperlinks and their domains
  // get stored.
  CheckPrefetchInfo(7, 0, 7, "a.com,b.com,d.com,f.com,g.com,i.com,j.com");
}

TEST_F(InsertDnsPrefetchFilterTest, FullFlowTest) {
  GoogleString html_input = CreateHtml(10);
  Parse("store_8_of_10", html_input);
  EXPECT_EQ(StrCat("<html>\n", html_input, "\n</html>"), output_);
  CheckPrefetchInfo(10, 0, 8, CreateDomainsVector(8));
  output_.clear();

  html_input = CreateHtml(9);
  Parse("store_8_of_9", html_input);
  EXPECT_EQ(StrCat("<html>\n", html_input, "\n</html>"), output_);
  CheckPrefetchInfo(9, 10, 8, CreateDomainsVector(8));
  output_.clear();

  html_input = CreateHtml(6);
  // 8 DNS prefetch tags inserted since the difference in the number of domains
  // in the last two rewrites (10, 9) is <= 2 and we had stored 8 domains in the
  // previous rewrite. This is the common case.
  // In this rewrite we have an unstable response, whose effect shows up in the
  // next rewrite.
  GoogleString html_output = CreateHtmlWithPrefetchTags(6, 8);
  Parse("stable_domain_list_so_insert_tags", html_input);
  EXPECT_EQ(StrCat("<html>\n", html_output, "\n</html>"), output_);
  CheckPrefetchInfo(6, 9, 6, CreateDomainsVector(6));
  output_.clear();

  // Since the last response caused instability in the domain list, we don't
  // insert any prefetch tags in this rewrite.
  Parse("after_unstable_response", html_input);
  EXPECT_EQ(StrCat("<html>\n", html_input, "\n</html>"), output_);
  CheckPrefetchInfo(6, 6, 6, CreateDomainsVector(6));
  output_.clear();
}

}  // namespace net_instaweb
