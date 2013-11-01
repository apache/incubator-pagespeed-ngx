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

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/public/experiment_util.h"
#include "net/instaweb/rewriter/public/rewrite_options_test_base.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/null_thread_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class RewriteOptionsTest : public RewriteOptionsTestBase<RewriteOptions> {
 protected:
  typedef RewriteOptions::FilterSet FilterSet;

  RewriteOptionsTest() : options_(&thread_system_) {
  }

  bool NoneEnabled() {
    FilterSet s;
    return OnlyEnabled(s);
  }

  bool OnlyEnabled(const FilterSet& filters) {
    bool ret = true;
    for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
         ret && (f < RewriteOptions::kEndOfFilters);
         f = static_cast<RewriteOptions::Filter>(f + 1)) {
      if (filters.IsSet(f)) {
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
    s.Insert(filter);
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
          options_.SetOptionFromNameAndLog(name, value, handler));
    } else {
      GoogleString msg;
      EXPECT_EQ(expected_result,
                options_.SetOptionFromName(name, value, &msg));
      // Should produce a message exactly when not OK.
      EXPECT_EQ(expected_result != RewriteOptions::kOptionOk, !msg.empty())
          << msg;
    }
  }

  void TestSetOptionFromName(bool test_log_variant);

  NullThreadSystem thread_system_;
  RewriteOptions options_;
  MockHasher hasher_;
};

TEST_F(RewriteOptionsTest, EnabledStates) {
  options_.set_enabled(RewriteOptions::kEnabledUnplugged);
  ASSERT_FALSE(options_.enabled());
  ASSERT_TRUE(options_.unplugged());
  options_.set_enabled(RewriteOptions::kEnabledOff);
  ASSERT_FALSE(options_.enabled());
  ASSERT_FALSE(options_.unplugged());
  options_.set_enabled(RewriteOptions::kEnabledOn);
  ASSERT_TRUE(options_.enabled());
  ASSERT_FALSE(options_.unplugged());
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
      s.Insert(f);
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
    s.Insert(f);
    s.Insert(RewriteOptions::kHtmlWriterFilter);  // enabled by default
    options_.EnableFilter(f);
    ASSERT_TRUE(OnlyEnabled(s));
  }
}

TEST_F(RewriteOptionsTest, CommaSeparatedList) {
  FilterSet s;
  s.Insert(RewriteOptions::kAddInstrumentation);
  s.Insert(RewriteOptions::kLeftTrimUrls);
  s.Insert(RewriteOptions::kHtmlWriterFilter);  // enabled by default
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
  s.Insert(RewriteOptions::kConvertGifToPng);
  s.Insert(RewriteOptions::kConvertJpegToProgressive);
  s.Insert(RewriteOptions::kConvertPngToJpeg);
  s.Insert(RewriteOptions::kInlineImages);
  s.Insert(RewriteOptions::kJpegSubsampling);
  s.Insert(RewriteOptions::kRecompressJpeg);
  s.Insert(RewriteOptions::kRecompressPng);
  s.Insert(RewriteOptions::kRecompressWebp);
  s.Insert(RewriteOptions::kResizeImages);
  s.Insert(RewriteOptions::kStripImageMetaData);
  s.Insert(RewriteOptions::kStripImageColorProfile);
  s.Insert(RewriteOptions::kHtmlWriterFilter);  // enabled by default
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
  s.Insert(RewriteOptions::kConvertGifToPng);
  s.Insert(RewriteOptions::kConvertJpegToProgressive);
  s.Insert(RewriteOptions::kJpegSubsampling);
  s.Insert(RewriteOptions::kRecompressJpeg);
  s.Insert(RewriteOptions::kRecompressPng);
  s.Insert(RewriteOptions::kRecompressWebp);
  s.Insert(RewriteOptions::kStripImageMetaData);
  s.Insert(RewriteOptions::kStripImageColorProfile);
  s.Insert(RewriteOptions::kHtmlWriterFilter);  // enabled by default
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

TEST_F(RewriteOptionsTest, IsRequestDeclined) {
  RewriteOptions one(&thread_system_);
  one.AddRejectedUrlWildcard("*blocked*");
  one.AddRejectedHeaderWildcard(HttpAttributes::kUserAgent,
                                "*blocked UA*");
  one.AddRejectedHeaderWildcard(HttpAttributes::kXForwardedFor,
                                "12.34.13.*");

  RequestHeaders headers;
  headers.Add(HttpAttributes::kUserAgent, "Chrome");
  ASSERT_FALSE(one.IsRequestDeclined("www.test.com/a", &headers));
  ASSERT_TRUE(one.IsRequestDeclined("www.test.com/blocked", &headers));

  headers.Add(HttpAttributes::kUserAgent, "this is blocked UA agent");
  ASSERT_TRUE(one.IsRequestDeclined("www.test.com/a", &headers));

  headers.Add(HttpAttributes::kUserAgent, "Chrome");
  headers.Add(HttpAttributes::kXForwardedFor, "12.34.13.1");
  ASSERT_TRUE(one.IsRequestDeclined("www.test.com/a", &headers));

  headers.Clear();
  ASSERT_FALSE(one.IsRequestDeclined("www.test.com/a", &headers));
}

TEST_F(RewriteOptionsTest, IsRequestDeclinedMerge) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  RequestHeaders headers;
  one.AddRejectedUrlWildcard("http://www.a.com/b/*");
  EXPECT_TRUE(one.IsRequestDeclined("http://www.a.com/b/sdsd123", &headers));
  EXPECT_FALSE(one.IsRequestDeclined("http://www.a.com/", &headers));
  EXPECT_FALSE(one.IsRequestDeclined("http://www.b.com/b/", &headers));

  two.AddRejectedHeaderWildcard(HttpAttributes::kUserAgent, "*Chrome*");
  two.AddRejectedUrlWildcard("http://www.b.com/b/*");
  MergeOptions(one, two);

  EXPECT_TRUE(options_.IsRequestDeclined("http://www.a.com/b/sds13", &headers));
  EXPECT_FALSE(options_.IsRequestDeclined("http://www.a.com/", &headers));
  EXPECT_TRUE(options_.IsRequestDeclined("http://www.b.com/b/", &headers));

  headers.Add(HttpAttributes::kUserAgent, "firefox");
  EXPECT_FALSE(options_.IsRequestDeclined("http://www.a.com/", &headers));

  headers.Add(HttpAttributes::kUserAgent, "abc Chrome 456");
  EXPECT_TRUE(options_.IsRequestDeclined("http://www.a.com/", &headers));
}


TEST_F(RewriteOptionsTest, MergeLevelsDefault) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kPassThrough, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsOneCore) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsOneCoreTwoPass) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.SetRewriteLevel(RewriteOptions::kPassThrough);  // overrides default
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kPassThrough, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsOnePassTwoCore) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kPassThrough);  // overrides default
  two.SetRewriteLevel(RewriteOptions::kCoreFilters);  // overrides one
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsBothCore) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.SetRewriteLevel(RewriteOptions::kCoreFilters);
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
}

TEST_F(RewriteOptionsTest, MergeFilterPassThrough) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterEnaOne) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.EnableFilter(RewriteOptions::kAddHead);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterEnaTwo) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  two.EnableFilter(RewriteOptions::kAddHead);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterEnaOneDisTwo) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.EnableFilter(RewriteOptions::kAddHead);
  two.DisableFilter(RewriteOptions::kAddHead);
  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterDisOneEnaTwo) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.DisableFilter(RewriteOptions::kAddHead);
  two.EnableFilter(RewriteOptions::kAddHead);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeCoreFilter) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterEnaOne) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.EnableFilter(RewriteOptions::kExtendCacheCss);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterEnaTwo) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.EnableFilter(RewriteOptions::kExtendCacheCss);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterEnaOneDisTwo) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.EnableFilter(RewriteOptions::kExtendCacheImages);
  two.DisableFilter(RewriteOptions::kExtendCacheImages);
  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheImages));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterDisOne) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.DisableFilter(RewriteOptions::kExtendCacheCss);
  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterDisOneEnaTwo) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.DisableFilter(RewriteOptions::kExtendCacheScripts);
  two.EnableFilter(RewriteOptions::kExtendCacheScripts);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheScripts));
}

TEST_F(RewriteOptionsTest, MergeThresholdDefault) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kDefaultCssInlineMaxBytes,
            options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeThresholdOne) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.set_css_inline_max_bytes(5);
  MergeOptions(one, two);
  EXPECT_EQ(5, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeThresholdTwo) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  two.set_css_inline_max_bytes(6);
  MergeOptions(one, two);
  EXPECT_EQ(6, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeThresholdOverride) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.set_css_inline_max_bytes(5);
  two.set_css_inline_max_bytes(6);
  MergeOptions(one, two);
  EXPECT_EQ(6, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampDefault) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kDefaultCacheInvalidationTimestamp,
            options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampOne) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.set_cache_invalidation_timestamp(11111111);
  MergeOptions(one, two);
  EXPECT_EQ(11111111, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampTwo) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  two.set_cache_invalidation_timestamp(22222222);
  MergeOptions(one, two);
  EXPECT_EQ(22222222, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampOneLarger) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.set_cache_invalidation_timestamp(33333333);
  two.set_cache_invalidation_timestamp(22222222);
  MergeOptions(one, two);
  EXPECT_EQ(33333333, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampTwoLarger) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.set_cache_invalidation_timestamp(11111111);
  two.set_cache_invalidation_timestamp(22222222);
  MergeOptions(one, two);
  EXPECT_EQ(22222222, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeDistributed) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  EXPECT_FALSE(options_.Distributable(RewriteOptions::kCacheExtenderId));
  EXPECT_FALSE(options_.Distributable(RewriteOptions::kImageCompressionId));
  EXPECT_FALSE(options_.Distributable(RewriteOptions::kCssFilterId));

  one.DistributeFilter(RewriteOptions::kCacheExtenderId);
  two.DistributeFilter(RewriteOptions::kImageCompressionId);
  MergeOptions(one, two);

  EXPECT_TRUE(options_.Distributable(RewriteOptions::kCacheExtenderId));
  EXPECT_TRUE(options_.Distributable(RewriteOptions::kImageCompressionId));
  EXPECT_FALSE(options_.Distributable(RewriteOptions::kCssFilterId));
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
  RewriteOptions one(&thread_system_), two(&thread_system_);
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

TEST_F(RewriteOptionsTest, DisableAllFilters) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.EnableFilter(RewriteOptions::kAddHead);
  two.EnableFilter(RewriteOptions::kExtendCacheCss);
  two.DisableAllFilters();  // Should disable both.
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));

  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddHead));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, DisableAllFiltersNotExplicitlyEnabled) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
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

TEST_F(RewriteOptionsTest, ForbidFilter) {
  // Forbid a core filter: this will disable it.
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.ForbidFilter(RewriteOptions::kExtendCacheCss);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  EXPECT_TRUE(options_.Forbidden(
      RewriteOptions::FilterId(RewriteOptions::kExtendCacheCss)));

  // Forbid a filter, then try to merge in an enablement: it won't take.
  // At the same time, merge in a new "forbiddenment": it will take.
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.ForbidFilter(RewriteOptions::kExtendCacheCss);
  two.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.ForbidFilter(RewriteOptions::kFlattenCssImports);
  one.Merge(two);
  EXPECT_FALSE(one.Enabled(RewriteOptions::kExtendCacheCss));
  EXPECT_FALSE(one.Enabled(RewriteOptions::kFlattenCssImports));
  EXPECT_TRUE(one.Forbidden(
      RewriteOptions::FilterId(RewriteOptions::kExtendCacheCss)));
  EXPECT_TRUE(one.Forbidden(
      RewriteOptions::FilterId(RewriteOptions::kFlattenCssImports)));
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
  RewriteOptions new_options(&thread_system_);
  new_options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);

  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  options_.Merge(new_options);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

void RewriteOptionsTest::TestSetOptionFromName(bool test_log_variant) {
  NullMessageHandler handler;

  // TODO(sriharis): Add tests for all Options here per LookupOptionByNameTest.

  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "FetcherTimeOutMs",
              "1024",
              &handler);
  // Default for this is 5 * Timer::kSecondMs.
  EXPECT_EQ(1024, options_.blocking_fetch_timeout_ms());

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
  EXPECT_FALSE(options_.combine_across_paths());

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

// All the base option names are explicitly enumerated here. Modifications are
// handled by the explicit tests. Additions/deletions are handled by checking
// the count explicitly (and assuming we add/delete an option value when we
// add/delete an option name).
TEST_F(RewriteOptionsTest, LookupOptionByNameTest) {
  // Use macro so that the failure message tells us the name of the option
  // failing the test (using a function would obscure that) and to ensure we
  // add every tested name to the set of tested names for further checks below.
  std::set<StringPiece> tested_names;
#define PassLookupOptionByName(name) \
  EXPECT_TRUE(NULL != RewriteOptions::LookupOptionByName(name)); \
  tested_names.insert(name)

  // Generic options that are registered by name by AddProperty().
  PassLookupOptionByName(RewriteOptions::kAddOptionsToUrls);
  PassLookupOptionByName(RewriteOptions::kAccessControlAllowOrigins);
  PassLookupOptionByName(RewriteOptions::kAllowLoggingUrlsInLogRecord);
  PassLookupOptionByName(RewriteOptions::kAlwaysRewriteCss);
  PassLookupOptionByName(RewriteOptions::kAnalyticsID);
  PassLookupOptionByName(RewriteOptions::kAvoidRenamingIntrospectiveJavascript);
  PassLookupOptionByName(RewriteOptions::kBeaconReinstrumentTimeSec);
  PassLookupOptionByName(RewriteOptions::kBeaconUrl);
  PassLookupOptionByName(RewriteOptions::kBlinkMaxHtmlSizeRewritable);
  PassLookupOptionByName(RewriteOptions::kCacheInvalidationTimestamp);
  PassLookupOptionByName(RewriteOptions::kCacheSmallImagesUnrewritten);
  PassLookupOptionByName(RewriteOptions::kClientDomainRewrite);
  PassLookupOptionByName(RewriteOptions::kCombineAcrossPaths);
  PassLookupOptionByName(RewriteOptions::kCriticalImagesBeaconEnabled);
  PassLookupOptionByName(RewriteOptions::kCriticalLineConfig);
  PassLookupOptionByName(RewriteOptions::kCssFlattenMaxBytes);
  PassLookupOptionByName(RewriteOptions::kCssImageInlineMaxBytes);
  PassLookupOptionByName(RewriteOptions::kCssInlineMaxBytes);
  PassLookupOptionByName(RewriteOptions::kCssOutlineMinBytes);
  PassLookupOptionByName(RewriteOptions::kCssPreserveURLs);
  PassLookupOptionByName(RewriteOptions::kDefaultCacheHtml);
  PassLookupOptionByName(RewriteOptions::kDisableBackgroundFetchesForBots);
  PassLookupOptionByName(RewriteOptions::kDisableRewriteOnNoTransform);
  PassLookupOptionByName(RewriteOptions::kDistributeFetches);
  PassLookupOptionByName(RewriteOptions::kDistributedRewriteKey);
  PassLookupOptionByName(RewriteOptions::kDistributedRewriteServers);
  PassLookupOptionByName(RewriteOptions::kDistributedRewriteTimeoutMs);
  PassLookupOptionByName(RewriteOptions::kDomainRewriteHyperlinks);
  PassLookupOptionByName(RewriteOptions::kDomainShardCount);
  PassLookupOptionByName(RewriteOptions::kUseImageScanlineApi);
  PassLookupOptionByName(RewriteOptions::kDownstreamCachePurgeMethod);
  PassLookupOptionByName(RewriteOptions::
                         kDownstreamCacheRewrittenPercentageThreshold);
  PassLookupOptionByName(RewriteOptions::kEnableAggressiveRewritersForMobile);
  PassLookupOptionByName(RewriteOptions::kEnableBlinkHtmlChangeDetection);
  PassLookupOptionByName(RewriteOptions::
                         kEnableBlinkHtmlChangeDetectionLogging);
  PassLookupOptionByName(RewriteOptions::kEnableCachePurge);
  PassLookupOptionByName(RewriteOptions::kEnableDeferJsExperimental);
  PassLookupOptionByName(RewriteOptions::kEnableExtendedInstrumentation);
  PassLookupOptionByName(RewriteOptions::kEnableFlushEarlyCriticalCss);
  PassLookupOptionByName(RewriteOptions::kEnableLazyLoadHighResImages);
  PassLookupOptionByName(RewriteOptions::kEnablePrioritizingScripts);
  PassLookupOptionByName(RewriteOptions::kEnabled);
  PassLookupOptionByName(RewriteOptions::kExperimentCookieDurationMs);
  PassLookupOptionByName(RewriteOptions::kExperimentSlot);
  PassLookupOptionByName(RewriteOptions::kFetcherTimeOutMs);
  PassLookupOptionByName(RewriteOptions::
                         kFinderPropertiesCacheExpirationTimeMs);
  PassLookupOptionByName(RewriteOptions::kFinderPropertiesCacheRefreshTimeMs);
  PassLookupOptionByName(RewriteOptions::kFlushBufferLimitBytes);
  PassLookupOptionByName(RewriteOptions::kFlushHtml);
  PassLookupOptionByName(RewriteOptions::kFlushMoreResourcesEarlyIfTimePermits);
  PassLookupOptionByName(RewriteOptions::kForbidAllDisabledFilters);
  PassLookupOptionByName(RewriteOptions::kHideRefererUsingMeta);
  PassLookupOptionByName(RewriteOptions::kIdleFlushTimeMs);
  PassLookupOptionByName(RewriteOptions::kImageInlineMaxBytes);
  PassLookupOptionByName(RewriteOptions::kImageJpegNumProgressiveScans);
  PassLookupOptionByName(RewriteOptions::
                         kImageJpegNumProgressiveScansForSmallScreens);
  PassLookupOptionByName(RewriteOptions::kImageJpegRecompressionQuality);
  PassLookupOptionByName(RewriteOptions::
                         kImageJpegRecompressionQualityForSmallScreens);
  PassLookupOptionByName(RewriteOptions::kImageLimitOptimizedPercent);
  PassLookupOptionByName(RewriteOptions::kImageLimitRenderedAreaPercent);
  PassLookupOptionByName(RewriteOptions::kImageLimitResizeAreaPercent);
  PassLookupOptionByName(RewriteOptions::kImageMaxRewritesAtOnce);
  PassLookupOptionByName(RewriteOptions::kImagePreserveURLs);
  PassLookupOptionByName(RewriteOptions::kImageRecompressionQuality);
  PassLookupOptionByName(RewriteOptions::kImageResolutionLimitBytes);
  PassLookupOptionByName(RewriteOptions::kImageWebpRecompressionQuality);
  PassLookupOptionByName(RewriteOptions::
                         kImageWebpRecompressionQualityForSmallScreens);
  PassLookupOptionByName(RewriteOptions::kImageWebpTimeoutMs);
  PassLookupOptionByName(RewriteOptions::kImplicitCacheTtlMs);
  PassLookupOptionByName(RewriteOptions::kIncreaseSpeedTracking);
  PassLookupOptionByName(RewriteOptions::kInlineOnlyCriticalImages);
  PassLookupOptionByName(RewriteOptions::kInPlacePreemptiveRewriteCss);
  PassLookupOptionByName(RewriteOptions::kInPlacePreemptiveRewriteCssImages);
  PassLookupOptionByName(RewriteOptions::kInPlacePreemptiveRewriteImages);
  PassLookupOptionByName(RewriteOptions::kInPlacePreemptiveRewriteJavascript);
  PassLookupOptionByName(RewriteOptions::kInPlaceResourceOptimization);
  PassLookupOptionByName(RewriteOptions::kInPlaceRewriteDeadlineMs);
  PassLookupOptionByName(RewriteOptions::kInPlaceWaitForOptimized);
  PassLookupOptionByName(RewriteOptions::kInlineOnlyCriticalImages);
  PassLookupOptionByName(RewriteOptions::kJsInlineMaxBytes);
  PassLookupOptionByName(RewriteOptions::kJsOutlineMinBytes);
  PassLookupOptionByName(RewriteOptions::kJsPreserveURLs);
  PassLookupOptionByName(RewriteOptions::kLazyloadImagesAfterOnload);
  PassLookupOptionByName(RewriteOptions::kLazyloadImagesBlankUrl);
  PassLookupOptionByName(RewriteOptions::kLogBackgroundRewrite);
  PassLookupOptionByName(RewriteOptions::kLogRewriteTiming);
  PassLookupOptionByName(RewriteOptions::kLogUrlIndices);
  PassLookupOptionByName(RewriteOptions::kLowercaseHtmlNames);
  PassLookupOptionByName(RewriteOptions::kMaxCacheableResponseContentLength);
  PassLookupOptionByName(RewriteOptions::kMaxCombinedCssBytes);
  PassLookupOptionByName(RewriteOptions::kMaxCombinedJsBytes);
  PassLookupOptionByName(RewriteOptions::kMaxLowResImageSizeBytes);
  PassLookupOptionByName(
      RewriteOptions::kMaxLowResToHighResImageSizePercentage);
  PassLookupOptionByName(RewriteOptions::kMaxHtmlCacheTimeMs);
  PassLookupOptionByName(RewriteOptions::kMaxHtmlParseBytes);
  PassLookupOptionByName(RewriteOptions::kMaxImageBytesForWebpInCss);
  PassLookupOptionByName(RewriteOptions::kMaxImageSizeLowResolutionBytes);
  PassLookupOptionByName(RewriteOptions::kMaxInlinedPreviewImagesIndex);
  PassLookupOptionByName(RewriteOptions::kMaxPrefetchJsElements);
  PassLookupOptionByName(RewriteOptions::kMaxRewriteInfoLogSize);
  PassLookupOptionByName(RewriteOptions::kMaxUrlSegmentSize);
  PassLookupOptionByName(RewriteOptions::kMaxUrlSize);
  PassLookupOptionByName(RewriteOptions::kMetadataCacheStalenessThresholdMs);
  PassLookupOptionByName(RewriteOptions::kMinCacheTtlMs);
  PassLookupOptionByName(RewriteOptions::kMinImageSizeLowResolutionBytes);
  PassLookupOptionByName(RewriteOptions::kMinResourceCacheTimeToRewriteMs);
  PassLookupOptionByName(RewriteOptions::kModifyCachingHeaders);
  PassLookupOptionByName(RewriteOptions::kNonCacheablesForCachePartialHtml);
  PassLookupOptionByName(RewriteOptions::kObliviousPagespeedUrls);
  PassLookupOptionByName(RewriteOptions::kOverrideCachingTtlMs);
  PassLookupOptionByName(RewriteOptions::kPersistBlinkBlacklist);
  PassLookupOptionByName(RewriteOptions::kPreserveUrlRelativity);
  PassLookupOptionByName(RewriteOptions::kProactivelyFreshenUserFacingRequest);
  PassLookupOptionByName(RewriteOptions::kProactiveResourceFreshening);
  PassLookupOptionByName(RewriteOptions::kProgressiveJpegMinBytes);
  PassLookupOptionByName(RewriteOptions::kRejectBlacklisted);
  PassLookupOptionByName(RewriteOptions::kRejectBlacklistedStatusCode);
  PassLookupOptionByName(RewriteOptions::kReportUnloadTime);
  PassLookupOptionByName(RewriteOptions::kRespectVary);
  PassLookupOptionByName(RewriteOptions::kRespectXForwardedProto);
  PassLookupOptionByName(RewriteOptions::kRewriteDeadlineMs);
  PassLookupOptionByName(RewriteOptions::kRewriteLevel);
  PassLookupOptionByName(RewriteOptions::kRewriteRandomDropPercentage);
  PassLookupOptionByName(RewriteOptions::kRewriteRequestUrlsEarly);
  PassLookupOptionByName(RewriteOptions::kRewriteUncacheableResources);
  PassLookupOptionByName(RewriteOptions::kRunningExperiment);
  PassLookupOptionByName(RewriteOptions::kServeGhostClickBusterWithSplitHtml);
  PassLookupOptionByName(RewriteOptions::kServeSplitHtmlInTwoChunks);
  PassLookupOptionByName(RewriteOptions::kServeStaleIfFetchError);
  PassLookupOptionByName(RewriteOptions::kServeXhrAccessControlHeaders);
  PassLookupOptionByName(RewriteOptions::
                         kServeStaleWhileRevalidateThresholdSec);
  PassLookupOptionByName(RewriteOptions::kSupportNoScriptEnabled);
  PassLookupOptionByName(RewriteOptions::
                         kTestOnlyPrioritizeCriticalCssDontApplyOriginalCss);
  PassLookupOptionByName(RewriteOptions::kUseBlankImageForInlinePreview);
  PassLookupOptionByName(RewriteOptions::kUseFallbackPropertyCacheValues);
  PassLookupOptionByName(RewriteOptions::kAwaitPcacheLookup);
  PassLookupOptionByName(RewriteOptions::kUseSelectorsForCriticalCss);
  PassLookupOptionByName(RewriteOptions::kUseSmartDiffInBlink);
  PassLookupOptionByName(RewriteOptions::kXModPagespeedHeaderValue);
  PassLookupOptionByName(RewriteOptions::kXPsaBlockingRewrite);

  // Now go through the named options in all_properties_ and check that each
  // one has been tested.
  int named_properties = 0;
  for (int i = 0, n = RewriteOptions::all_properties_->size(); i < n; ++i) {
    StringPiece name =
        RewriteOptions::all_properties_->property(i)->option_name();
    if (!name.empty()) {
      ++named_properties;
      EXPECT_NE(tested_names.end(), tested_names.find(name))
          << name << " has not been tested!";
    }
  }
  EXPECT_EQ(named_properties, tested_names.size());

  // Check that case doesn't matter when looking up directives.
  PassLookupOptionByName("EnableRewriting");  // As declared.
  PassLookupOptionByName("eNaBlErEWrItIng");  // mutated.
}

// All the non-base option names are explicitly enumerated here. Modifications
// are handled by the explicit tests. Additions/deletions are NOT handled.
TEST_F(RewriteOptionsTest, LookupNonBaseOptionByNameTest) {
  // Use macro so that the failure message tells us the name of the option
  // failing the test; using a function would obscure that.
#define FailLookupOptionByName(name) \
  EXPECT_TRUE(NULL == RewriteOptions::LookupOptionByName(name))

  // The following are not accessible by name, they are handled explicitly
  // by name comparison. We could/should test them all using their setters,
  // though -some- of them are (cf. ParseAndSetOptionFromName1/2/3 following).

  // Non-scalar options
  FailLookupOptionByName(RewriteOptions::kAllow);
  FailLookupOptionByName(RewriteOptions::kBlockingRewriteRefererUrls);
  FailLookupOptionByName(RewriteOptions::kDisableFilters);
  FailLookupOptionByName(RewriteOptions::kDisallow);
  FailLookupOptionByName(RewriteOptions::kDistributableFilters);
  FailLookupOptionByName(RewriteOptions::kDomain);
  FailLookupOptionByName(RewriteOptions::kDownstreamCachePurgeLocationPrefix);
  FailLookupOptionByName(RewriteOptions::kEnableFilters);
  FailLookupOptionByName(RewriteOptions::kExperimentVariable);
  FailLookupOptionByName(RewriteOptions::kExperimentSpec);
  FailLookupOptionByName(RewriteOptions::kForbidFilters);
  FailLookupOptionByName(RewriteOptions::kRetainComment);

  // 2-arg options
  FailLookupOptionByName(RewriteOptions::kCustomFetchHeader);
  FailLookupOptionByName(RewriteOptions::kLoadFromFile);
  FailLookupOptionByName(RewriteOptions::kLoadFromFileMatch);
  FailLookupOptionByName(RewriteOptions::kLoadFromFileRule);
  FailLookupOptionByName(RewriteOptions::kLoadFromFileRuleMatch);
  FailLookupOptionByName(RewriteOptions::kMapOriginDomain);
  FailLookupOptionByName(RewriteOptions::kMapProxyDomain);
  FailLookupOptionByName(RewriteOptions::kMapRewriteDomain);
  FailLookupOptionByName(RewriteOptions::kShardDomain);

  // 3-arg options
  FailLookupOptionByName(RewriteOptions::kUrlValuedAttribute);
  FailLookupOptionByName(RewriteOptions::kLibrary);

  // system/ and apache/ options.
  FailLookupOptionByName(RewriteOptions::kCacheFlushFilename);
  FailLookupOptionByName(RewriteOptions::kCacheFlushPollIntervalSec);
  FailLookupOptionByName(RewriteOptions::kCompressMetadataCache);
  FailLookupOptionByName(RewriteOptions::kFetchFromModSpdy);
  FailLookupOptionByName(RewriteOptions::kFetchHttps);
  FailLookupOptionByName(RewriteOptions::kFetcherProxy);
  FailLookupOptionByName(RewriteOptions::kFileCacheCleanIntervalMs);
  FailLookupOptionByName(RewriteOptions::kFileCachePath);
  FailLookupOptionByName(RewriteOptions::kFileCacheCleanSizeKb);
  FailLookupOptionByName(RewriteOptions::kFileCacheCleanInodeLimit);
  FailLookupOptionByName(RewriteOptions::kLogDir);
  FailLookupOptionByName(RewriteOptions::kLruCacheByteLimit);
  FailLookupOptionByName(RewriteOptions::kLruCacheKbPerProcess);
  FailLookupOptionByName(RewriteOptions::kMemcachedServers);
  FailLookupOptionByName(RewriteOptions::kMemcachedThreads);
  FailLookupOptionByName(RewriteOptions::kMemcachedTimeoutUs);
  FailLookupOptionByName(RewriteOptions::kRateLimitBackgroundFetches);
  FailLookupOptionByName(RewriteOptions::kUseSharedMemLocking);
  FailLookupOptionByName(RewriteOptions::kSlurpDirectory);
  FailLookupOptionByName(RewriteOptions::kSlurpFlushLimit);
  FailLookupOptionByName(RewriteOptions::kSlurpReadOnly);
  FailLookupOptionByName(RewriteOptions::kStatisticsEnabled);
  FailLookupOptionByName(RewriteOptions::kStatisticsHandlerPath);
  FailLookupOptionByName(RewriteOptions::kStatisticsLoggingEnabled);
  FailLookupOptionByName(RewriteOptions::kStatisticsLoggingChartsCSS);
  FailLookupOptionByName(RewriteOptions::kStatisticsLoggingChartsJS);
  FailLookupOptionByName(RewriteOptions::kStatisticsLoggingIntervalMs);
  FailLookupOptionByName(RewriteOptions::kStatisticsLoggingMaxFileSizeKb);
  FailLookupOptionByName(RewriteOptions::kTestProxy);
  FailLookupOptionByName(RewriteOptions::kTestProxySlurp);
}

TEST_F(RewriteOptionsTest, ParseAndSetOptionFromName1) {
  GoogleString msg;
  NullMessageHandler handler;

  // Unknown option.
  EXPECT_EQ(RewriteOptions::kOptionNameUnknown,
            options_.ParseAndSetOptionFromName1("arghh", "", &msg, &handler));

  // Simple scalar option.
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1("JsInlineMaxBytes", "42",
                                                &msg, &handler));
  EXPECT_EQ(42, options_.js_inline_max_bytes());

  // Scalar with invalid value.
  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName1("JsInlineMaxBytes", "one",
                                                &msg, &handler));
  EXPECT_EQ("Cannot set option JsInlineMaxBytes to one.", msg);

  // Complex, valid value.
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kDebug));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kOutlineCss));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                "EnableFilters", "debug,outline_css", &msg, &handler));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDebug));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kOutlineCss));

  // Complex, invalid value.
  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName1(
                "EnableFilters", "no_such_filter", &msg, &handler));
  EXPECT_EQ("Failed to enable some filters.", msg);

  // Disallow/Allow.
  options_.Disallow("*");
  EXPECT_FALSE(options_.IsAllowed("example.com"));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kAllow, "*.com", &msg, &handler));
  EXPECT_TRUE(options_.IsAllowed("example.com"));
  EXPECT_TRUE(options_.IsAllowed("evil.com"));
  EXPECT_FALSE(options_.IsAllowed("example.org"));

  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kDisallow, "*evil*", &msg, &handler));
  EXPECT_TRUE(options_.IsAllowed("example.com"));
  EXPECT_FALSE(options_.IsAllowed("evil.com"));

  // Disable/forbid filters (enable covered above).
  options_.EnableFilter(RewriteOptions::kDebug);
  options_.EnableFilter(RewriteOptions::kOutlineCss);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDebug));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kOutlineCss));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kDisableFilters, "debug,outline_css",
                &msg, &handler));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kDebug));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kOutlineCss));
  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kDisableFilters, "nosuch",
                &msg, &handler));
  EXPECT_EQ("Failed to disable some filters.", msg);

  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kForbidFilters, "debug",
                &msg, &handler));
  EXPECT_FALSE(
      options_.Forbidden(options_.FilterId(RewriteOptions::kOutlineCss)));
  EXPECT_TRUE(
      options_.Forbidden(options_.FilterId(RewriteOptions::kDebug)));

  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kForbidFilters, "nosuch",
                &msg, &handler));
  EXPECT_EQ("Failed to forbid some filters.", msg);

  // Domain.
  GoogleUrl main("http://example.com");
  GoogleUrl content("http://static.example.com");
  EXPECT_FALSE(options_.domain_lawyer()->IsDomainAuthorized(main, content));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kDomain, "static.example.com",
                &msg, &handler));
  EXPECT_TRUE(options_.domain_lawyer()->IsDomainAuthorized(main, content)) <<
      options_.domain_lawyer()->ToString();

  // Downstream cache purge location prefix.
  // 1) Valid location.
  GoogleUrl valid_downstream_cache("http://caching-layer.example.com:8118");
  EXPECT_FALSE(options_.domain_lawyer()->IsOriginKnown(valid_downstream_cache));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kDownstreamCachePurgeLocationPrefix,
                "http://caching-layer.example.com:8118/mypurgepath",
                &msg, &handler));
  EXPECT_TRUE(options_.domain_lawyer()->IsOriginKnown(valid_downstream_cache));
  EXPECT_EQ("http://caching-layer.example.com:8118/mypurgepath",
            options_.downstream_cache_purge_location_prefix());
  // 2) Invalid location.
  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kDownstreamCachePurgeLocationPrefix,
                "",
                &msg, &handler));
  EXPECT_EQ("Downstream cache purge location prefix is invalid.", msg);

  // Experiments.
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kExperimentSpec,
                "id=2;enable=recompress_png;percent=50",
                &msg, &handler));
  RewriteOptions::ExperimentSpec* spec = options_.GetExperimentSpec(2);
  ASSERT_TRUE(spec != NULL);
  EXPECT_EQ(2, spec->id());
  EXPECT_EQ(50, spec->percent());
  EXPECT_EQ(1,  spec->enabled_filters().size());
  EXPECT_TRUE(
      spec->enabled_filters().IsSet(RewriteOptions::kRecompressPng));

  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kExperimentSpec, "@)#@(#@(#@)((#)@",
                &msg, &handler));
  EXPECT_EQ("not a valid experiment spec", msg);

  EXPECT_NE(4, options_.experiment_ga_slot());
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kExperimentVariable, "4", &msg, &handler));
  EXPECT_EQ(4, options_.experiment_ga_slot());

  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kExperimentVariable, "10", &msg, &handler));
  EXPECT_EQ("must be an integer between 1 and 5", msg);

  // Retain comment.
  EXPECT_FALSE(options_.IsRetainedComment("important"));
  EXPECT_FALSE(options_.IsRetainedComment("silly"));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kRetainComment, "*port*", &msg, &handler));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kBlockingRewriteRefererUrls,
                "http://www.test.com/*", &msg, &handler));
  EXPECT_TRUE(options_.IsBlockingRewriteRefererUrlPatternPresent());
  EXPECT_TRUE(options_.IsBlockingRewriteEnabledForReferer(
      "http://www.test.com/"));
  EXPECT_FALSE(options_.IsBlockingRewriteEnabledForReferer(
      "http://www.testa.com/"));
  EXPECT_TRUE(options_.IsRetainedComment("important"));
  EXPECT_FALSE(options_.IsRetainedComment("silly"));
}

TEST_F(RewriteOptionsTest, ParseAndSetOptionFromName2) {
  GoogleString msg;
  NullMessageHandler handler;

  // Unknown option.
  EXPECT_EQ(RewriteOptions::kOptionNameUnknown,
            options_.ParseAndSetOptionFromName2("arghh", "", "",
                                                &msg, &handler));

  // Option mapped, but not a 2-argument.
  EXPECT_EQ(RewriteOptions::kOptionNameUnknown,
            options_.ParseAndSetOptionFromName2("JsInlineMaxBytes", "", "",
                                                &msg, &handler));

  // Valid value.
  EXPECT_EQ(0, options_.num_custom_fetch_headers());
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName2(
                "CustomFetchHeader", "header", "value", &msg, &handler));
  ASSERT_EQ(1, options_.num_custom_fetch_headers());
  EXPECT_EQ("header", options_.custom_fetch_header(0)->name);
  EXPECT_EQ("value", options_.custom_fetch_header(0)->value);

  // Invalid value.
  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName2(
                "LoadFromFileRule", "weird", "42", &msg, &handler));
  EXPECT_EQ("Argument 1 must be either 'Allow' or 'Disallow'", msg);

  // Various LoadFromFile options.
  GoogleString file_out;
  GoogleUrl url1("http://www.example.com/a.css");
  EXPECT_FALSE(
      options_.file_load_policy()->ShouldLoadFromFile(url1, &file_out));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName2(
                RewriteOptions::kLoadFromFile, "http://www.example.com",
                "/example/", &msg, &handler));
  EXPECT_TRUE(
      options_.file_load_policy()->ShouldLoadFromFile(url1, &file_out));
  EXPECT_EQ("/example/a.css", file_out);

  GoogleUrl url2("http://www.example.com/styles/b.css");
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName2(
                RewriteOptions::kLoadFromFileMatch,
                "^http://www.example.com/styles/([^/]*)", "/style/\\1",
                &msg, &handler));
  EXPECT_TRUE(
      options_.file_load_policy()->ShouldLoadFromFile(url2, &file_out));
  EXPECT_EQ("/style/b.css", file_out);

  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName2(
                RewriteOptions::kLoadFromFileMatch,
                "[a-", "/style/\\1",
                &msg, &handler));
  EXPECT_EQ("File mapping regular expression must match beginning of string. "
            "(Must start with '^'.)", msg);

  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName2(
                RewriteOptions::kLoadFromFileRuleMatch,
                "Allow", "[a-",
                &msg, &handler));
  // Not testing the message since it's RE2-originated.

  GoogleUrl url3("http://www.example.com/images/a.png");
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName2(
                RewriteOptions::kLoadFromFileRule,
                "Disallow", "/example/images/",
                &msg, &handler));
  EXPECT_FALSE(
      options_.file_load_policy()->ShouldLoadFromFile(url3, &file_out));

  GoogleUrl url4("http://www.example.com/images/a.jpeg");
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName2(
                RewriteOptions::kLoadFromFileRuleMatch,
                "Allow", "\\.jpeg", &msg, &handler));
  EXPECT_FALSE(
      options_.file_load_policy()->ShouldLoadFromFile(url3, &file_out));
  EXPECT_TRUE(
      options_.file_load_policy()->ShouldLoadFromFile(url4, &file_out));
  EXPECT_EQ("/example/images/a.jpeg", file_out);

  // Domain lawyer options.
  scoped_ptr<RewriteOptions> options2(new RewriteOptions(&thread_system_));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options2->ParseAndSetOptionFromName2(
                RewriteOptions::kMapOriginDomain,
                "localhost/example", "www.example.com",
                &msg, &handler));
  EXPECT_EQ("http://localhost/example/\n"
            "http://www.example.com/ Auth "
                "OriginDomain:http://localhost/example/\n",
            options2->domain_lawyer()->ToString());

  scoped_ptr<RewriteOptions> options3(new RewriteOptions(&thread_system_));
  // This is an option 2 or 3, so test 2 here and 3 below.
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options3->ParseAndSetOptionFromName3(
                RewriteOptions::kMapProxyDomain,
                "mainsite.com/static", "static.mainsite.com", "",
                &msg, &handler));
  EXPECT_EQ("http://mainsite.com/static/ Auth "
                "ProxyOriginDomain:http://static.mainsite.com/\n"
            "http://static.mainsite.com/ Auth "
                "ProxyDomain:http://mainsite.com/static/\n",
            options3->domain_lawyer()->ToString());

  scoped_ptr<RewriteOptions> options4(new RewriteOptions(&thread_system_));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options4->ParseAndSetOptionFromName2(
                RewriteOptions::kMapRewriteDomain,
                "cdn.example.com", "*example.com",
                &msg, &handler));
  EXPECT_EQ("http://*example.com/ Auth RewriteDomain:http://cdn.example.com/\n"
            "http://cdn.example.com/ Auth\n",
            options4->domain_lawyer()->ToString());

  scoped_ptr<RewriteOptions> options5(new RewriteOptions(&thread_system_));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options5->ParseAndSetOptionFromName2(
                RewriteOptions::kShardDomain,
                "https://www.example.com",
                "https://example1.cdn.com,https://example2.cdn.com",
                &msg, &handler));
  EXPECT_EQ("https://example1.cdn.com/ Auth "
                "RewriteDomain:https://www.example.com/\n"
            "https://example2.cdn.com/ Auth "
                "RewriteDomain:https://www.example.com/\n"
            "https://www.example.com/ Auth Shards:"
                "{https://example1.cdn.com/, "
                "https://example2.cdn.com/}\n",
            options5->domain_lawyer()->ToString());
}

TEST_F(RewriteOptionsTest, ParseAndSetOptionFromName3) {
  GoogleString msg;
  NullMessageHandler handler;

  // Unknown option.
  EXPECT_EQ(RewriteOptions::kOptionNameUnknown,
            options_.ParseAndSetOptionFromName3("arghh", "", "", "",
                                                &msg, &handler));

  // Option mapped, but not a 2-argument.
  EXPECT_EQ(RewriteOptions::kOptionNameUnknown,
            options_.ParseAndSetOptionFromName3("JsInlineMaxBytes", "", "", "",
                                                &msg, &handler));

  // Valid value.
  EXPECT_EQ(0, options_.num_url_valued_attributes());
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName3(
                "UrlValuedAttribute", "span", "src", "Hyperlink",
                &msg, &handler));
  ASSERT_EQ(1, options_.num_url_valued_attributes());
  StringPiece element, attribute;
  semantic_type::Category category;
  options_.UrlValuedAttribute(0, &element, &attribute, &category);
  EXPECT_EQ("span", element);
  EXPECT_EQ("src", attribute);
  EXPECT_EQ(semantic_type::kHyperlink, category);

  // Invalid value.
  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName3(
                "UrlValuedAttribute", "span", "src", "nonsense",
                &msg, &handler));
  EXPECT_EQ("Invalid resource category: nonsense", msg);

  // Domain lawyer.
  scoped_ptr<RewriteOptions> options(new RewriteOptions(&thread_system_));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options->ParseAndSetOptionFromName3(
                RewriteOptions::kMapProxyDomain,
                "myproxy.com/static",
                "static.origin.com",
                "myproxy.cdn.com",
                &msg, &handler));
  EXPECT_EQ("http://myproxy.cdn.com/ Auth "
                "ProxyOriginDomain:http://static.origin.com/\n"
            "http://myproxy.com/static/ Auth "
                "RewriteDomain:http://myproxy.cdn.com/ "
                "ProxyOriginDomain:http://static.origin.com/\n"
            "http://static.origin.com/ Auth "
                "ProxyDomain:http://myproxy.cdn.com/\n",
            options->domain_lawyer()->ToString());

  options_.EnableFilter(RewriteOptions::kCanonicalizeJavascriptLibraries);
  GoogleString sig;
  options_.javascript_library_identification()->AppendSignature(&sig);
  EXPECT_EQ("", sig);
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName3(
                RewriteOptions::kLibrary, "43567", "5giEj_jl-Ag5G8",
                "http://www.example.com/url.js",
                &msg, &handler));
  sig.clear();
  options_.javascript_library_identification()->AppendSignature(&sig);
  EXPECT_EQ("S:43567_H:5giEj_jl-Ag5G8_J:http://www.example.com/url.js", sig);

  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName3(
                RewriteOptions::kLibrary, "43567", "#@#)@(#@)",
                "http://www.example.com/url.js",
                &msg, &handler));
  EXPECT_EQ("Format is size md5 url; bad md5 #@#)@(#@) or "
            "URL http://www.example.com/url.js", msg);
}

TEST_F(RewriteOptionsTest, ExperimentSpecTest) {
  // Test that we handle experiment specs properly, and that when we set the
  // options to one experiment or another, it works.
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.set_ga_id("UA-111111-1");
  // Set the default slot to 4.
  options_.set_experiment_ga_slot(4);
  EXPECT_FALSE(options_.AddExperimentSpec("id=0", &handler));
  EXPECT_TRUE(options_.AddExperimentSpec(
      "id=7;percent=10;level=CoreFilters;enabled=sprite_images;"
      "disabled=inline_css;inline_js=600000", &handler));

  // Extra spaces to test whitespace handling.
  EXPECT_TRUE(options_.AddExperimentSpec("id=2;    percent=15;ga=UA-2222-1;"
                                         "disabled=insert_ga ;slot=3;",
                                         &handler));

  // Invalid slot - make sure the spec still gets added, and the slot defaults
  // to the global slot (4).
  EXPECT_TRUE(options_.AddExperimentSpec("id=17;percent=3;slot=8", &handler));

  options_.SetExperimentState(7);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kSpriteImages));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineCss));
  // This experiment didn't have a ga_id, so make sure we still have the
  // global ga_id.
  EXPECT_EQ("UA-111111-1", options_.ga_id());
  EXPECT_EQ(4, options_.experiment_ga_slot());

  // insert_ga can not be disabled in any experiment because that filter injects
  // the instrumentation we use to collect the data.
  options_.SetExperimentState(2);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineCss));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kSpriteImages));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kLeftTrimUrls));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInsertGA));
  EXPECT_EQ(3, options_.experiment_ga_slot());
  // This experiment specified a ga_id, so make sure that we set it.
  EXPECT_EQ("UA-2222-1", options_.ga_id());

  options_.SetExperimentState(17);
  EXPECT_EQ(4, options_.experiment_ga_slot());

  options_.SetExperimentState(7);
  EXPECT_EQ("a", options_.GetExperimentStateStr());
  options_.SetExperimentState(2);
  EXPECT_EQ("b", options_.GetExperimentStateStr());
  options_.SetExperimentState(17);
  EXPECT_EQ("c", options_.GetExperimentStateStr());
  options_.SetExperimentState(experiment::kExperimentNotSet);
  EXPECT_EQ("", options_.GetExperimentStateStr());
  options_.SetExperimentState(experiment::kNoExperiment);
  EXPECT_EQ("", options_.GetExperimentStateStr());

  options_.SetExperimentStateStr("a");
  EXPECT_EQ("a", options_.GetExperimentStateStr());
  options_.SetExperimentStateStr("b");
  EXPECT_EQ("b", options_.GetExperimentStateStr());
  options_.SetExperimentStateStr("c");
  EXPECT_EQ("c", options_.GetExperimentStateStr());

  // Invalid state index 'd'; we only added three specs above.
  options_.SetExperimentStateStr("d");
  // No effect on the experiment state; stay with 'c' from before.
  EXPECT_EQ("c", options_.GetExperimentStateStr());

  // Check a state index that will be out of bounds in the other direction.
  options_.SetExperimentStateStr("`");
  // Still no effect on the experiment state.
  EXPECT_EQ("c", options_.GetExperimentStateStr());

  // Check that we have a maximum size of 26 concurrent experiment specs.
  // Get us up to 26.
  for (int i = options_.num_experiments(); i < 26 ; ++i) {
    int tmp_id = i+100;  // Don't want conflict with experiments added above.
    EXPECT_TRUE(options_.AddExperimentSpec(
        StrCat("id=", IntegerToString(tmp_id),
               ";percent=1;default"), &handler));
  }
  EXPECT_EQ(26, options_.num_experiments());
  // Object to adding a 27th.
  EXPECT_FALSE(options_.AddExperimentSpec("id=200;percent=1;default",
                                          &handler));
}

TEST_F(RewriteOptionsTest, PreserveURLDefaults) {
  // This test serves as a warning. If you enable preserve URLs by default then
  // many unit tests will fail due to filters being omitted from the HTML path.
  // Further, preserve_urls is not explicitly tested for the 'false' case, it is
  // assumed to be tested by the normal unit tests since the default value is
  // false.
  EXPECT_FALSE(options_.image_preserve_urls());
  EXPECT_FALSE(options_.css_preserve_urls());
  EXPECT_FALSE(options_.js_preserve_urls());
}

TEST_F(RewriteOptionsTest, RewriteDeadlineTest) {
  EXPECT_EQ(RewriteOptions::kDefaultRewriteDeadlineMs,
            options_.rewrite_deadline_ms());
  options_.set_rewrite_deadline_ms(40);
  EXPECT_EQ(40, options_.rewrite_deadline_ms());
}

TEST_F(RewriteOptionsTest, ExperimentPrintTest) {
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.set_ga_id("UA-111111-1");
  options_.set_running_experiment(true);
  EXPECT_FALSE(options_.AddExperimentSpec("id=2;enabled=rewrite_css;",
                                          &handler));
  EXPECT_TRUE(options_.AddExperimentSpec("id=1;percent=15;default", &handler));
  EXPECT_TRUE(options_.AddExperimentSpec("id=7;percent=15;level=AllFilters;",
                                         &handler));
  EXPECT_TRUE(options_.AddExperimentSpec("id=2;percent=15;enabled=rewrite_css;"
                                         "inline_css=4096;ga_id=122333-4",
                                         &handler));
  options_.SetExperimentState(-7);
  // This should be the core filters.
  EXPECT_EQ("ah,cc,jc,gp,jp,mc,pj,ec,ei,es,fc,if,hw,ci,ii,il,ji,js,rj,rp,rw,"
            "ri,cf,jm,cu,cp,md,css:2048,im:3072,js:2048;",
            options_.ToExperimentDebugString());
  EXPECT_EQ("", options_.ToExperimentString());
  options_.SetExperimentState(1);
  EXPECT_EQ("Experiment: 1; ah,ai,ca,cc,jc,gp,jp,mc,pj,ec,ei,es,fc,if,hw,ci,ii,"
            "il,ji,ig,js,rj,rp,rw,ri,cf,jm,cu,cp,md,"
            "css:2048,im:3072,js:2048;",
            options_.ToExperimentDebugString());
  EXPECT_EQ("Experiment: 1", options_.ToExperimentString());
  options_.SetExperimentState(7);
  EXPECT_EQ("Experiment: 7", options_.ToExperimentString());
  options_.SetExperimentState(2);
  // This should be the filters we need to run an experiment (add_head,
  // add_instrumentation, html_writer, insert_ga) plus rewrite_css.
  // The image inline threshold is 0 because ImageInlineMaxBytes()
  // only returns the threshold if inline_images is enabled.
  EXPECT_EQ("Experiment: 2; ah,ai,ca,hw,ig,cf,css:4096,im:0,js:2048;",
            options_.ToExperimentDebugString());
  EXPECT_EQ("Experiment: 2", options_.ToExperimentString());

  // Make sure we set the ga_id to the one specified by spec 2.
  EXPECT_EQ("122333-4", options_.ga_id());
}

TEST_F(RewriteOptionsTest, ExperimentUndoOptionsTest) {
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.set_running_experiment(true);

  // Default for this is kDefaultImageInlineMaxBytes.
  EXPECT_EQ(RewriteOptions::kDefaultImageInlineMaxBytes,
            options_.ImageInlineMaxBytes());
  EXPECT_TRUE(options_.AddExperimentSpec(
      "id=1;percent=15;enable=inline_images;"
      "inline_images=1024", &handler));
  options_.SetExperimentState(1);
  EXPECT_EQ(1024L, options_.ImageInlineMaxBytes());
  EXPECT_TRUE(options_.AddExperimentSpec(
      "id=2;percent=15;enable=inline_images", &handler));
  options_.SetExperimentState(2);
  EXPECT_EQ(RewriteOptions::kDefaultImageInlineMaxBytes,
            options_.ImageInlineMaxBytes());
}

TEST_F(RewriteOptionsTest, ExperimentOptionsTest) {
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.set_running_experiment(true);

  // Default for this is 2048.
  EXPECT_EQ(2048L, options_.css_inline_max_bytes());
  EXPECT_TRUE(options_.AddExperimentSpec(
      "id=1;percent=15;enable=defer_javascript;"
      "options=CssInlineMaxBytes=1024", &handler));
  options_.SetExperimentState(1);
  EXPECT_EQ(1024L, options_.css_inline_max_bytes());
  EXPECT_TRUE(options_.AddExperimentSpec(
      "id=2;percent=15;enable=resize_images;options=BogusOption=35", &handler));
  EXPECT_TRUE(options_.AddExperimentSpec(
      "id=3;percent=15;enable=defer_javascript", &handler));
  options_.SetExperimentState(3);
  EXPECT_EQ(2048L, options_.css_inline_max_bytes());
  EXPECT_TRUE(options_.AddExperimentSpec(
      "id=4;percent=15;enable=defer_javascript;"
      "options=CssInlineMaxBytes=Cabbage", &handler));
  options_.SetExperimentState(4);
  EXPECT_EQ(2048L, options_.css_inline_max_bytes());
  EXPECT_TRUE(options_.AddExperimentSpec(
      "id=5;percent=15;enable=defer_javascript;"
      "options=Potato=Carrot,5=10,6==9,CssInlineMaxBytes=1024", &handler));
  options_.SetExperimentState(5);
  EXPECT_EQ(1024L, options_.css_inline_max_bytes());
  EXPECT_TRUE(options_.AddExperimentSpec(
      "id=6;percent=15;enable=defer_javascript;"
      "options=JsOutlineMinBytes=4096,JpegRecompresssionQuality=50,"
      "CssInlineMaxBytes=100,JsInlineMaxBytes=123", &handler));
  options_.SetExperimentState(6);
  EXPECT_EQ(100L, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, ExperimentMergeTest) {
  NullMessageHandler handler;
  RewriteOptions::ExperimentSpec *spec = new
      RewriteOptions::ExperimentSpec("id=1;percentage=15;"
                                     "enable=defer_javascript;"
                                     "options=CssInlineMaxBytes=100",
                                     &options_, &handler);

  RewriteOptions::ExperimentSpec *spec2 = new
      RewriteOptions::ExperimentSpec("id=2;percentage=25;enable=resize_images;"
                                     "options=CssInlineMaxBytes=125", &options_,
                                     &handler);
  options_.InsertExperimentSpecInVector(spec);
  options_.InsertExperimentSpecInVector(spec2);
  options_.SetExperimentState(1);
  EXPECT_EQ(15, spec->percent());
  EXPECT_EQ(1, spec->id());
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDeferJavascript));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kResizeImages));
  EXPECT_EQ(100L, options_.css_inline_max_bytes());
  spec->Merge(*spec2);
  options_.SetExperimentState(1);
  EXPECT_EQ(25, spec->percent());
  EXPECT_EQ(1, spec->id());
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDeferJavascript));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kResizeImages));
  EXPECT_EQ(125L, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, SetOptionsFromName) {
  RewriteOptions::OptionSet option_set;
  option_set.insert(RewriteOptions::OptionStringPair(
      "CssInlineMaxBytes", "1024"));
  EXPECT_TRUE(options_.SetOptionsFromName(option_set));
  option_set.insert(RewriteOptions::OptionStringPair(
      "Not an Option", "nothing"));
  EXPECT_FALSE(options_.SetOptionsFromName(option_set));
}

// TODO(sriharis):  Add thorough ComputeSignature tests

TEST_F(RewriteOptionsTest, ComputeSignatureWildcardGroup) {
  options_.ComputeSignature();
  GoogleString signature1 = options_.signature();
  // Tweak allow_resources_ and check that signature changes.
  options_.ClearSignatureForTesting();
  options_.Disallow("http://www.example.com/*");
  options_.ComputeSignature();
  GoogleString signature2 = options_.signature();
  EXPECT_NE(signature1, signature2);
  // Tweak retain_comments and check that signature changes.
  options_.ClearSignatureForTesting();
  options_.RetainComment("TEST");
  options_.ComputeSignature();
  GoogleString signature3 = options_.signature();
  EXPECT_NE(signature1, signature3);
  EXPECT_NE(signature2, signature3);
}

TEST_F(RewriteOptionsTest, ComputeSignatureOptionEffect) {
  options_.ClearSignatureForTesting();
  options_.set_css_image_inline_max_bytes(2048);
  options_.set_in_place_rewriting_enabled(false);
  options_.ComputeSignature();
  GoogleString signature1 = options_.signature();

  // Changing an Option used in signature computation will change the signature.
  options_.ClearSignatureForTesting();
  options_.set_css_image_inline_max_bytes(1024);
  options_.ComputeSignature();
  GoogleString signature2 = options_.signature();
  EXPECT_NE(signature1, signature2);

  // Changing an Option not used in signature computation will not change the
  // signature.
  options_.ClearSignatureForTesting();
  options_.set_in_place_rewriting_enabled(true);
  options_.ComputeSignature();
  GoogleString signature3 = options_.signature();

  // See the comment in RewriteOptions::RewriteOptions -- we need to leave
  // signatures sensitive to ajax_rewriting.
  EXPECT_NE(signature2, signature3);
}

TEST_F(RewriteOptionsTest, IsEqual) {
  RewriteOptions a(&thread_system_), b(&thread_system_);
  a.ComputeSignature();
  b.ComputeSignature();
  EXPECT_TRUE(a.IsEqual(b));
  a.ClearSignatureForTesting();
  a.EnableFilter(RewriteOptions::kSpriteImages);
  a.ComputeSignature();
  EXPECT_FALSE(a.IsEqual(b));
  b.ClearSignatureForTesting();
  b.EnableFilter(RewriteOptions::kSpriteImages);
  b.ComputeSignature();
  EXPECT_TRUE(a.IsEqual(b));
}

TEST_F(RewriteOptionsTest, ComputeSignatureEmptyIdempotent) {
  options_.ClearSignatureForTesting();
  options_.DisallowTroublesomeResources();
  options_.ComputeSignature();
  GoogleString signature1 = options_.signature();
  options_.ClearSignatureForTesting();

  // Merging in empty RewriteOptions should not change the signature.
  RewriteOptions options2(&thread_system_);
  options_.Merge(options2);
  options_.ComputeSignature();
  EXPECT_EQ(signature1, options_.signature());
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

  options_.EnableFilter(RewriteOptions::kConvertToWebpLossless);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kConvertToWebpLossless);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());
}

TEST_F(RewriteOptionsTest, UrlCacheInvalidationTest) {
  options_.AddUrlCacheInvalidationEntry("one*", 10L, true);
  options_.AddUrlCacheInvalidationEntry("two*", 25L, false);
  options_.AddUrlCacheInvalidationEntry("four", 40L, false);
  options_.AddUrlCacheInvalidationEntry("five", 50L, false);
  options_.AddUrlCacheInvalidationEntry("six", 60L, false);
  RewriteOptions options1(&thread_system_);
  options1.AddUrlCacheInvalidationEntry("one*", 20L, true);
  options1.AddUrlCacheInvalidationEntry("three*", 23L, false);
  options1.AddUrlCacheInvalidationEntry("three*", 30L, true);
  options1.AddUrlCacheInvalidationEntry("four", 39L, false);
  options1.AddUrlCacheInvalidationEntry("five", 51L, false);
  options1.AddUrlCacheInvalidationEntry("seven", 70L, false);
  options_.Merge(options1);
  EXPECT_TRUE(options_.IsUrlCacheInvalidationEntriesSorted());
  EXPECT_FALSE(options_.IsUrlCacheValid("one1", 9L));
  EXPECT_FALSE(options_.IsUrlCacheValid("one1", 19L));
  EXPECT_TRUE(options_.IsUrlCacheValid("one1", 21L));
  EXPECT_FALSE(options_.IsUrlCacheValid("two2", 21L));
  EXPECT_TRUE(options_.IsUrlCacheValid("two2", 26L));
  EXPECT_TRUE(options_.IsUrlCacheValid("three3", 31L));
  EXPECT_FALSE(options_.IsUrlCacheValid("four", 40L));
  EXPECT_TRUE(options_.IsUrlCacheValid("four", 41L));
  EXPECT_FALSE(options_.IsUrlCacheValid("five", 51L));
  EXPECT_TRUE(options_.IsUrlCacheValid("five", 52L));
  EXPECT_FALSE(options_.IsUrlCacheValid("six", 60L));
  EXPECT_TRUE(options_.IsUrlCacheValid("six", 61L));
  EXPECT_FALSE(options_.IsUrlCacheValid("seven", 70L));
  EXPECT_TRUE(options_.IsUrlCacheValid("seven", 71L));
}

TEST_F(RewriteOptionsTest, UrlCacheInvalidationSignatureTest) {
  options_.ComputeSignature();
  GoogleString signature1 = options_.signature();
  options_.ClearSignatureForTesting();
  options_.AddUrlCacheInvalidationEntry("one*", 10L, true);
  options_.ComputeSignature();
  GoogleString signature2 = options_.signature();
  EXPECT_EQ(signature1, signature2);
  options_.ClearSignatureForTesting();
  options_.AddUrlCacheInvalidationEntry("two*", 10L, false);
  options_.ComputeSignature();
  GoogleString signature3 = options_.signature();
  EXPECT_NE(signature2, signature3);
}

TEST_F(RewriteOptionsTest, EnabledFiltersRequiringJavaScriptTest) {
  RewriteOptions foo(&thread_system_);
  foo.ClearFilters();
  foo.EnableFilter(RewriteOptions::kDeferJavascript);
  foo.EnableFilter(RewriteOptions::kResizeImages);
  RewriteOptions::FilterVector foo_fs;
  foo.GetEnabledFiltersRequiringScriptExecution(&foo_fs);
  EXPECT_FALSE(foo_fs.empty());
  EXPECT_EQ(1, foo_fs.size());

  RewriteOptions bar(&thread_system_);
  bar.ClearFilters();
  bar.EnableFilter(RewriteOptions::kResizeImages);
  bar.EnableFilter(RewriteOptions::kConvertPngToJpeg);
  RewriteOptions::FilterVector bar_fs;
  bar.GetEnabledFiltersRequiringScriptExecution(&bar_fs);
  EXPECT_TRUE(bar_fs.empty());
}

TEST_F(RewriteOptionsTest, FilterLookupMethods) {
  EXPECT_STREQ("Add Head",
               RewriteOptions::FilterName(RewriteOptions::kAddHead));
  EXPECT_STREQ("Remove Comments",
               RewriteOptions::FilterName(RewriteOptions::kRemoveComments));
  // Can't do these unless we remove the LOG(DFATAL) from FilterName().
  // EXPECT_STREQ("End of Filters",
  //              RewriteOptions::FilterName(RewriteOptions::kEndOfFilters));
  // EXPECT_STREQ("Unknown Filter",
  //              RewriteOptions::FilterName(
  //                  static_cast<RewriteOptions::Filter>(-1)));

  EXPECT_STREQ("ah",
               RewriteOptions::FilterId(RewriteOptions::kAddHead));
  EXPECT_STREQ("rc",
               RewriteOptions::FilterId(RewriteOptions::kRemoveComments));
  // Can't do these unless we remove the LOG(DFATAL) from FilterName().
  // EXPECT_STREQ("UF",
  //              RewriteOptions::FilterId(RewriteOptions::kEndOfFilters));
  // EXPECT_STREQ("UF",
  //              RewriteOptions::FilterId(
  //                  static_cast<RewriteOptions::Filter>(-1)));

  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById("  "));
  EXPECT_EQ(RewriteOptions::kAddHead,
            RewriteOptions::LookupFilterById("ah"));
  EXPECT_EQ(RewriteOptions::kRemoveComments,
            RewriteOptions::LookupFilterById("rc"));
  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById("zz"));
  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById("UF"));
  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById("junk"));
  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById(""));
  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById(NULL));

  EXPECT_EQ(RewriteOptions::kAnalyticsID,
            RewriteOptions::LookupOptionNameById("ig"));
  EXPECT_EQ(RewriteOptions::kImageJpegRecompressionQuality,
            RewriteOptions::LookupOptionNameById("iq"));
  EXPECT_TRUE(RewriteOptions::LookupOptionNameById("  ").empty());
  EXPECT_TRUE(RewriteOptions::LookupOptionNameById("junk").empty());
  EXPECT_TRUE(RewriteOptions::LookupOptionNameById("").empty());
  EXPECT_TRUE(RewriteOptions::LookupOptionNameById(NULL).empty());
}

TEST_F(RewriteOptionsTest, ParseBeaconUrl) {
  RewriteOptions::BeaconUrl beacon_url;
  GoogleString url = "www.example.com";
  GoogleString url2 = "www.example.net";

  EXPECT_FALSE(RewriteOptions::ParseBeaconUrl("", &beacon_url));
  EXPECT_FALSE(RewriteOptions::ParseBeaconUrl("a b c", &beacon_url));

  EXPECT_TRUE(RewriteOptions::ParseBeaconUrl("http://" + url, &beacon_url));
  EXPECT_STREQ("http://" + url, beacon_url.http);
  EXPECT_STREQ("https://" + url, beacon_url.https);

  EXPECT_TRUE(RewriteOptions::ParseBeaconUrl("https://" + url, &beacon_url));
  EXPECT_STREQ("https://" + url, beacon_url.http);
  EXPECT_STREQ("https://" + url, beacon_url.https);

  EXPECT_TRUE(RewriteOptions::ParseBeaconUrl(
      "http://" + url + " " + "https://" + url2, &beacon_url));
  EXPECT_STREQ("http://" + url, beacon_url.http);
  EXPECT_STREQ("https://" + url2, beacon_url.https);

  // Verify that ets parameters get stripped from the beacon_url
  EXPECT_TRUE(RewriteOptions::ParseBeaconUrl("http://" + url + "?ets=" + " " +
                                             "https://"+ url2 + "?foo=bar&ets=",
                                             &beacon_url));
  EXPECT_STREQ("http://" + url, beacon_url.http);
  EXPECT_STREQ("https://" + url2 + "?foo=bar", beacon_url.https);
  EXPECT_STREQ("http://" + url, beacon_url.http_in);
  EXPECT_STREQ("https://" + url2, beacon_url.https_in);

  EXPECT_TRUE(RewriteOptions::ParseBeaconUrl("/mod_pagespeed_beacon?a=b",
                                             &beacon_url));
  EXPECT_STREQ("/mod_pagespeed_beacon?a=b", beacon_url.http);
  EXPECT_STREQ("/mod_pagespeed_beacon?a=b", beacon_url.https);
  EXPECT_STREQ("/mod_pagespeed_beacon", beacon_url.http_in);
  EXPECT_STREQ("/mod_pagespeed_beacon", beacon_url.https_in);
}

TEST_F(RewriteOptionsTest, AccessOptionByIdAndName) {
  const char* id = NULL;
  GoogleString value;
  bool was_set = false;
  EXPECT_TRUE(options_.OptionValue(
      RewriteOptions::kImageJpegRecompressionQuality, &id, &was_set, &value));
  EXPECT_FALSE(was_set);
  EXPECT_STREQ("iq", id);
  const StringPiece kBogusOptionName("bogosity!");
  EXPECT_EQ(RewriteOptions::kOptionNameUnknown,
            options_.SetOptionFromName(kBogusOptionName, ""));
  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.SetOptionFromName(
                RewriteOptions::kImageJpegRecompressionQuality, "garbage"));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.SetOptionFromName(
                RewriteOptions::kImageJpegRecompressionQuality, "63"));
  id = NULL;
  EXPECT_TRUE(options_.OptionValue(
      RewriteOptions::kImageJpegRecompressionQuality, &id, &was_set, &value));
  EXPECT_TRUE(was_set);
  EXPECT_STREQ("iq", id);
  EXPECT_STREQ("63", value);

  EXPECT_FALSE(options_.OptionValue(kBogusOptionName, &id, &was_set, &value));
}

TEST_F(RewriteOptionsTest, AccessAcrossThreads) {
#ifndef NDEBUG  // Depends on bits set in rewrite_options.cc under debug
  NullThreadSystem null_thread_system;

  null_thread_system.set_current_thread(5);

  RewriteOptions options(&null_thread_system);
  // We can continue to modify in the same thread.
  EXPECT_TRUE(options.ModificationOK());

  // Unmodified, we could switch to a different thread.
  null_thread_system.set_current_thread(6);
  EXPECT_TRUE(options.ModificationOK());
  null_thread_system.set_current_thread(5);

  // Now make a modification.  We can continue to modify in the same thread.
  options.set_enabled(RewriteOptions::kEnabledOff);
  EXPECT_TRUE(options.ModificationOK());

  // But from a different thread we must not modify.
  null_thread_system.set_current_thread(4);
  EXPECT_FALSE(options.ModificationOK());

  // Back in thread 5 we can modify.
  null_thread_system.set_current_thread(5);
  EXPECT_TRUE(options.ModificationOK());

  // We can merge from the same thread, but not from a different one.
  EXPECT_TRUE(options.MergeOK());
  null_thread_system.set_current_thread(4);
  EXPECT_FALSE(options.MergeOK());

  // Clearing the signature gets us on a clean slate and we can take over
  // from thread 4.
  options.ClearSignatureWithCaution();
  EXPECT_TRUE(options.MergeOK());

  // Once we freeze it we can merge from it.
  options.Freeze();
  EXPECT_TRUE(options.MergeOK());
  null_thread_system.set_current_thread(5);
  EXPECT_TRUE(options.MergeOK());
#endif
}

}  // namespace net_instaweb
