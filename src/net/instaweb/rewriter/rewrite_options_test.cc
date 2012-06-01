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
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/string.h"

namespace {

using net_instaweb::MessageHandler;
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

  void MergeOptions(const RewriteOptions& one, const RewriteOptions& two) {
    options_.Merge(one);
    options_.Merge(two);
  }

  // Tests either SetOptionFromName or SetOptionFromNameAndLog depending
  // on 'test_log_variant'
  void TestNameSet(RewriteOptions::OptionSettingResult expected_result,
                   bool test_log_variant,
                   const StringPiece& name,
                   const StringPiece& value,
                   MessageHandler* handler) {
    if (test_log_variant) {
      bool expected = (expected_result == RewriteOptions::kOptionOk);
      EXPECT_EQ(
          expected,
          options_.SetOptionFromNameAndLog(name, value.as_string(), handler));
    } else {
      GoogleString msg;
      EXPECT_EQ(expected_result,
                options_.SetOptionFromName(name, value.as_string(), &msg));
      // Should produce a message exactly when not OK.
      EXPECT_EQ(expected_result != RewriteOptions::kOptionOk, !msg.empty())
          << msg;
    }
  }

  void TestSetOptionFromName(bool test_log_variant);

  RewriteOptions options_;
  net_instaweb::MockHasher hasher_;
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
  s.insert(RewriteOptions::kConvertGifToPng);
  s.insert(RewriteOptions::kInlineImages);
  s.insert(RewriteOptions::kRecompressJpeg);
  s.insert(RewriteOptions::kRecompressPng);
  s.insert(RewriteOptions::kRecompressWebp);
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

TEST_F(RewriteOptionsTest, CompoundFlagRecompressImages) {
  FilterSet s;
  s.insert(RewriteOptions::kConvertGifToPng);
  s.insert(RewriteOptions::kRecompressJpeg);
  s.insert(RewriteOptions::kRecompressPng);
  s.insert(RewriteOptions::kRecompressWebp);
  s.insert(RewriteOptions::kHtmlWriterFilter);  // enabled by default
  const char* kList = "recompress_images";
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
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kPassThrough, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsOneCore) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsOneCoreTwoPass) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.SetRewriteLevel(RewriteOptions::kPassThrough);  // overrides default
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kPassThrough, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsOnePassTwoCore) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kPassThrough);  // overrides default
  two.SetRewriteLevel(RewriteOptions::kCoreFilters);  // overrides one
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsBothCore) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.SetRewriteLevel(RewriteOptions::kCoreFilters);
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
}

TEST_F(RewriteOptionsTest, MergeFilterPassThrough) {
  RewriteOptions one, two;
  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterEnaOne) {
  RewriteOptions one, two;
  one.EnableFilter(RewriteOptions::kAddHead);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterEnaTwo) {
  RewriteOptions one, two;
  two.EnableFilter(RewriteOptions::kAddHead);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterEnaOneDisTwo) {
  RewriteOptions one, two;
  one.EnableFilter(RewriteOptions::kAddHead);
  two.DisableFilter(RewriteOptions::kAddHead);
  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterDisOneEnaTwo) {
  RewriteOptions one, two;
  one.DisableFilter(RewriteOptions::kAddHead);
  two.EnableFilter(RewriteOptions::kAddHead);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeCoreFilter) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterEnaOne) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.EnableFilter(RewriteOptions::kExtendCacheCss);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterEnaTwo) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.EnableFilter(RewriteOptions::kExtendCacheCss);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterEnaOneDisTwo) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.EnableFilter(RewriteOptions::kExtendCacheImages);
  two.DisableFilter(RewriteOptions::kExtendCacheImages);
  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheImages));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterDisOne) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.DisableFilter(RewriteOptions::kExtendCacheCss);
  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterDisOneEnaTwo) {
  RewriteOptions one, two;
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.DisableFilter(RewriteOptions::kExtendCacheScripts);
  two.EnableFilter(RewriteOptions::kExtendCacheScripts);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheScripts));
}

TEST_F(RewriteOptionsTest, MergeThresholdDefault) {
  RewriteOptions one, two;
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kDefaultCssInlineMaxBytes,
            options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeThresholdOne) {
  RewriteOptions one, two;
  one.set_css_inline_max_bytes(5);
  MergeOptions(one, two);
  EXPECT_EQ(5, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeThresholdTwo) {
  RewriteOptions one, two;
  two.set_css_inline_max_bytes(6);
  MergeOptions(one, two);
  EXPECT_EQ(6, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeThresholdOverride) {
  RewriteOptions one, two;
  one.set_css_inline_max_bytes(5);
  two.set_css_inline_max_bytes(6);
  MergeOptions(one, two);
  EXPECT_EQ(6, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampDefault) {
  RewriteOptions one, two;
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kDefaultCacheInvalidationTimestamp,
            options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampOne) {
  RewriteOptions one, two;
  one.set_cache_invalidation_timestamp(11111111);
  MergeOptions(one, two);
  EXPECT_EQ(11111111, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampTwo) {
  RewriteOptions one, two;
  two.set_cache_invalidation_timestamp(22222222);
  MergeOptions(one, two);
  EXPECT_EQ(22222222, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampOneLarger) {
  RewriteOptions one, two;
  one.set_cache_invalidation_timestamp(33333333);
  two.set_cache_invalidation_timestamp(22222222);
  MergeOptions(one, two);
  EXPECT_EQ(33333333, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampTwoLarger) {
  RewriteOptions one, two;
  one.set_cache_invalidation_timestamp(11111111);
  two.set_cache_invalidation_timestamp(22222222);
  MergeOptions(one, two);
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

  MergeOptions(one, two);
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
  MergeOptions(one, two);

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

TEST_F(RewriteOptionsTest, SetDefaultRewriteLevel) {
  NullMessageHandler handler;
  RewriteOptions new_options;
  new_options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);

  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  options_.Merge(new_options);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

void RewriteOptionsTest::TestSetOptionFromName(bool test_log_variant) {
  NullMessageHandler handler;

  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "CssInlineMaxBytes",
              "1024",
              &handler);
  // Default for this is 2048.
  EXPECT_EQ(1024L, options_.css_inline_max_bytes());

  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "JpegRecompressionQuality",
              "1",
              &handler);
  // Default is -1.
  EXPECT_EQ(1, options_.image_jpeg_recompress_quality());

  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "CombineAcrossPaths",
              "false",
              &handler);
  // Default is true
  EXPECT_EQ(false, options_.combine_across_paths());

  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "BeaconUrl",
              "http://www.example.com/beacon",
              &handler);
  EXPECT_EQ("http://www.example.com/beacon", options_.beacon_url().http);
  EXPECT_EQ("https://www.example.com/beacon", options_.beacon_url().https);
  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "BeaconUrl",
              "http://www.example.com/beacon2 https://www.example.com/beacon3",
              &handler);
  EXPECT_EQ("http://www.example.com/beacon2", options_.beacon_url().http);
  EXPECT_EQ("https://www.example.com/beacon3", options_.beacon_url().https);
  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "BeaconUrl",
              "/pagespeed_beacon?",
              &handler);
  EXPECT_EQ("/pagespeed_beacon?", options_.beacon_url().http);
  EXPECT_EQ("/pagespeed_beacon?", options_.beacon_url().https);

  RewriteOptions::RewriteLevel old_level = options_.level();
  TestNameSet(RewriteOptions::kOptionValueInvalid,
              test_log_variant,
              "RewriteLevel",
              "does_not_work",
              &handler);
  EXPECT_EQ(old_level, options_.level());

  TestNameSet(RewriteOptions::kOptionNameUnknown,
              test_log_variant,
              "InvalidName",
              "example",
              &handler);

  TestNameSet(RewriteOptions::kOptionValueInvalid,
              test_log_variant,
              "JsInlineMaxBytes",
              "NOT_INT",
              &handler);
  EXPECT_EQ(RewriteOptions::kDefaultJsInlineMaxBytes,
            options_.js_inline_max_bytes());  // unchanged from default.
}

TEST_F(RewriteOptionsTest, SetOptionFromName) {
  TestSetOptionFromName(false);
}

TEST_F(RewriteOptionsTest, SetOptionFromNameAndLog) {
  TestSetOptionFromName(true);
}

// All the option names are explicitly enumerated here. Modifications are
// handled by the explicit tests. Additions/deletions are handled by checking
// kEndOfOptions explicitly (and assuming we add/delete an option value when we
// add/delete an option name).
TEST_F(RewriteOptionsTest, LookupOptionEnumTest) {
  RewriteOptions::Initialize();
  EXPECT_EQ(80, RewriteOptions::kEndOfOptions);
  EXPECT_EQ(StringPiece("AboveTheFoldCacheTime"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kPrioritizeVisibleContentCacheTime));
  EXPECT_EQ(StringPiece("AboveTheFoldNonCacheableElements"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kPrioritizeVisibleContentNonCacheableElements));
  EXPECT_EQ(StringPiece("AjaxRewritingEnabled"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kAjaxRewritingEnabled));
  EXPECT_EQ(StringPiece("AlwaysRewriteCss"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kAlwaysRewriteCss));
  EXPECT_EQ(StringPiece("AnalyticsID"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kAnalyticsID));
  EXPECT_EQ(StringPiece("AvoidRenamingIntrospectiveJavascript"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kAvoidRenamingIntrospectiveJavascript));
  EXPECT_EQ(StringPiece("BeaconUrl"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kBeaconUrl));
  EXPECT_EQ(StringPiece("BotdetectEnabled"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kBotdetectEnabled));
  EXPECT_EQ(StringPiece("CombineAcrossPaths"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kCombineAcrossPaths));
  EXPECT_EQ(StringPiece("CriticalImagesCacheExpirationTimeMs"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kCriticalImagesCacheExpirationTimeMs));
  EXPECT_EQ(StringPiece("CssImageInlineMaxBytes"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kCssImageInlineMaxBytes));
  EXPECT_EQ(StringPiece("CssInlineMaxBytes"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kCssInlineMaxBytes));
  EXPECT_EQ(StringPiece("CssOutlineMinBytes"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kCssOutlineMinBytes));
  EXPECT_EQ(StringPiece("DefaultCacheHtml"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kDefaultCacheHtml));
  EXPECT_EQ(StringPiece("DomainRewriteHyperlinks"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kDomainRewriteHyperlinks));
  EXPECT_EQ(StringPiece("EnableBlinkCriticalLine"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kEnableBlinkCriticalLine));
  EXPECT_EQ(StringPiece("EnableBlinkForMobileDevices"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kEnableBlinkForMobileDevices));
  EXPECT_EQ(StringPiece("EnableDeferJsExperimental"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kEnableDeferJsExperimental));
  EXPECT_EQ(StringPiece("FallbackRewriteCssUrls"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kFallbackRewriteCssUrls));
  EXPECT_EQ(StringPiece("FlushHtml"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kFlushHtml));
  EXPECT_EQ(StringPiece("IdleFlushTimeMs"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kIdleFlushTimeMs));
  EXPECT_EQ(StringPiece("ImageInlineMaxBytes"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kImageInlineMaxBytes));
  EXPECT_EQ(StringPiece("ImageJpegNumProgressiveScans"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kImageJpegNumProgressiveScans));
  EXPECT_EQ(StringPiece("ImageLimitOptimizedPercent"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kImageLimitOptimizedPercent));
  EXPECT_EQ(StringPiece("ImageLimitResizeAreaPercent"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kImageLimitResizeAreaPercent));
  EXPECT_EQ(StringPiece("ImageMaxRewritesAtOnce"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kImageMaxRewritesAtOnce));
  EXPECT_EQ(StringPiece("ImageRetainColorProfile"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kImageRetainColorProfile));
  EXPECT_EQ(StringPiece("ImageRetainColorSampling"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kImageRetainColorSampling));
  EXPECT_EQ(StringPiece("ImageRetainExifData"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kImageRetainExifData));
  EXPECT_EQ(StringPiece("ImageWebpRecompressQuality"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kImageWebpRecompressQuality));
  EXPECT_EQ(StringPiece("ImplicitCacheTtlMs"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kImplicitCacheTtlMs));
  EXPECT_EQ(StringPiece("JpegRecompressionQuality"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kImageJpegRecompressionQuality));
  EXPECT_EQ(StringPiece("JsInlineMaxBytes"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kJsInlineMaxBytes));
  EXPECT_EQ(StringPiece("JsOutlineMinBytes"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kJsOutlineMinBytes));
  EXPECT_EQ(StringPiece("LazyloadImagesAfterOnload"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kLazyloadImagesAfterOnload));
  EXPECT_EQ(StringPiece("LogRewriteTiming"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kLogRewriteTiming));
  EXPECT_EQ(StringPiece("LowercaseHtmlNames"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kLowercaseHtmlNames));
  EXPECT_EQ(StringPiece("MaxHtmlCacheTimeMs"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kMaxHtmlCacheTimeMs));
  EXPECT_EQ(StringPiece("MaxImageSizeLowResolutionBytes"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kMaxImageSizeLowResolutionBytes));
  EXPECT_EQ(StringPiece("MaxInlinedPreviewImagesIndex"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kMaxInlinedPreviewImagesIndex));
  EXPECT_EQ(StringPiece("MaxSegmentLength"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kMaxUrlSegmentSize));
  EXPECT_EQ(StringPiece("MaxUrlSize"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kMaxUrlSize));
  EXPECT_EQ(StringPiece("MinImageSizeLowResolutionBytes"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kMinImageSizeLowResolutionBytes));
  EXPECT_EQ(StringPiece("MinResourceCacheTimeToRewriteMs"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kMinResourceCacheTimeToRewriteMs));
  EXPECT_EQ(StringPiece("ModifyCachingHeaders"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kModifyCachingHeaders));
  EXPECT_EQ(StringPiece("ProgressiveJpegMinBytes"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kProgressiveJpegMinBytes));
  EXPECT_EQ(StringPiece("RejectBlacklisted"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kRejectBlacklisted));
  EXPECT_EQ(StringPiece("RejectBlacklistedStatusCode"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kRejectBlacklistedStatusCode));
  EXPECT_EQ(StringPiece("RespectVary"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kRespectVary));
  EXPECT_EQ(StringPiece("RewriteLevel"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kRewriteLevel));
  EXPECT_EQ(StringPiece("RunExperiment"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kRunningFurious));
  EXPECT_EQ(StringPiece("ServeBlinkNonCritical"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kServeBlinkNonCritical));
  EXPECT_EQ(StringPiece("ServeStaleIfFetchError"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kServeStaleIfFetchError));
  EXPECT_EQ(StringPiece("UseFixedUserAgentForBlinkCacheMisses"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kUseFixedUserAgentForBlinkCacheMisses));
  EXPECT_EQ(StringPiece("XHeaderValue"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kXModPagespeedHeaderValue));

  EXPECT_EQ(StringPiece("CollectRefererStatistics"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kCollectRefererStatistics));
  EXPECT_EQ(StringPiece("FetchProxy"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kFetcherProxy));
  EXPECT_EQ(StringPiece("FetcherTimeOutMs"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kFetcherTimeOutMs));
  EXPECT_EQ(StringPiece("FileCacheCleanIntervalMs"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kFileCacheCleanIntervalMs));
  EXPECT_EQ(StringPiece("FileCachePath"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kFileCachePath));
  EXPECT_EQ(StringPiece("FileCacheSizeKb"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kFileCacheCleanSizeKb));
  EXPECT_EQ(StringPiece("GeneratedFilePrefix"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kFileNamePrefix));
  EXPECT_EQ(StringPiece("HashRefererStatistics"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kHashRefererStatistics));
  EXPECT_EQ(StringPiece("LRUCacheByteLimit"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kLruCacheByteLimit));
  EXPECT_EQ(StringPiece("LRUCacheKbPerProcess"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kLruCacheKbPerProcess));
  EXPECT_EQ(StringPiece("RefererStatisticsOutputLevel"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kRefererStatisticsOutputLevel));
  EXPECT_EQ(StringPiece("SharedMemoryLocks"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kUseSharedMemLocking));
  EXPECT_EQ(StringPiece("SlurpDirectory"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kSlurpDirectory));
  EXPECT_EQ(StringPiece("SlurpFlushLimit"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kSlurpFlushLimit));
  EXPECT_EQ(StringPiece("SlurpReadOnly"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kSlurpReadOnly));
  EXPECT_EQ(StringPiece("Statistics"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kStatisticsEnabled));
  EXPECT_EQ(StringPiece("TestProxy"),
            RewriteOptions::LookupOptionEnum(
                RewriteOptions::kTestProxy));
}

TEST_F(RewriteOptionsTest, PrioritizeCacheableFamilies1) {
  // Default matches nothing.
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "MatchesNothing"));
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(""));

  // Set explicitly.
  options_.AddToPrioritizeVisibleContentCacheableFamilies("zero*?");
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "MatchesNothing"));
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(""));
  EXPECT_TRUE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "zero1"));
  EXPECT_TRUE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "zerooooo1"));

  // Merge in an options with default cacheable families.  This should not
  // affect options_.
  RewriteOptions options1;
  options_.Merge(options1);
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "MatchesNothing"));
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(""));
  EXPECT_TRUE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "zero1"));
  EXPECT_TRUE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "zerooooo1"));

  // Merge in an options with explicit options.
  options1.AddToPrioritizeVisibleContentCacheableFamilies("one?");
  options1.AddToPrioritizeVisibleContentCacheableFamilies("?two*");
  options1.AddToPrioritizeVisibleContentCacheableFamilies("three");
  options_.Merge(options1);
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "MatchesNothing"));
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(""));
  EXPECT_TRUE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "zero1"));
  EXPECT_TRUE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "zerooooo1"));
  EXPECT_TRUE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "one1"));
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "one"));
  EXPECT_TRUE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "2two"));
  EXPECT_TRUE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "2twoANYTHING"));
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "twoANYTHING"));
  EXPECT_TRUE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "three"));
}

TEST_F(RewriteOptionsTest, PrioritizeCacheableFamilies2) {
  // Default matches nothing.
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "MatchesNothing"));
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(""));

  // Merge in an options with default cacheable families.  This should not
  // affect options_.
  RewriteOptions options1;
  options_.Merge(options1);
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "MatchesNothing"));
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(""));

  // Merge in an options with explicit options.
  options1.AddToPrioritizeVisibleContentCacheableFamilies("one?");
  options1.AddToPrioritizeVisibleContentCacheableFamilies("?two*");
  options1.AddToPrioritizeVisibleContentCacheableFamilies("three");
  options_.Merge(options1);
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "MatchesNothing"));
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(""));
  EXPECT_TRUE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "one1"));
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "one"));
  EXPECT_TRUE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "2two"));
  EXPECT_TRUE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "2twoANYTHING"));
  EXPECT_FALSE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "twoANYTHING"));
  EXPECT_TRUE(options_.MatchesPrioritizeVisibleContentCacheableFamilies(
      "three"));
}

TEST_F(RewriteOptionsTest, FuriousSpecTest) {
  // Test that we handle furious specs properly, and that when
  // we set the options to one experiment or another, it works.
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.set_ga_id("UA-111111-1");
  // Set the default slot to 4.
  options_.set_furious_ga_slot(4);
  EXPECT_FALSE(options_.AddFuriousSpec("id=0", &handler));
  EXPECT_TRUE(options_.AddFuriousSpec(
      "id=7;percent=10;level=CoreFilters;enabled=sprite_images;"
      "disabled=inline_css;inline_js=600000", &handler));

  // Extra spaces to test whitespace handling.
  EXPECT_TRUE(options_.AddFuriousSpec("id=2;    percent=15;ga=UA-2222-1;"
                                      "disabled=insert_ga ;slot=3;",
                                      &handler));

  // Invalid slot - make sure the spec still gets added, and the slot defaults
  // to the global slot (4).
  EXPECT_TRUE(options_.AddFuriousSpec("id=17;percent=3;slot=8", &handler));

  options_.SetFuriousState(7);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kSpriteImages));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineCss));
  // This experiment didn't have a ga_id, so make sure we still have the
  // global ga_id.
  EXPECT_EQ("UA-111111-1", options_.ga_id());
  EXPECT_EQ(4, options_.furious_ga_slot());

  // insert_ga can not be disabled in any furious experiment because
  // that filter injects the instrumentation we use to collect the data.
  options_.SetFuriousState(2);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineCss));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kSpriteImages));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kLeftTrimUrls));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInsertGA));
  EXPECT_EQ(3, options_.furious_ga_slot());
  // This experiment specified a ga_id, so make sure that we set it.
  EXPECT_EQ("UA-2222-1", options_.ga_id());

  options_.SetFuriousState(17);
  EXPECT_EQ(4, options_.furious_ga_slot());
}

TEST_F(RewriteOptionsTest, FuriousPrintTest) {
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.set_ga_id("UA-111111-1");
  options_.set_running_furious_experiment(true);
  EXPECT_FALSE(options_.AddFuriousSpec("id=2;enabled=rewrite_css;", &handler));
  EXPECT_TRUE(options_.AddFuriousSpec("id=1;percent=15;default", &handler));
  EXPECT_TRUE(options_.AddFuriousSpec("id=7;percent=15;level=AllFilters;",
                                      &handler));
  EXPECT_TRUE(options_.AddFuriousSpec("id=2;percent=15;enabled=rewrite_css;"
                                      "inline_css=4096;ga_id=122333-4",
                                      &handler));
  options_.SetFuriousState(-7);
  // This should be the core filters.
  EXPECT_EQ("ah,cc,gp,mc,ec,ei,es,hw,ci,ii,il,ji,rj,rp,rw,ri,cf,jm,cu,"
            "css:2048,im:2048,js:2048;", options_.ToExperimentDebugString());
  EXPECT_EQ("", options_.ToExperimentString());
  options_.SetFuriousState(1);
  EXPECT_EQ("Experiment: 1; ah,ai,cc,gp,mc,ec,ei,es,hw,ci,ii,il,ji,ig,rj,"
            "rp,rw,ri,cf,jm,cu,css:2048,im:2048,js:2048;",
            options_.ToExperimentDebugString());
  EXPECT_EQ("Experiment: 1", options_.ToExperimentString());
  options_.SetFuriousState(7);
  // This should be all non-dangerous filters.
  EXPECT_EQ("Experiment: 7; ab,ah,ai,cw,cc,ch,jc,gp,jp,jw,mc,pj,db,di,ea,ec,ei,"
            "es,if,hw,ci,ii,il,ji,ig,id,tu,ls,ga,cj,cm,co,jo,pv,rj,rp,rw,rc,rq,"
            "ri,rm,cf,rd,jm,cs,cu,is,css:2048,im:2048,js:2048;",
            options_.ToExperimentDebugString());
  EXPECT_EQ("Experiment: 7", options_.ToExperimentString());
  options_.SetFuriousState(2);
  // This should be the filters we need to run an experiment (add_head,
  // add_instrumentation, html_writer, insert_ga) plus rewrite_css.
  // The image inline threshold is 0 because ImageInlineMaxBytes()
  // only returns the threshold if inline_images is enabled.
  EXPECT_EQ("Experiment: 2; ah,ai,hw,ig,cf,css:4096,im:0,js:2048;",
            options_.ToExperimentDebugString());
  EXPECT_EQ("Experiment: 2", options_.ToExperimentString());

  // Make sure we set the ga_id to the one specified by spec 2.
  EXPECT_EQ("122333-4", options_.ga_id());
}

// TODO(sriharis):  Add thorough ComputeSignature tests

TEST_F(RewriteOptionsTest, ComputeSignatureWildcardGroup) {
  // hasher_ is a MockHasher and always returns 0.  This is fine for this test
  // (and all tests that do not depend on Option<GoogleString>'s signature
  // changing with change in value).  But if hasher is used more widely in
  // ComputeSignature we need to revisit the usage of MockHasher here.
  options_.ComputeSignature(&hasher_);
  GoogleString signature1 = options_.signature();
  // Tweak allow_resources_ and check that signature changes.
  options_.ClearSignatureForTesting();
  options_.Disallow("http://www.example.com/*");
  options_.ComputeSignature(&hasher_);
  GoogleString signature2 = options_.signature();
  EXPECT_NE(signature1, signature2);
  // Tweak retain_comments and check that signature changes.
  options_.ClearSignatureForTesting();
  options_.RetainComment("TEST");
  options_.ComputeSignature(&hasher_);
  GoogleString signature3 = options_.signature();
  EXPECT_NE(signature1, signature3);
  EXPECT_NE(signature2, signature3);
}

TEST_F(RewriteOptionsTest, ComputeSignatureOptionEffect) {
  options_.ClearSignatureForTesting();
  options_.set_css_image_inline_max_bytes(2048);
  options_.set_ajax_rewriting_enabled(false);
  options_.ComputeSignature(&hasher_);
  GoogleString signature1 = options_.signature();

  // Changing an Option used in signature computation will change the signature.
  options_.ClearSignatureForTesting();
  options_.set_css_image_inline_max_bytes(1024);
  options_.ComputeSignature(&hasher_);
  GoogleString signature2 = options_.signature();
  EXPECT_NE(signature1, signature2);

  // Changing an Option not used in signature computation will not change the
  // signature.
  options_.ClearSignatureForTesting();
  options_.set_ajax_rewriting_enabled(true);
  options_.ComputeSignature(&hasher_);
  GoogleString signature3 = options_.signature();

  // See the comment in RewriteOptions::RewriteOptions -- we need to leave
  // signatures sensitive to ajax_rewriting.
  EXPECT_NE(signature2, signature3);
}

TEST_F(RewriteOptionsTest, ImageOptimizableCheck) {
  options_.ClearFilters();
  options_.EnableFilter(RewriteOptions::kRecompressJpeg);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kRecompressJpeg);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kRecompressPng);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kRecompressPng);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kRecompressWebp);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kRecompressWebp);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kConvertGifToPng);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kConvertGifToPng);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kConvertJpegToWebp);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kConvertJpegToWebp);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kConvertPngToJpeg);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kConvertPngToJpeg);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());
}

}  // namespace
