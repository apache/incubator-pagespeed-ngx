// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: morlovich@google.com (Maksim Orlovich)

#include "pagespeed/kernel/sharedmem/shared_mem_statistics_test_base.h"

#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/statistics_template.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/sharedmem/shared_mem_test_base.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

namespace {

const char kPrefix[] = "/prefix/";
const char kVar1[] = "v1";
const char kVar2[] = "num_flushes";
const char kHist1[] = "H1";
const char kHist2[] = "Html Time us Histogram";

// We cannot init the logger unless all stats are initialized.
const char kStatsLogFile[] = "";

}  // namespace

const int64 SharedMemStatisticsTestBase::kLogIntervalMs = 3 * Timer::kSecondMs;
// Set this small for TestLogfileTrimming.
const int64 SharedMemStatisticsTestBase::kMaxLogfileSizeKb = 10;

SharedMemStatisticsTestBase::SharedMemStatisticsTestBase(
    SharedMemTestEnv* test_env)
    : thread_system_(Platform::CreateThreadSystem()),
      handler_(thread_system_->NewMutex()),
      test_env_(test_env),
      shmem_runtime_(test_env->CreateSharedMemRuntime()) {
}

SharedMemStatisticsTestBase::SharedMemStatisticsTestBase()
    : thread_system_(Platform::CreateThreadSystem()),
      handler_(thread_system_->NewMutex()) {
}

void SharedMemStatisticsTestBase::SetUp() {
  timer_.reset(
      new MockTimer(thread_system_->NewMutex(), MockTimer::kApr_5_2010_ms));
  file_system_.reset(new MemFileSystem(thread_system_.get(), timer_.get()));
  stats_.reset(new SharedMemStatistics(
      kLogIntervalMs, kMaxLogfileSizeKb, kStatsLogFile, false /* no logging */,
      kPrefix, shmem_runtime_.get(), &handler_, file_system_.get(),
      timer_.get()));
}

void SharedMemStatisticsTestBase::TearDown() {
  stats_->GlobalCleanup(&handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

bool SharedMemStatisticsTestBase::CreateChild(TestMethod method) {
  Function* callback =
      new MemberFunction0<SharedMemStatisticsTestBase>(method, this);
  return test_env_->CreateChild(callback);
}

bool SharedMemStatisticsTestBase::AddVars(SharedMemStatistics* stats) {
  UpDownCounter* v1 = stats->AddUpDownCounter(kVar1);
  UpDownCounter* v2 = stats->AddUpDownCounter(kVar2);
  return ((v1 != NULL) && (v2 != NULL));
}

bool SharedMemStatisticsTestBase::AddHistograms(SharedMemStatistics* stats) {
  Histogram* hist1 = stats->AddHistogram(kHist1);
  Histogram* hist2 = stats->AddHistogram(kHist2);
  return ((hist1 != NULL) && (hist2 != NULL));
}

SharedMemStatistics* SharedMemStatisticsTestBase::ChildInit() {
  scoped_ptr<SharedMemStatistics> stats(new SharedMemStatistics(
      kLogIntervalMs, kMaxLogfileSizeKb, kStatsLogFile, false /* no logging */,
      kPrefix, shmem_runtime_.get(), &handler_, file_system_.get(),
      timer_.get()));
  if (!AddVars(stats.get()) || !AddHistograms(stats.get())) {
    test_env_->ChildFailed();
    return NULL;
  }

  stats->Init(false, &handler_);
  return stats.release();
}

void SharedMemStatisticsTestBase::ParentInit() {
  EXPECT_TRUE(AddVars(stats_.get()));
  EXPECT_TRUE(AddHistograms(stats_.get()));
  stats_->Init(true, &handler_);
}

void SharedMemStatisticsTestBase::TestCreate() {
  // Basic initialization/reading/cleanup test
  ParentInit();

  UpDownCounter* v1 = stats_->GetUpDownCounter(kVar1);
  UpDownCounter* v2 = stats_->GetUpDownCounter(kVar2);
  EXPECT_EQ(0, v1->Get());
  EXPECT_EQ(0, v2->Get());
  Histogram* hist1 = stats_->GetHistogram(kHist1);
  Histogram* hist2 = stats_->GetHistogram(kHist2);
  EXPECT_EQ(0, hist1->Maximum());
  EXPECT_EQ(0, hist2->Maximum());

  ASSERT_TRUE(CreateChild(&SharedMemStatisticsTestBase::TestCreateChild));
  test_env_->WaitForChildren();
}

void SharedMemStatisticsTestBase::TestCreateChild() {
  scoped_ptr<SharedMemStatistics> stats(ChildInit());

  UpDownCounter* v1 = stats->GetUpDownCounter(kVar1);
  Histogram* hist1 = stats->GetHistogram(kHist1);
  stats->Init(false, &handler_);
  UpDownCounter* v2 = stats->GetUpDownCounter(kVar2);
  Histogram* hist2 = stats->GetHistogram(kHist2);
  // We create one var & hist before SHM attach, one after for test coverage.

  if (v1->Get() != 0 || hist1->Count() != 0) {
    test_env_->ChildFailed();
  }

  if (v2->Get() != 0 || hist2->Count() != 0) {
    test_env_->ChildFailed();
  }
}

void SharedMemStatisticsTestBase::TestSet() {
  // -> Set works as well, propagates right
  ParentInit();

  UpDownCounter* v1 = stats_->GetUpDownCounter(kVar1);
  UpDownCounter* v2 = stats_->GetUpDownCounter(kVar2);
  EXPECT_EQ(0, v1->Get());
  EXPECT_EQ(0, v2->Get());
  v1->Set(3);
  v2->Set(17);
  EXPECT_EQ(3, v1->Get());
  EXPECT_EQ(17, v2->Get());

  ASSERT_TRUE(CreateChild(&SharedMemStatisticsTestBase::TestSetChild));
  test_env_->WaitForChildren();
  EXPECT_EQ(3*3, v1->Get());
  EXPECT_EQ(17*17, v2->Get());
}

void SharedMemStatisticsTestBase::TestSetChild() {
  scoped_ptr<SharedMemStatistics> stats(ChildInit());

  UpDownCounter* v1 = stats->GetUpDownCounter(kVar1);
  stats->Init(false, &handler_);
  UpDownCounter* v2 = stats->GetUpDownCounter(kVar2);

  v1->Set(v1->Get() * v1->Get());
  v2->Set(v2->Get() * v2->Get());
}

void SharedMemStatisticsTestBase::TestClear() {
  // We can clear things from the kid
  ParentInit();

  UpDownCounter* v1 = stats_->GetUpDownCounter(kVar1);
  UpDownCounter* v2 = stats_->GetUpDownCounter(kVar2);
  EXPECT_EQ(0, v1->Get());
  EXPECT_EQ(0, v2->Get());
  v1->Set(3);
  v2->Set(17);
  EXPECT_EQ(3, v1->Get());
  EXPECT_EQ(17, v2->Get());

  Histogram* hist1 = stats_->GetHistogram(kHist1);
  Histogram* hist2 = stats_->GetHistogram(kHist2);
  EXPECT_EQ(0, hist1->Count());
  EXPECT_EQ(0, hist2->Count());
  hist1->Add(1);
  hist2->Add(2);
  hist2->Add(4);
  EXPECT_EQ(1, hist1->Count());
  EXPECT_EQ(2, hist2->Count());
  EXPECT_EQ(1, hist1->Maximum());
  EXPECT_EQ(2, hist2->Minimum());

  ASSERT_TRUE(CreateChild(&SharedMemStatisticsTestBase::TestClearChild));
  test_env_->WaitForChildren();
  EXPECT_EQ(0, v1->Get());
  EXPECT_EQ(0, v2->Get());
  EXPECT_EQ(0, hist1->Count());
  EXPECT_EQ(0, hist2->Count());
  EXPECT_EQ(0, hist1->Maximum());
  EXPECT_EQ(0, hist2->Minimum());
}

void SharedMemStatisticsTestBase::TestClearChild() {
  scoped_ptr<SharedMemStatistics> stats(ChildInit());
  // Double check the child process gets the data in Histogram before clears it.
  Histogram* hist1 = stats->GetHistogram(kHist1);
  Histogram* hist2 = stats->GetHistogram(kHist2);
  EXPECT_EQ(1, hist1->Count());
  EXPECT_EQ(2, hist2->Count());
  EXPECT_EQ(1, hist1->Maximum());
  EXPECT_EQ(2, hist2->Minimum());

  stats->Init(false, &handler_);
  stats->Clear();
}

void SharedMemStatisticsTestBase::TestAdd() {
  ParentInit();

  UpDownCounter* v1 = stats_->GetUpDownCounter(kVar1);
  UpDownCounter* v2 = stats_->GetUpDownCounter(kVar2);
  Histogram* hist1 = stats_->GetHistogram(kHist1);
  Histogram* hist2 = stats_->GetHistogram(kHist2);
  EXPECT_EQ(0, v1->Get());
  EXPECT_EQ(0, v2->Get());
  EXPECT_EQ(0, hist1->Count());
  EXPECT_EQ(0, hist2->Count());
  v1->Set(3);
  v2->Set(17);
  EXPECT_EQ(3, v1->Get());
  EXPECT_EQ(17, v2->Get());

  // We will add 10x 1 to v1, and 10x 2 to v2.
  // Add 10x (1,2) to hist1, and 10x (3,4) to hist2.
  for (int i = 0; i < 10; ++i) {
    ASSERT_TRUE(CreateChild(&SharedMemStatisticsTestBase::TestAddChild));
  }
  test_env_->WaitForChildren();
  EXPECT_EQ(3 + 10 * 1, v1->Get());
  EXPECT_EQ(17 + 10 * 2, v2->Get());
  EXPECT_EQ(20, hist1->Count());
  EXPECT_EQ(1, hist1->Minimum());
  EXPECT_EQ(2, hist1->Maximum());
  EXPECT_EQ(20, hist2->Count());
  EXPECT_EQ(3, hist2->Minimum());
  EXPECT_EQ(4, hist2->Maximum());
}

void SharedMemStatisticsTestBase::TestSetReturningPrevious() {
  ParentInit();

  UpDownCounter* v1 = stats_->GetUpDownCounter(kVar1);
  EXPECT_EQ(0, v1->Get());
  EXPECT_EQ(0, v1->SetReturningPreviousValue(5));
  EXPECT_EQ(5, v1->SetReturningPreviousValue(-3));
  EXPECT_EQ(-3, v1->SetReturningPreviousValue(10));
  EXPECT_EQ(10, v1->Get());
}

void SharedMemStatisticsTestBase::TestAddChild() {
  scoped_ptr<SharedMemStatistics> stats(ChildInit());
  stats->Init(false, &handler_);
  UpDownCounter* v1 = stats->GetUpDownCounter(kVar1);
  UpDownCounter* v2 = stats->GetUpDownCounter(kVar2);
  Histogram* hist1 = stats->GetHistogram(kHist1);
  Histogram* hist2 = stats->GetHistogram(kHist2);
  v1->Add(1);
  v2->Add(2);
  hist1->Add(1);
  hist1->Add(2);
  hist2->Add(3);
  hist2->Add(4);
}

// This function tests the Histogram options with multi-processes.
void SharedMemStatisticsTestBase::TestHistogram() {
  ParentInit();
  Histogram* hist1 = stats_->GetHistogram(kHist1);
  hist1->SetMaxValue(200);

  // Test Avg, Min, Max, Median, Percentile, STD, Count.
  // Add 0 to 14 to hist1.
  for (int i = 0; i <= 14; ++i) {
    hist1->Add(i);
  }
  EXPECT_EQ(15, hist1->Count());
  EXPECT_EQ(0, hist1->Minimum());
  EXPECT_EQ(14, hist1->Maximum());
  EXPECT_EQ(7, hist1->Average());
  EXPECT_NEAR(4.32049, hist1->StandardDeviation(), 0.1);
  // Note Median() invokes Percentile(50), so it's estimated.
  EXPECT_NEAR(7, hist1->Median(), 1);
  // The return of Percentile() is an estimated value. It's more accurate when
  // the histogram has more numbers.
  EXPECT_NEAR(3, hist1->Percentile(20), 1);

  // Test EnableNegativeBuckets();
  hist1->EnableNegativeBuckets();
  hist1->SetMaxValue(100);
  // Child process adds 1, 2 to the histogram.
  ASSERT_TRUE(CreateChild(&SharedMemStatisticsTestBase::TestAddChild));
  test_env_->WaitForChildren();
  EXPECT_EQ(2, hist1->Count());
  EXPECT_EQ(1, hist1->Minimum());
  EXPECT_EQ(2, hist1->Maximum());
  hist1->Add(-50);
  EXPECT_EQ(-50, hist1->Minimum());

  // Test overflow.
  // The value range of histogram is [min_value, max_value) or
  // (-max_value, max_value) if enabled negative buckets.
  // First test when histogram does not have negative buckets.
  hist1->Clear();
  hist1->SetMaxValue(100);
  hist1->Add(1);
  hist1->Add(5);
  EXPECT_EQ(0, hist1->BucketCount(hist1->NumBuckets() - 1));
  hist1->Add(100);
  // 10 is the max_value, so 100 should be added to the histogram, but into the
  // last bucket.
  EXPECT_EQ(1, hist1->BucketCount(hist1->NumBuckets() - 1));
  EXPECT_EQ(3, hist1->Count());
  EXPECT_EQ(1, hist1->Minimum());
  EXPECT_EQ(100, hist1->Maximum());

  // Test when negative buckets are enabled.
  // -101 and 101 are just outside limits, so they should have been stuck into
  // the extreme buckets.
  hist1->Clear();
  hist1->SetMaxValue(100);
  hist1->EnableNegativeBuckets();
  EXPECT_EQ(0, hist1->BucketCount(0));
  hist1->Add(-101);
  EXPECT_EQ(1, hist1->BucketCount(0));
  hist1->Add(-5);
  hist1->Add(0);
  hist1->Add(5);
  EXPECT_EQ(0, hist1->BucketCount(hist1->NumBuckets() - 1));
  hist1->Add(101);
  EXPECT_EQ(1, hist1->BucketCount(hist1->NumBuckets() - 1));

  EXPECT_EQ(5, hist1->Count());
  EXPECT_EQ(-101, hist1->Minimum());
  EXPECT_EQ(101, hist1->Maximum());
}

bool SharedMemStatisticsTestBase::Contains(const StringPiece& html,
                                           const StringPiece& pattern) {
  return (html.find(pattern) != GoogleString::npos);
}

// This function tests the Histogram graph is written to html.
void SharedMemStatisticsTestBase::TestHistogramRender() {
  // Test header.
  // A basic sanity test showing that even there's no data in histograms,
  // the script, histogram title, histogram table header are written to html.
  // The message written to html should look like:
  //   <td>H1 (click to view)</td> ...
  //   Raw Histogram Data ...
  //   <script> ... </script>
  // ParentInit() adds two histograms: H1 and Html Time us Histogram.
  ParentInit();
  GoogleString html;
  StringWriter writer(&html);
  stats_->RenderHistograms(&writer, &handler_);
  EXPECT_TRUE(Contains(html, "No histogram data yet.  Refresh once there is"))
      << "zero state message";
  EXPECT_FALSE(Contains(html, "setHistogram"));

  // Test basic graph.
  Histogram* h1 = stats_->GetHistogram(kHist1);
  // Default max_buckets is 500, with max_value = 2500, bucket width is 5.
  h1->SetMaxValue(2500);
  h1->Add(1);
  h1->Add(2);
  h1->Add(10);
  h1->Add(20);
  h1->Add(100);
  h1->Add(200);
  h1->Add(1000);
  h1->Add(2000);
  // The table of histogram graph should look like:
  // [0,5) 2 25.0% 25.0% ||||||
  // [10,15) 1 12.5% 37.5% |||
  // ...
  // Check if the above number appears.
  GoogleString html_graph;
  StringWriter writer_graph(&html_graph);
  stats_->RenderHistograms(&writer_graph, &handler_);
  EXPECT_FALSE(Contains(html_graph, "inf"));
  EXPECT_TRUE(Contains(html_graph, "5)</td>"));
  EXPECT_TRUE(Contains(html_graph, "25.0%"));
  EXPECT_TRUE(Contains(html_graph, "15)</td>"));
  EXPECT_TRUE(Contains(html_graph, "12.5%"));
  EXPECT_TRUE(Contains(html_graph, "37.5%"));
  EXPECT_TRUE(Contains(html_graph, "setHistogram"));

  // Now add something out-of-range, that should also add a negative infinity
  // bucket
  h1->Add(-10);
  html_graph.clear();
  stats_->RenderHistograms(&writer_graph, &handler_);
  EXPECT_TRUE(Contains(html_graph, "-&infin;,</td>"));
}

void SharedMemStatisticsTestBase::TestHistogramNoExtraClear() {
  // Make sure we don't lose histogram data when a child process
  // redundantly applies the same settings.
  ParentInit();
  Histogram* h1 = stats_->GetHistogram(kHist1);
  h1->EnableNegativeBuckets();
  h1->SetMaxValue(100.0);
  h1->Add(42);
  EXPECT_EQ(1, h1->Count());
  ASSERT_TRUE(CreateChild(
      &SharedMemStatisticsTestBase::TestHistogramNoExtraClearChild));
  test_env_->WaitForChildren();
  EXPECT_EQ(1, h1->Count());
}

void SharedMemStatisticsTestBase::TestHistogramNoExtraClearChild() {
  scoped_ptr<SharedMemStatistics> stats(ChildInit());
  Histogram* h1 = stats->GetHistogram(kHist1);
  // This would previously lose the data.
  h1->EnableNegativeBuckets();
  h1->SetMaxValue(100.0);
}

void SharedMemStatisticsTestBase::TestHistogramExtremeBuckets() {
  ParentInit();
  Histogram* h1 = stats_->GetHistogram(kHist1);
  h1->SetMaxValue(100.0);
  h1->Add(0);
  // The median will be approximated, but it really ought to be
  // in the [0, End of first bucket] range.
  EXPECT_LE(0.0, h1->Median());
  EXPECT_LE(h1->Median(), h1->BucketLimit(0));
}

void SharedMemStatisticsTestBase::TestTimedVariableEmulation() {
  // Simple test of timed variable emulation. Not using ParentInit
  // here since we want to add some custom things.
  UpDownCounter* a = stats_->AddUpDownCounter("A");
  TimedVariable* b = stats_->AddTimedVariable("B", "some group");
  stats_->Init(true, &handler_);

  b->IncBy(42);
  EXPECT_EQ(0, a->Get());
  EXPECT_EQ(42, b->Get(TimedVariable::START));
}

}  // namespace net_instaweb
