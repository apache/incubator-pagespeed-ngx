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

#include "net/instaweb/util/public/shared_mem_statistics_test_base.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/shared_mem_test_base.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {

const char kPrefix[] = "/prefix/";
const char kVar1[] = "v1";
const char kVar2[] = "num_flushes";
const char kHist1[] = "H1";
const char kHist2[] = "Html Time us Histogram";
const char kStatsLogFile[] = "mod_pagespeed_stats.log";
}  // namespace

SharedMemStatisticsTestBase::SharedMemStatisticsTestBase(
    SharedMemTestEnv* test_env)
    : test_env_(test_env),
      shmem_runtime_(test_env->CreateSharedMemRuntime()) {
}

void SharedMemStatisticsTestBase::SetUp() {
  // This time is in the afternoon of 17 July 2012.
  timer_.reset(new MockTimer(1342567288560ULL));
  thread_system_.reset(ThreadSystem::CreateThreadSystem());
  file_system_.reset(new MemFileSystem(thread_system_.get(), timer_.get()));
  stats_.reset(new SharedMemStatistics(
      3000, kStatsLogFile, true, kPrefix, shmem_runtime_.get(),
      &handler_, file_system_.get(), timer_.get()));
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
  Variable* v1 = stats->AddVariable(kVar1);
  Variable* v2 = stats->AddVariable(kVar2);
  return ((v1 != NULL) && (v2 != NULL));
}

bool SharedMemStatisticsTestBase::AddHistograms(SharedMemStatistics* stats) {
  Histogram* hist1 = stats->AddHistogram(kHist1);
  Histogram* hist2 = stats->AddHistogram(kHist2);
  return ((hist1 != NULL) && (hist2 != NULL));
}

SharedMemStatistics* SharedMemStatisticsTestBase::ChildInit() {
  scoped_ptr<SharedMemStatistics> stats(
      new SharedMemStatistics(3000, kStatsLogFile, true,
                              kPrefix, shmem_runtime_.get(), &handler_,
                              file_system_.get(), timer_.get()));
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

  Variable* v1 = stats_->GetVariable(kVar1);
  Variable* v2 = stats_->GetVariable(kVar2);
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

  Variable* v1 = stats->GetVariable(kVar1);
  Histogram* hist1 = stats->GetHistogram(kHist1);
  stats->Init(false, &handler_);
  Variable* v2 = stats->GetVariable(kVar2);
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

  Variable* v1 = stats_->GetVariable(kVar1);
  Variable* v2 = stats_->GetVariable(kVar2);
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

  Variable* v1 = stats->GetVariable(kVar1);
  stats->Init(false, &handler_);
  Variable* v2 = stats->GetVariable(kVar2);

  v1->Set(v1->Get() * v1->Get());
  v2->Set(v2->Get() * v2->Get());
}

void SharedMemStatisticsTestBase::TestClear() {
  // We can clear things from the kid
  ParentInit();

  Variable* v1 = stats_->GetVariable(kVar1);
  Variable* v2 = stats_->GetVariable(kVar2);
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

  Variable* v1 = stats_->GetVariable(kVar1);
  Variable* v2 = stats_->GetVariable(kVar2);
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

  GoogleString dump;
  StringWriter writer(&dump);
  stats_->Dump(&writer, &handler_);
  GoogleString result = "timestamp_: 1342567288560\n"
                        "v1:                    13\n"
                        "num_flushes:           37\n";
  EXPECT_EQ(result, dump);
}

void SharedMemStatisticsTestBase::TestAddChild() {
  scoped_ptr<SharedMemStatistics> stats(ChildInit());
  stats->Init(false, &handler_);
  Variable* v1 = stats->GetVariable(kVar1);
  Variable* v2 = stats->GetVariable(kVar2);
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
  hist1->Add(100);
  // 10 is the max_value, it shouldn't have been added to histogram.
  EXPECT_EQ(2, hist1->Count());
  EXPECT_EQ(1, hist1->Minimum());
  EXPECT_EQ(5, hist1->Maximum());
  // Test when negative buckets are enabled.
  hist1->Clear();
  hist1->SetMaxValue(100);
  hist1->EnableNegativeBuckets();
  hist1->Add(-100);
  hist1->Add(-5);
  hist1->Add(0);
  hist1->Add(5);
  hist1->Add(100);
  // -10 and 10 shouldn't have been added to histogram.
  EXPECT_EQ(3, hist1->Count());
  EXPECT_EQ(-5, hist1->Minimum());
  EXPECT_EQ(5, hist1->Maximum());
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
  EXPECT_TRUE(Contains(html_graph, "5)</td>"));
  EXPECT_TRUE(Contains(html_graph, "25.0%"));
  EXPECT_TRUE(Contains(html_graph, "15)</td>"));
  EXPECT_TRUE(Contains(html_graph, "12.5%"));
  EXPECT_TRUE(Contains(html_graph, "37.5%"));
  EXPECT_TRUE(Contains(html_graph, "setHistogram"));
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

void SharedMemStatisticsTestBase::TestTimedVariableEmulation() {
  // Simple test of timed variable emulation. Not using ParentInit
  // here since we want to add some custom things.
  Variable* a = stats_->AddVariable("A");
  TimedVariable* b = stats_->AddTimedVariable("B", "some group");
  stats_->Init(true, &handler_);

  b->IncBy(42);
  EXPECT_EQ(0, a->Get());
  EXPECT_EQ(42, b->Get(TimedVariable::START));
}

void SharedMemStatisticsTestBase::TestConsoleStatisticsLogger() {
  ParentInit();
  // See IMPORTANT note in shared_mem_statistics.cc
  EXPECT_TRUE(stats_->IsIgnoredVariable("timestamp_"));
  Variable* v1 = stats_->GetVariable(kVar1);
  Variable* v2 = stats_->GetVariable(kVar2);
  v1->Set(2300);
  v2->Set(300);
  Histogram* h1 = stats_->GetHistogram(kHist1);
  h1->SetMaxValue(2500);
  h1->Add(1);
  h1->Add(2);
  h1->Add(10);
  h1->Add(20);
  h1->Add(100);
  h1->Add(200);
  h1->Add(1000);
  h1->Add(2000);
  Histogram* h2 = stats_->GetHistogram(kHist2);
  h2->SetMaxValue(2500);
  h2->Add(1);
  h2->Add(2);
  h2->Add(10);
  h2->Add(20);
  h2->Add(100);
  h2->Add(200);
  h2->Add(1000);
  h2->Add(2000);
  GoogleString logger_output;
  StringWriter logger_writer(&logger_output);
  stats_->DumpConsoleVarsToWriter(timer_->NowMs(), &logger_writer, &handler_);
  GoogleString result = "timestamp: 1342567288560\n"
                        "num_flushes: 300\n"
                        "histogram: Html Time us Histogram\n"
                        "[0.000000, 5.000000): 2.000000\n"
                        "[10.000000, 15.000000): 1.000000\n"
                        "[20.000000, 25.000000): 1.000000\n"
                        "[100.000000, 105.000000): 1.000000\n"
                        "[200.000000, 205.000000): 1.000000\n"
                        "[1000.000000, 1005.000000): 1.000000\n"
                        "[2000.000000, 2005.000000): 1.000000\n";
  EXPECT_EQ(result, logger_output);
}

}  // namespace net_instaweb
