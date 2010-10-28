/**
 * Copyright 2010 Google Inc.
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

// Author: bmcquade@google.com (Bryan McQuade)

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"

namespace {

using net_instaweb::NullMessageHandler;
using net_instaweb::RewriteOptions;

class RewriteOptionsTest : public ::testing::Test {
 protected:
  typedef std::set<RewriteOptions::Filter> FilterSet;
  void AssertNoneEnabled() {
    FilterSet s;
    AssertEnabled(s);
  }

  void AssertEnabled(const FilterSet& filters) {
    for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
         f <= RewriteOptions::kLastFilter;
         f = static_cast<RewriteOptions::Filter>(f + 1)) {
      if (filters.find(f) != filters.end()) {
        ASSERT_TRUE(options.Enabled(f));
      } else {
        ASSERT_FALSE(options.Enabled(f));
      }
    }
  }

  RewriteOptions options;
};

TEST_F(RewriteOptionsTest, NoneEnabledByDefault) {
  AssertNoneEnabled();
}

TEST_F(RewriteOptionsTest, InstrumentationDisabled) {
  // Make sure the kCoreFilters enables some filters.
  options.SetRewriteLevel(RewriteOptions::kCoreFilters);
  ASSERT_TRUE(options.Enabled(RewriteOptions::kAddInstrumentation));

  // Now disable all filters and make sure none are enabled.
  for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
       f <= RewriteOptions::kLastFilter;
       f = static_cast<RewriteOptions::Filter>(f + 1)) {
    options.DisableFilter(f);
  }
  AssertNoneEnabled();
}

TEST_F(RewriteOptionsTest, DisableTrumpsEnable) {
  for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
       f <= RewriteOptions::kLastFilter;
       f = static_cast<RewriteOptions::Filter>(f + 1)) {
    options.DisableFilter(f);
    options.EnableFilter(f);
    AssertNoneEnabled();
  }
}

TEST_F(RewriteOptionsTest, CoreFilters) {
  options.SetRewriteLevel(RewriteOptions::kCoreFilters);
  FilterSet s;
  for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
       f <= RewriteOptions::kLastFilter;
       f = static_cast<RewriteOptions::Filter>(f + 1)) {
    if (options.Enabled(f)) {
      s.insert(f);
    }
  }

  // Make sure that more than one filter is enabled in the core filter
  // set.
  ASSERT_GT(s.size(), 1);
}

TEST_F(RewriteOptionsTest, Enable) {
  FilterSet s;
  for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
       f <= RewriteOptions::kLastFilter;
       f = static_cast<RewriteOptions::Filter>(f + 1)) {
    s.insert(f);
    options.EnableFilter(f);
    AssertEnabled(s);
  }
}

TEST_F(RewriteOptionsTest, CommaSeparatedList) {
  FilterSet s;
  s.insert(RewriteOptions::kAddBaseTag);
  s.insert(RewriteOptions::kLeftTrimUrls);
  const char* kList = "add_base_tag,left_trim_urls";
  NullMessageHandler handler;
  ASSERT_TRUE(
      options.EnableFiltersByCommaSeparatedList(kList, &handler));
  AssertEnabled(s);
  ASSERT_TRUE(
      options.DisableFiltersByCommaSeparatedList(kList, &handler));
  AssertNoneEnabled();
}

TEST_F(RewriteOptionsTest, ParseRewriteLevel) {
  RewriteOptions::RewriteLevel level;
  ASSERT_TRUE(RewriteOptions::ParseRewriteLevel("PassThrough", &level));
  ASSERT_EQ(RewriteOptions::kPassThrough, level);

  ASSERT_TRUE(RewriteOptions::ParseRewriteLevel("CoreFilters", &level));
  ASSERT_EQ(RewriteOptions::kCoreFilters, level);

  ASSERT_FALSE(RewriteOptions::ParseRewriteLevel(NULL, &level));
  ASSERT_FALSE(RewriteOptions::ParseRewriteLevel("", &level));
  ASSERT_FALSE(RewriteOptions::ParseRewriteLevel("Garbage", &level));
}

}  // namespace
