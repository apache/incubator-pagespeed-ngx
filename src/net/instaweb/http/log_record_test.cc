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

// Author: marq@google.com (Mark Cogan)

#include "net/instaweb/http/public/log_record.h"

#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/null_mutex.h"
#include "net/instaweb/util/public/scoped_ptr.h"

namespace net_instaweb {

class LogRecordTest : public testing::Test {
 protected:
  virtual void SetUp() {
    log_record_.reset(new LogRecord(new NullMutex));
  }

  const DeviceInfo& device_info() {
    return log_record_->logging_info()->device_info();
  }

  // Test that the rewriter_info array is |size| elements long, and that the
  // last element has the |last_id| id string and the |last_status| status.
  void TestRewriterInfo(int size, GoogleString last_id, int last_status) {
    LoggingInfo* l = log_record_->logging_info();
    EXPECT_EQ(size, l->rewriter_info_size());
    EXPECT_EQ(last_id, l->rewriter_info(size-1).id());
    EXPECT_EQ(last_status, l->rewriter_info(size-1).status());
  }

  scoped_ptr<AbstractLogRecord> log_record_;
};

typedef LogRecordTest LogRecordDeathTest;

TEST_F(LogRecordTest, NoAppliedRewriters) {
  LoggingInfo* logging_info = log_record_->logging_info();
  EXPECT_EQ("", log_record_->AppliedRewritersString());
  EXPECT_EQ(0, logging_info->rewriter_info_size());
}

TEST_F(LogRecordTest, SimpleAppliedRewriters) {
  log_record_->SetRewriterLoggingStatus("zz", RewriterApplication::APPLIED_OK);
  TestRewriterInfo(1, "zz", RewriterApplication::APPLIED_OK);
  log_record_->SetRewriterLoggingStatus("aa", RewriterApplication::APPLIED_OK);
  TestRewriterInfo(2, "aa", RewriterApplication::APPLIED_OK);
  EXPECT_EQ("aa,zz",  log_record_->AppliedRewritersString());
}

TEST_F(LogRecordTest, DuplicateAppliedRewriters) {
  log_record_->SetRewriterLoggingStatus("zz", RewriterApplication::APPLIED_OK);
  TestRewriterInfo(1, "zz", RewriterApplication::APPLIED_OK);
  log_record_->SetRewriterLoggingStatus("aa", RewriterApplication::APPLIED_OK);
  TestRewriterInfo(2, "aa", RewriterApplication::APPLIED_OK);
  log_record_->SetRewriterLoggingStatus("zz", RewriterApplication::APPLIED_OK);
  TestRewriterInfo(3, "zz", RewriterApplication::APPLIED_OK);
  EXPECT_EQ("aa,zz", log_record_->AppliedRewritersString());
}

TEST_F(LogRecordTest, ProgressiveAppliedRewriters) {
  log_record_->SetRewriterLoggingStatus("zz", RewriterApplication::APPLIED_OK);
  EXPECT_EQ("zz", log_record_->AppliedRewritersString());
  TestRewriterInfo(1, "zz", RewriterApplication::APPLIED_OK);
  log_record_->SetRewriterLoggingStatus("aa", RewriterApplication::APPLIED_OK);
  EXPECT_EQ("aa,zz", log_record_->AppliedRewritersString());
  TestRewriterInfo(2, "aa", RewriterApplication::APPLIED_OK);
  log_record_->SetRewriterLoggingStatus("zz", RewriterApplication::APPLIED_OK);
  EXPECT_EQ("aa,zz", log_record_->AppliedRewritersString());
  TestRewriterInfo(3, "zz", RewriterApplication::APPLIED_OK);
  log_record_->SetRewriterLoggingStatus("bb", RewriterApplication::APPLIED_OK);
  EXPECT_EQ("aa,bb,zz", log_record_->AppliedRewritersString());
  TestRewriterInfo(4, "bb", RewriterApplication::APPLIED_OK);
}

TEST_F(LogRecordTest, RewriterInfoSizeLimit) {
  LoggingInfo* logging_info = log_record_->logging_info();
  log_record_->SetRewriterInfoMaxSize(2);
  for (int i = 0; i < 100; ++i) {
    log_record_->SetRewriterLoggingStatus("zz",
                                          RewriterApplication::APPLIED_OK);
  }
  EXPECT_EQ("zz", log_record_->AppliedRewritersString());
  EXPECT_TRUE(logging_info->rewriter_info_size_limit_exceeded());
  TestRewriterInfo(2, "zz", RewriterApplication::APPLIED_OK);
}

TEST_F(LogRecordTest, ProgressiveAppliedRewritersWithStatus) {
  LoggingInfo* logging_info = log_record_->logging_info();
  RewriterInfo* info = log_record_->NewRewriterInfo("zz");
  // Not setting the status has the rewriter not show up.
  EXPECT_EQ("", log_record_->AppliedRewritersString());
  TestRewriterInfo(1, "zz", RewriterApplication::UNKNOWN_STATUS);
  // Set it here and it shows up
  info->set_status(RewriterApplication::APPLIED_OK);
  EXPECT_EQ("zz", log_record_->AppliedRewritersString());
  TestRewriterInfo(1, "zz", RewriterApplication::APPLIED_OK);
  // Leave this unset until later
  RewriterInfo* unset_status_info = log_record_->NewRewriterInfo("cc");
  EXPECT_EQ("zz", log_record_->AppliedRewritersString());
  TestRewriterInfo(2, "cc", RewriterApplication::UNKNOWN_STATUS);
  // Adding through the old method interoperates correctly
  log_record_->SetRewriterLoggingStatus("aa", RewriterApplication::APPLIED_OK);
  EXPECT_EQ("aa,zz", log_record_->AppliedRewritersString());
  TestRewriterInfo(3, "aa", RewriterApplication::APPLIED_OK);
  // Adding a duplicate through the new method doesn't produce a dupe in the
  // string
  info = log_record_->NewRewriterInfo("aa");
  info->set_status(RewriterApplication::APPLIED_OK);
  EXPECT_EQ("aa,zz", log_record_->AppliedRewritersString());
  TestRewriterInfo(4, "aa", RewriterApplication::APPLIED_OK);
  // Changing the entry with unset status to OK adds it
  unset_status_info->set_status(RewriterApplication::APPLIED_OK);
  EXPECT_EQ("aa,cc,zz", log_record_->AppliedRewritersString());
  TestRewriterInfo(4, "aa", RewriterApplication::APPLIED_OK);
  // Check that the 'cc' record added earlier is now APPLIED_OK.
  EXPECT_EQ(RewriterApplication::APPLIED_OK,
            logging_info->rewriter_info(1).status());
}

TEST_F(LogRecordTest, DoNotLogUrlsOrIndices) {
  LoggingInfo* logging_info = log_record_->logging_info();
  log_record_->SetRewriterLoggingStatus("z1", "url",
                                        RewriterApplication::APPLIED_OK);
  EXPECT_EQ(0, logging_info->resource_url_info().url_size());
  EXPECT_FALSE(logging_info->rewriter_info(0).has_rewrite_resource_info());
}


TEST_F(LogRecordTest, LogOnlyUrlIndices) {
  LoggingInfo* logging_info = log_record_->logging_info();
  log_record_->SetLogUrlIndices(true);

  // Url not passed as argument.
  log_record_->SetRewriterLoggingStatus("z", RewriterApplication::APPLIED_OK);
  EXPECT_EQ(1, logging_info->rewriter_info_size());
  EXPECT_EQ(0, log_record_->logging_info()->resource_url_info().url_size());
  EXPECT_FALSE(logging_info->rewriter_info(0).has_rewrite_resource_info());

  // Allow logging  not set.
  log_record_->SetRewriterLoggingStatus("z1", "url",
                                        RewriterApplication::APPLIED_OK);
  EXPECT_EQ(0, logging_info->resource_url_info().url_size());
  EXPECT_TRUE(logging_info->rewriter_info(1).has_rewrite_resource_info());
  EXPECT_EQ(0, logging_info->rewriter_info(1).rewrite_resource_info().
            original_resource_url_index());

  // NOT_APPLIED case.
  log_record_->SetRewriterLoggingStatus("z2", "url",
                                        RewriterApplication::NOT_APPLIED);
  EXPECT_EQ(0, logging_info->resource_url_info().url_size());
  EXPECT_TRUE(logging_info->rewriter_info(2).has_rewrite_resource_info());
  EXPECT_EQ(0, logging_info->rewriter_info(1).rewrite_resource_info().
            original_resource_url_index());
}

TEST_F(LogRecordTest, LogUrls) {
  LoggingInfo* logging_info = log_record_->logging_info();
  log_record_->SetAllowLoggingUrls(true);

  // Allow logging set and url passed.
  log_record_->SetRewriterLoggingStatus("z1", "url",
                                        RewriterApplication::APPLIED_OK);
  EXPECT_EQ(1, logging_info->resource_url_info().url_size());
  EXPECT_EQ("url", logging_info->resource_url_info().url(0));
  EXPECT_EQ(0, logging_info->rewriter_info(0).rewrite_resource_info().
            original_resource_url_index());

  // Another record with same url.
  log_record_->SetRewriterLoggingStatus("z2", "url",
                                        RewriterApplication::APPLIED_OK);
  EXPECT_EQ(1, logging_info->resource_url_info().url_size());
  EXPECT_EQ(0, logging_info->rewriter_info(1).rewrite_resource_info().
            original_resource_url_index());

  // Record with different url
  log_record_->SetRewriterLoggingStatus("z3", "url3",
                                        RewriterApplication::APPLIED_OK);
  EXPECT_EQ(2, logging_info->resource_url_info().url_size());
  EXPECT_EQ(1, logging_info->rewriter_info(2).rewrite_resource_info().
            original_resource_url_index());
  EXPECT_EQ("url3", logging_info->resource_url_info().url(1));
}

TEST_F(LogRecordDeathTest, CommasFail) {
  log_record_->SetRewriterLoggingStatus("z,z", RewriterApplication::APPLIED_OK);
  EXPECT_DEBUG_DEATH(log_record_->AppliedRewritersString(), "comma");
}

TEST_F(LogRecordTest, NumHtmlCriticalImages) {
  EXPECT_FALSE(log_record_->logging_info()->has_num_html_critical_images());
  EXPECT_EQ(-1, log_record_->logging_info()->num_html_critical_images());
  log_record_->SetNumHtmlCriticalImages(3);
  EXPECT_EQ(3, log_record_->logging_info()->num_html_critical_images());
}

TEST_F(LogRecordTest, NumCssCriticalImages) {
  EXPECT_FALSE(log_record_->logging_info()->has_num_css_critical_images());
  EXPECT_EQ(-1, log_record_->logging_info()->num_css_critical_images());
  log_record_->SetNumCssCriticalImages(2);
  EXPECT_EQ(2, log_record_->logging_info()->num_css_critical_images());
}

TEST_F(LogRecordTest, SetCriticalCssInfo) {
  EXPECT_FALSE(log_record_->logging_info()->has_critical_css_info());
  log_record_->SetCriticalCssInfo(11, 22, 33);
  EXPECT_TRUE(log_record_->logging_info()->has_critical_css_info());
  const CriticalCssInfo& info =
      log_record_->logging_info()->critical_css_info();
  EXPECT_EQ(11, info.critical_inlined_bytes());
  EXPECT_EQ(22, info.original_external_bytes());
  EXPECT_EQ(33, info.overhead_bytes());
}

TEST_F(LogRecordTest, LogRewriterApplicationStatus) {
  log_record_->LogRewriterHtmlStatus("aa", RewriterHtmlApplication::ACTIVE);
  log_record_->LogRewriterApplicationStatus("aa",
                                            RewriterApplication::APPLIED_OK);
  log_record_->LogRewriterApplicationStatus("aa",
                                            RewriterApplication::NOT_APPLIED);
  log_record_->LogRewriterApplicationStatus("aa",
                                            RewriterApplication::NOT_APPLIED);

  log_record_->LogRewriterApplicationStatus(
      "bb", RewriterApplication::PROPERTY_NOT_FOUND);

  log_record_->LogRewriterHtmlStatus("cc", RewriterHtmlApplication::ACTIVE);
  log_record_->LogRewriterApplicationStatus("cc",
                                            RewriterApplication::APPLIED_OK);

  log_record_->LogRewriterHtmlStatus("dd", RewriterHtmlApplication::DISABLED);

  log_record_->WriteLog();

  // Verify that logging info has the following contents:
  //  rewriter_stats < id: "aa"
  //    html_status: ACTIVE
  //    status_counts { application_status: 1 count: 1 }
  //    status_counts { application_status: 2 count: 2 } >
  //  rewriter_stats < id: "bb"
  //    html_status: UNKNOWN_STATUS
  //    status_counts { application_status: 3 count: 1 } > "
  //  rewriter_stats < id: "cc"
  //    html_status: ACTIVE
  //    status_counts { application_status: 1 count: 1 } >
  //  rewriter_stats < id: "dd"
  //    html_status: DISABLED >
  const LoggingInfo& logged = *log_record_->logging_info();
  EXPECT_EQ(4, logged.rewriter_stats_size());

  EXPECT_EQ("aa", logged.rewriter_stats(0).id());
  EXPECT_EQ(RewriterHtmlApplication::ACTIVE,
            logged.rewriter_stats(0).html_status());
  ASSERT_EQ(2, logged.rewriter_stats(0).status_counts_size());
  EXPECT_EQ(RewriterApplication::APPLIED_OK,
            logged.rewriter_stats(0).status_counts(0).application_status());
  EXPECT_EQ(1, logged.rewriter_stats(0).status_counts(0).count());
  EXPECT_EQ(RewriterApplication::NOT_APPLIED,
            logged.rewriter_stats(0).status_counts(1).application_status());
  EXPECT_EQ(2, logged.rewriter_stats(0).status_counts(1).count());

  // If any application status count is logged, the filter is considered ACTIVE.
  EXPECT_EQ("bb", logged.rewriter_stats(1).id());
  EXPECT_EQ(RewriterHtmlApplication::ACTIVE,
            logged.rewriter_stats(1).html_status());
  ASSERT_EQ(1, logged.rewriter_stats(1).status_counts_size());
  EXPECT_EQ(RewriterApplication::PROPERTY_NOT_FOUND,
            logged.rewriter_stats(1).status_counts(0).application_status());
  EXPECT_EQ(1, logged.rewriter_stats(1).status_counts(0).count());

  EXPECT_EQ("cc", logged.rewriter_stats(2).id());
  EXPECT_EQ(RewriterHtmlApplication::ACTIVE,
            logged.rewriter_stats(2).html_status());
  ASSERT_EQ(1, logged.rewriter_stats(1).status_counts_size());
  EXPECT_EQ(RewriterApplication::APPLIED_OK,
            logged.rewriter_stats(2).status_counts(0).application_status());
  EXPECT_EQ(1, logged.rewriter_stats(2).status_counts(0).count());

  EXPECT_EQ("dd", logged.rewriter_stats(3).id());
  EXPECT_EQ(RewriterHtmlApplication::DISABLED,
            logged.rewriter_stats(3).html_status());
  ASSERT_EQ(0, logged.rewriter_stats(3).status_counts_size());

  // We do not yet populate the applied rewriters string.
  EXPECT_EQ("", log_record_->AppliedRewritersString());
}

TEST_F(LogRecordTest, LogDeviceInfo) {
  EXPECT_FALSE(log_record_->logging_info()->has_device_info());
  log_record_->LogDeviceInfo(
      UserAgentMatcher::kMobile,
      true /* supports_image_inlining */,
      true /* supports_lazyload_images */,
      true /* supports_critical_images_beacon */,
      true /* supports_deferjs */,
      true /* supports_webp */,
      true /* supports_webplossless_alpha */,
      true /* is_bot */,
      true /* supports_split_html */,
      true /* can_preload_resources */);
  EXPECT_TRUE(log_record_->logging_info()->has_device_info());
  EXPECT_EQ(UserAgentMatcher::kMobile,
            log_record_->logging_info()->device_info().device_type());
  EXPECT_TRUE(device_info().supports_image_inlining());
  EXPECT_TRUE(device_info().supports_lazyload_images());
  EXPECT_TRUE(device_info().supports_critical_images_beacon());
  EXPECT_TRUE(device_info().supports_deferjs());
  EXPECT_TRUE(device_info().supports_webp());
  EXPECT_TRUE(device_info().supports_webplossless_alpha());
  EXPECT_TRUE(device_info().is_bot());
  EXPECT_TRUE(device_info().supports_split_html());
  EXPECT_TRUE(device_info().can_preload_resources());
}

TEST_F(LogRecordTest, LogIsXhr) {
  EXPECT_FALSE(log_record_->logging_info()->has_is_xhr());
  log_record_->LogIsXhr(true);
  EXPECT_TRUE(log_record_->logging_info()->has_is_xhr());
  EXPECT_TRUE(log_record_->logging_info()->is_xhr());
}

}  // namespace net_instaweb
