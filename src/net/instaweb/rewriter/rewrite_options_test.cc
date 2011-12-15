/*
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

#include "net/instaweb/rewriter/public/rewrite_options.h"

#include <set>

#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/null_message_handler.h"

namespace {

using net_instaweb::NullMessageHandler;
using net_instaweb::RewriteOptions;

class RewriteOptionsTest : public ::testing::Test {
 protected:
  typedef std::set<RewriteOptions::Filter> FilterSet;
  bool NoneEnabled() {
    FilterSet s;
    return OnlyEnabled(s);
  }

  bool OnlyEnabled(const FilterSet& filters) {
    bool ret = true;
    for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
         ret && (f < RewriteOptions::kEndOfFilters);
         f = static_cast<RewriteOptions::Filter>(f + 1)) {
      if (filters.find(f) != filters.end()) {
        if (!options_.Enabled(f)) {
          ret = false;
        }
      } else {
        if (options_.Enabled(f)) {
          ret = false;
        }
      }
    }
    return ret;
  }

  bool OnlyEnabled(RewriteOptions::Filter filter) {
    FilterSet s;
    s.insert(filter);
    return OnlyEnabled(s);
  }

  RewriteOptions options_;
};

TEST_F(RewriteOptionsTest, BotDetectEnabledByDefault) {
  ASSERT_FALSE(options_.botdetect_enabled());
}

TEST_F(RewriteOptionsTest, BotDetectEnable) {
  options_.set_botdetect_enabled(true);
  ASSERT_TRUE(options_.botdetect_enabled());
}

TEST_F(RewriteOptionsTest, BotDetectDisable) {
  options_.set_botdetect_enabled(false);
  ASSERT_FALSE(options_.botdetect_enabled());
}

TEST_F(RewriteOptionsTest, DefaultEnabledFilters) {
  ASSERT_TRUE(OnlyEnabled(RewriteOptions::kHtmlWriterFilter));
}

TEST_F(RewriteOptionsTest, InstrumentationDisabled) {
  // Make sure the kCoreFilters enables some filters.
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheImages));

  // Now disable all filters and make sure none are enabled.
  for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
       f < RewriteOptions::kEndOfFilters;
       f = static_cast<RewriteOptions::Filter>(f + 1)) {
    options_.DisableFilter(f);
  }
  ASSERT_TRUE(NoneEnabled());
}

TEST_F(RewriteOptionsTest, DisableTrumpsEnable) {
  // Disable the default filter.
  options_.DisableFilter(RewriteOptions::kHtmlWriterFilter);
  for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
       f < RewriteOptions::kEndOfFilters;
       f = static_cast<RewriteOptions::Filter>(f + 1)) {
    options_.DisableFilter(f);
    options_.EnableFilter(f);
  }
}

TEST_F(RewriteOptionsTest, ForceEnableFilter) {
  options_.DisableFilter(RewriteOptions::kHtmlWriterFilter);
  options_.EnableFilter(RewriteOptions::kHtmlWriterFilter);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kHtmlWriterFilter));

  options_.ForceEnableFilter(RewriteOptions::kHtmlWriterFilter);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kHtmlWriterFilter));
}

TEST_F(RewriteOptionsTest, CoreFilters) {
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  FilterSet s;
  for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
       f < RewriteOptions::kEndOfFilters;
       f = static_cast<RewriteOptions::Filter>(f + 1)) {
    if (options_.Enabled(f)) {
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
       f < RewriteOptions::kEndOfFilters;
       f = static_cast<RewriteOptions::Filter>(f + 1)) {
    s.insert(f);
    s.insert(RewriteOptions::kHtmlWriterFilter);  // enabled by default
    options_.EnableFilter(f);
    ASSERT_TRUE(OnlyEnabled(s));
  }
}

TEST_F(RewriteOptionsTest, CommaSeparatedList) {
  FilterSet s;
  s.insert(RewriteOptions::kAddInstrumentation);
  s.insert(RewriteOptions::kLeftTrimUrls);
  s.insert(RewriteOptions::kHtmlWriterFilter);  // enabled by default
  const char* kList = "add_instrumentation,trim_urls";
  NullMessageHandler handler;
  ASSERT_TRUE(
      options_.EnableFiltersByCommaSeparatedList(kList, &handler));
  ASSERT_TRUE(OnlyEnabled(s));
  ASSERT_TRUE(
      options_.DisableFiltersByCommaSeparatedList(kList, &handler));
  ASSERT_TRUE(OnlyEnabled(RewriteOptions::kHtmlWriterFilter));  // default
}

TEST_F(RewriteOptionsTest, CompoundFlag) {
  FilterSet s;
  // TODO(jmaessen): add kConvertJpegToWebp here when it becomes part of
  // rewrite_images.
  s.insert(RewriteOptions::kInlineImages);
  s.insert(RewriteOptions::kInsertImageDimensions);
  s.insert(RewriteOptions::kRecompressImages);
  s.insert(RewriteOptions::kResizeImages);
  s.insert(RewriteOptions::kHtmlWriterFilter);  // enabled by default
  const char* kList = "rewrite_images";
  NullMessageHandler handler;
  ASSERT_TRUE(
      options_.EnableFiltersByCommaSeparatedList(kList, &handler));
  ASSERT_TRUE(OnlyEnabled(s));
  ASSERT_TRUE(
      options_.DisableFiltersByCommaSeparatedList(kList, &handler));
  ASSERT_TRUE(OnlyEnabled(RewriteOptions::kHtmlWriterFilter));  // default
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

TEST_F(RewriteOptionsTest, MergeLevelsDefault) {
  RewriteOptions one, two;
  options_.Merge(one, two);
  EXPECT_EQ(RewriteOptions::kPassThrough, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsOneCore) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.Merge(one, two);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsOneCoreTwoPass) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.SetRewriteLevel(RewriteOptions::kPassThrough);  // overrides default
  options_.Merge(one, two);
  EXPECT_EQ(RewriteOptions::kPassThrough, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsOnePassTwoCore) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kPassThrough);  // overrides default
  two.SetRewriteLevel(RewriteOptions::kCoreFilters);  // overrides one
  options_.Merge(one, two);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsBothCore) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.Merge(one, two);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
}

TEST_F(RewriteOptionsTest, MergeFilterPassThrough) {
  RewriteOptions one, two;
  options_.Merge(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterEnaOne) {
  RewriteOptions one, two;
  one.EnableFilter(RewriteOptions::kAddHead);
  options_.Merge(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterEnaTwo) {
  RewriteOptions one, two;
  two.EnableFilter(RewriteOptions::kAddHead);
  options_.Merge(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterEnaOneDisTwo) {
  RewriteOptions one, two;
  one.EnableFilter(RewriteOptions::kAddHead);
  two.DisableFilter(RewriteOptions::kAddHead);
  options_.Merge(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterDisOneEnaTwo) {
  RewriteOptions one, two;
  one.DisableFilter(RewriteOptions::kAddHead);
  two.EnableFilter(RewriteOptions::kAddHead);
  options_.Merge(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeCoreFilter) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.Merge(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterEnaOne) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.EnableFilter(RewriteOptions::kExtendCacheCss);
  options_.Merge(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterEnaTwo) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.EnableFilter(RewriteOptions::kExtendCacheCss);
  options_.Merge(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterEnaOneDisTwo) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.EnableFilter(RewriteOptions::kExtendCacheImages);
  two.DisableFilter(RewriteOptions::kExtendCacheImages);
  options_.Merge(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheImages));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterDisOne) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.DisableFilter(RewriteOptions::kExtendCacheCss);
  options_.Merge(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterDisOneEnaTwo) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.DisableFilter(RewriteOptions::kExtendCacheScripts);
  two.EnableFilter(RewriteOptions::kExtendCacheScripts);
  options_.Merge(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheScripts));
}

TEST_F(RewriteOptionsTest, MergeThresholdDefault) {
  RewriteOptions one, two;
  options_.Merge(one, two);
  EXPECT_EQ(RewriteOptions::kDefaultCssInlineMaxBytes,
            options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeThresholdOne) {
  RewriteOptions one, two;
  one.set_css_inline_max_bytes(5);
  options_.Merge(one, two);
  EXPECT_EQ(5, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeThresholdTwo) {
  RewriteOptions one, two;
  two.set_css_inline_max_bytes(6);
  options_.Merge(one, two);
  EXPECT_EQ(6, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeThresholdOverride) {
  RewriteOptions one, two;
  one.set_css_inline_max_bytes(5);
  two.set_css_inline_max_bytes(6);
  options_.Merge(one, two);
  EXPECT_EQ(6, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampDefault) {
  RewriteOptions one, two;
  options_.Merge(one, two);
  EXPECT_EQ(RewriteOptions::kDefaultCacheInvalidationTimestamp,
            options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampOne) {
  RewriteOptions one, two;
  one.set_cache_invalidation_timestamp(11111111);
  options_.Merge(one, two);
  EXPECT_EQ(11111111, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampTwo) {
  RewriteOptions one, two;
  two.set_cache_invalidation_timestamp(22222222);
  options_.Merge(one, two);
  EXPECT_EQ(22222222, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampOneLarger) {
  RewriteOptions one, two;
  one.set_cache_invalidation_timestamp(33333333);
  two.set_cache_invalidation_timestamp(22222222);
  options_.Merge(one, two);
  EXPECT_EQ(33333333, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampTwoLarger) {
  RewriteOptions one, two;
  one.set_cache_invalidation_timestamp(11111111);
  two.set_cache_invalidation_timestamp(22222222);
  options_.Merge(one, two);
  EXPECT_EQ(22222222, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, Allow) {
  options_.Allow("*.css");
  EXPECT_TRUE(options_.IsAllowed("abcd.css"));
  options_.Disallow("a*.css");
  EXPECT_FALSE(options_.IsAllowed("abcd.css"));
  options_.Allow("ab*.css");
  EXPECT_TRUE(options_.IsAllowed("abcd.css"));
  options_.Disallow("abc*.css");
  EXPECT_FALSE(options_.IsAllowed("abcd.css"));
}

TEST_F(RewriteOptionsTest, MergeAllow) {
  RewriteOptions one, two;
  one.Allow("*.css");
  EXPECT_TRUE(one.IsAllowed("abcd.css"));
  one.Disallow("a*.css");
  EXPECT_FALSE(one.IsAllowed("abcd.css"));

  two.Allow("ab*.css");
  EXPECT_TRUE(two.IsAllowed("abcd.css"));
  two.Disallow("abc*.css");
  EXPECT_FALSE(two.IsAllowed("abcd.css"));

  options_.Merge(one, two);
  EXPECT_FALSE(options_.IsAllowed("abcd.css"));
  EXPECT_FALSE(options_.IsAllowed("abc.css"));
  EXPECT_TRUE(options_.IsAllowed("ab.css"));
  EXPECT_FALSE(options_.IsAllowed("a.css"));
}

TEST_F(RewriteOptionsTest, DisableAllFiltersNotExplicitlyEnabled) {
  RewriteOptions one, two;
  one.EnableFilter(RewriteOptions::kAddHead);
  two.EnableFilter(RewriteOptions::kExtendCacheCss);
  two.DisableAllFiltersNotExplicitlyEnabled();  // Should disable AddHead.
  options_.Merge(one, two);

  // Make sure AddHead enabling didn't leak through.
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddHead));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, DisableAllFiltersOverrideFilterLevel) {
  // Disable the default enabled filter.
  options_.DisableFilter(RewriteOptions::kHtmlWriterFilter);

  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.EnableFilter(RewriteOptions::kAddHead);
  options_.DisableAllFiltersNotExplicitlyEnabled();

  // Check that *only* AddHead is enabled, even though we have CoreFilters
  // level set.
  EXPECT_TRUE(OnlyEnabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, AllDoesNotImplyStripScrips) {
  options_.SetRewriteLevel(RewriteOptions::kAllFilters);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kStripScripts));
}

TEST_F(RewriteOptionsTest, ExplicitlyEnabledDangerousFilters) {
  options_.SetRewriteLevel(RewriteOptions::kAllFilters);
  options_.EnableFilter(RewriteOptions::kStripScripts);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kDivStructure));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kStripScripts));
  options_.EnableFilter(RewriteOptions::kDivStructure);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDivStructure));
}

TEST_F(RewriteOptionsTest, CoreAndNotDangerous) {
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddInstrumentation));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kCombineCss));
}

TEST_F(RewriteOptionsTest, CoreByNameNotLevel) {
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kPassThrough);
  ASSERT_TRUE(options_.EnableFiltersByCommaSeparatedList("core", &handler));

  // Test the same ones as tested in InstrumentationDisabled.
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheImages));

  // Test these for PlusAndMinus validation.
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kDivStructure));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInlineCss));
}

TEST_F(RewriteOptionsTest, PlusAndMinus) {
  const char* kList = "core,+div_structure,-inline_css,+extend_cache_css";
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kPassThrough);
  ASSERT_TRUE(options_.AdjustFiltersByCommaSeparatedList(kList, &handler));

  // Test the same ones as tested in InstrumentationDisabled.
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheImages));

  // These should be opposite from normal.
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDivStructure));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineCss));
}

}  // namespace
