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

#include <cstddef>                     // for NULL
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class RewriteQueryTest : public ::testing::Test {
 protected:
  RewriteOptions* ParseAndScan(const StringPiece& query) {
    QueryParams params;
    params.Parse(query);
    options_.reset(RewriteQuery::Scan(params, &handler_));
    return options_.get();
  }

  GoogleMessageHandler handler_;
  scoped_ptr<RewriteOptions> options_;
};

TEST_F(RewriteQueryTest, Empty) {
  EXPECT_TRUE(ParseAndScan("") == NULL);
}

TEST_F(RewriteQueryTest, Off) {
  RewriteOptions* options = ParseAndScan("ModPagespeed=off");
  ASSERT_TRUE(options != NULL);
  EXPECT_FALSE(options->enabled());
}

TEST_F(RewriteQueryTest, OnWithDefaultFilters) {
  RewriteOptions* options = ParseAndScan("ModPagespeed=on");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kExtendCache));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kResizeImages));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRewriteCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRewriteJavascript));
}

TEST_F(RewriteQueryTest, SetFilters) {
  RewriteOptions* options = ParseAndScan("ModPagespeedFilters=remove_quotes");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kRemoveQuotes));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kExtendCache));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kResizeImages));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kRewriteCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kRewriteJavascript));
}

TEST_F(RewriteQueryTest, Multiple) {
  RewriteOptions* options = ParseAndScan("ModPagespeedFilters=inline_css"
                                         "&ModPagespeedCssInlineMaxBytes=10");
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
                                         "&Unrelated3=value");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kInlineCss));
  EXPECT_EQ(10, options->css_inline_max_bytes());
}

TEST_F(RewriteQueryTest, MultipleBroken) {
  RewriteOptions* options = ParseAndScan("ModPagespeedFilters=inline_css"
                                         "&ModPagespeedCssInlineMaxBytes=10"
                                         "&ModPagespeedFilters=bogus_filter");
  EXPECT_TRUE(options == NULL);
}

TEST_F(RewriteQueryTest, Bots) {
  RewriteOptions* options = ParseAndScan("ModPagespeedDisableForBots=on");
  ASSERT_TRUE(options != NULL);
  EXPECT_TRUE(options->botdetect_enabled());
  options = ParseAndScan("ModPagespeedDisableForBots=off");
  ASSERT_TRUE(options != NULL);
  EXPECT_FALSE(options->botdetect_enabled());
}

}  // namespace net_instaweb
