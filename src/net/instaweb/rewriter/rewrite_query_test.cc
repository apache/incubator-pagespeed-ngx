/*
 * Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/rewrite_query.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class RewriteQueryTest : public ResourceManagerTestBase {
 protected:
  RewriteOptions* ParseAndScan(const StringPiece& in_query,
                               const StringPiece& in_header_string) {
    return ParseAndScan(in_query, in_header_string, NULL, NULL);
  }

  // Parses query-params &/or HTTP headers.  The HTTP headers are just in
  // semicolon-separated format, alternating names & values.  There must be
  // an even number of components, implying an odd number of semicolons.
  RewriteOptions* ParseAndScan(const StringPiece& in_query,
                               const StringPiece& in_header_string,
                               GoogleString* out_query,
                               GoogleString* out_header_string) {
    RequestHeaders request_headers;
    StringPieceVector components;
    SplitStringPieceToVector(in_header_string, ";", &components, true);
    CHECK_EQ(0, components.size() % 2);
    for (int i = 0, n = components.size(); i < n; i += 2) {
      request_headers.Add(components[i], components[i + 1]);
    }
    return ParseAndScan(in_query, &request_headers, out_query,
                        out_header_string);
  }

  RewriteOptions* ParseAndScan(const StringPiece& in_query,
                               RequestHeaders* request_headers,
                               GoogleString* out_query,
                               GoogleString* out_header_string) {
    options_.reset(new RewriteOptions);
    GoogleUrl url(StrCat("http://www.test.com/index.jsp?", in_query));
    if (RewriteQuery::Scan(factory(), &url, request_headers,
                           &options_, &handler_)
        != RewriteQuery::kSuccess) {
      options_.reset(NULL);
    }
    if (out_query != NULL) {
      out_query->assign(url.Query().data(), url.Query().size());
    }
    if (out_header_string != NULL) {
      out_header_string->assign(request_headers->ToString());
    }
    return options_.get();
  }

  void CheckExtendCache(RewriteOptions* options, bool x) {
    EXPECT_EQ(x, options->Enabled(RewriteOptions::kExtendCacheCss));
    EXPECT_EQ(x, options->Enabled(RewriteOptions::kExtendCacheImages));
    EXPECT_EQ(x, options->Enabled(RewriteOptions::kExtendCacheScripts));
  }

  // In a fashion patterned after the usage in mod_instaweb.cc, establish
  // a base configuration, and update it based on the passed-in query string.
  void Incremental(const StringPiece& query, RewriteOptions* options) {
    scoped_ptr<RewriteOptions> query_options;
    GoogleUrl gurl(StrCat("http://example.com/?ModPagespeedFilters=", query));
    RequestHeaders request_headers;
    EXPECT_EQ(RewriteQuery::kSuccess,
              RewriteQuery::Scan(factory(), &gurl, &request_headers,
                                 &query_options, message_handler()));
    options->Merge(*query_options.get());
  }

  GoogleMessageHandler handler_;
  scoped_ptr<RewriteOptions> options_;
};

TEST_F(RewriteQueryTest, Empty) {
  EXPECT_TRUE(ParseAndScan("", "") == NULL);
}

TEST_F(RewriteQueryTest, OffQuery) {
  RewriteOptions* options = ParseAndScan("ModPagespeed=off", "");
  ASSERT_TRUE(options != NULL);
  EXPECT_FALSE(options->enabled());
}

TEST_F(RewriteQueryTest, OffHeaders) {
  RewriteOptions* options = ParseAndScan("", "ModPagespeed;off");
  ASSERT_TRUE(options != NULL);
  EXPECT_FALSE(options->enabled());
}

TEST_F(RewriteQueryTest, OnWithDefaultFiltersQuery) {
  RewriteOptions* options = ParseAndScan("ModPagespeed=on", "");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  CheckExtendCache(options, true);
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kResizeImages));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRewriteCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRewriteJavascript));
}

TEST_F(RewriteQueryTest, OnWithDefaultFiltersHeaders) {
  RewriteOptions* options = ParseAndScan("", "ModPagespeed;on");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  CheckExtendCache(options, true);
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kResizeImages));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRewriteCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRewriteJavascript));
}

TEST_F(RewriteQueryTest, SetFiltersQuery) {
  RewriteOptions* options = ParseAndScan("ModPagespeedFilters=remove_quotes",
                                         "");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRemoveQuotes));
  CheckExtendCache(options, false);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kResizeImages));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kRewriteCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kRewriteJavascript));
}

TEST_F(RewriteQueryTest, SetFiltersQueryCorePlusMinus) {
  RewriteOptions* options = ParseAndScan("ModPagespeedFilters="
                                         "core,+div_structure,-inline_css,"
                                         "+extend_cache_css",
                                         "");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());

  CheckExtendCache(options, true);
  EXPECT_TRUE(options->Enabled(RewriteOptions::kExtendCacheCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kExtendCacheImages));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kDivStructure));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kInlineCss));
  // Unlike above, these are true because 'core' is in the filter list.
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kResizeImages));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRewriteCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRewriteJavascript));
}

TEST_F(RewriteQueryTest, SetFiltersHeaders) {
  RewriteOptions* options = ParseAndScan("",
                                         "ModPagespeedFilters;remove_quotes");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRemoveQuotes));
  CheckExtendCache(options, false);
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kResizeImages));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kRewriteCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kRewriteJavascript));
}

TEST_F(RewriteQueryTest, MultipleQuery) {
  RewriteOptions* options = ParseAndScan("ModPagespeedFilters=inline_css"
                                         "&ModPagespeedCssInlineMaxBytes=10",
                                         "");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kInlineCss));
  EXPECT_EQ(10, options->css_inline_max_bytes());
}

TEST_F(RewriteQueryTest, MultipleHeaders) {
  RewriteOptions* options = ParseAndScan("",
                                         "ModPagespeedFilters;inline_css;"
                                         "ModPagespeedCssInlineMaxBytes;10");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kInlineCss));
  EXPECT_EQ(10, options->css_inline_max_bytes());
}

TEST_F(RewriteQueryTest, MultipleQueryAndHeaders) {
  RewriteOptions* options = ParseAndScan("ModPagespeedFilters=inline_css",
                                         "ModPagespeedCssInlineMaxBytes;10");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kInlineCss));
  EXPECT_EQ(10, options->css_inline_max_bytes());
}

TEST_F(RewriteQueryTest, MultipleIgnoreUnrelated) {
  RewriteOptions* options = ParseAndScan("ModPagespeedFilters=inline_css"
                                         "&ModPagespeedCssInlineMaxBytes=10"
                                         "&Unrelated1"
                                         "&Unrelated2="
                                         "&Unrelated3=value",
                                         "");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kInlineCss));
  EXPECT_EQ(10, options->css_inline_max_bytes());
}

TEST_F(RewriteQueryTest, MultipleBroken) {
  RewriteOptions* options = ParseAndScan("ModPagespeedFilters=inline_css"
                                         "&ModPagespeedCssInlineMaxBytes=10"
                                         "&ModPagespeedFilters=bogus_filter",
                                         "");
  EXPECT_TRUE(options == NULL);
}

TEST_F(RewriteQueryTest, Bots) {
  RewriteOptions* options = ParseAndScan("ModPagespeedDisableForBots=on", "");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->botdetect_enabled());
  options = ParseAndScan("ModPagespeedDisableForBots=off", "");
  ASSERT_TRUE(options != NULL);
  EXPECT_FALSE(options->botdetect_enabled());
}

TEST_F(RewriteQueryTest, MultipleInt64Params) {
  RewriteOptions* options = ParseAndScan("ModPagespeedCssInlineMaxBytes=3"
                                         "&ModPagespeedImageInlineMaxBytes=5"
                                         "&ModPagespeedCssImageInlineMaxBytes=7"
                                         "&ModPagespeedJsInlineMaxBytes=11"
                                         "&ModPagespeedDomainShardCount=2",
                                         "");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_EQ(3, options->css_inline_max_bytes());
  EXPECT_EQ(5, options->ImageInlineMaxBytes());
  EXPECT_EQ(7, options->CssImageInlineMaxBytes());
  EXPECT_EQ(11, options->js_inline_max_bytes());
  EXPECT_EQ(2, options->domain_shard_count());
}

TEST_F(RewriteQueryTest, OutputQueryandHeaders) {
  GoogleString output_query, output_headers;
  ParseAndScan("ModPagespeedCssInlineMaxBytes=3"
               "&ModPagespeedImageInlineMaxBytes=5"
               "&ModPagespeedCssImageInlineMaxBytes=7"
               "&ModPagespeedJsInlineMaxBytes=11"
               "&ModPagespeedDomainShardCount=100"
               "&ModPagespeedCssFlattenMaxBytes=13"
               "&abc=1"
               "&def",
               "ModPagespeedFilters;inline_css;"
               "xyz;6;"
               "ModPagespeedFilters;remove_quotes",
               &output_query, &output_headers);
  EXPECT_EQ(output_query, "abc=1&def");
  EXPECT_EQ(output_headers, "GET  HTTP/1.0\r\nxyz: 6\r\n\r\n");
  ParseAndScan("ModPagespeedCssInlineMaxBytes=3", "",
               &output_query, &output_headers);
  EXPECT_EQ(output_query, "");
}

TEST_F(RewriteQueryTest, OutputQueryandHeadersPostRequest) {
  GoogleString output_query, output_headers;
  RequestHeaders request_headers;
  request_headers.set_method(RequestHeaders::kPost);
  request_headers.Add("ModPagespeedFilters", "inline_css");
  request_headers.Add("xyz", "6");
  request_headers.set_message_body("pqr");
  ParseAndScan("ModPagespeedCssInlineMaxBytes=3"
               "&abc=1"
               "&def",
               &request_headers,
               &output_query, &output_headers);
  EXPECT_EQ(output_query, "abc=1&def");
  EXPECT_EQ(output_headers, "POST  HTTP/1.0\r\nxyz: 6\r\n\r\n");
  EXPECT_EQ(request_headers.message_body(), "pqr");
}

// Tests the ability to add an additional filter on the command-line based
// on whatever set is already installed in the configuration.
TEST_F(RewriteQueryTest, IncrementalAdd) {
  RewriteOptions options;
  options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  options.EnableFilter(RewriteOptions::kStripScripts);
  Incremental("+debug", &options);
  EXPECT_TRUE(options.Enabled(RewriteOptions::kStripScripts));
  EXPECT_TRUE(options.Enabled(RewriteOptions::kDebug));
  EXPECT_TRUE(options.Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options.Enabled(RewriteOptions::kAddBaseTag));
  EXPECT_TRUE(options.modified());
}

// Same exact test as above, except that we omit the "+".  This wipes out
// the explicitly enabled filter in the configuration and also the core
// level.
TEST_F(RewriteQueryTest, NonIncrementalAdd) {
  RewriteOptions options;
  options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  options.EnableFilter(RewriteOptions::kStripScripts);
  Incremental("debug", &options);
  EXPECT_FALSE(options.Enabled(RewriteOptions::kStripScripts));
  EXPECT_TRUE(options.Enabled(RewriteOptions::kDebug));
  EXPECT_FALSE(options.Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options.modified());
}

// In this version we specify nothing, and that should erase the filters.
TEST_F(RewriteQueryTest, IncrementalEmpty) {
  RewriteOptions options;
  options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  options.EnableFilter(RewriteOptions::kStripScripts);
  Incremental("", &options);
  EXPECT_FALSE(options.Enabled(RewriteOptions::kStripScripts));
  EXPECT_FALSE(options.Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options.modified());
}

TEST_F(RewriteQueryTest, IncrementalRemoveExplicit) {
  RewriteOptions options;
  options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  options.EnableFilter(RewriteOptions::kStripScripts);
  Incremental("-strip_scripts", &options);
  EXPECT_FALSE(options.Enabled(RewriteOptions::kStripScripts));
  EXPECT_TRUE(options.Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options.modified());
}

TEST_F(RewriteQueryTest, IncrementalRemoveFromCore) {
  RewriteOptions options;
  options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  options.EnableFilter(RewriteOptions::kStripScripts);
  Incremental("-combine_css", &options);
  EXPECT_TRUE(options.Enabled(RewriteOptions::kStripScripts));
  EXPECT_FALSE(options.Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options.modified());
}

TEST_F(RewriteQueryTest, NoChangesShouldNotModify) {
  RewriteOptions options;
  options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  Incremental("+combine_css", &options);
  EXPECT_FALSE(options.Enabled(RewriteOptions::kStripScripts));
  EXPECT_TRUE(options.Enabled(RewriteOptions::kCombineCss));
  //
  // TODO(jmarantz): We would like at this point to have options show up
  // as unmodified.  However our implementation of query-params parsing
  // does not allow for this at this point, because it doesn't know
  // that it is working with the core filters.  Right now this is not
  // that important as the only usage of RewriteOptions::modified() is
  // in apache/mod_instaweb.cc which is just checking to see if there are
  // any directory-specific options set.
  //
  // EXPECT_FALSE(options.modified());
}

}  // namespace net_instaweb
