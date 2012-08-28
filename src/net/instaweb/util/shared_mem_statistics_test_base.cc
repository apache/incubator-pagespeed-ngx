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
#include "net/instaweb/util/public/json.h"
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
  Variable* a = stats_->AddVariable("A");
  TimedVariable* b = stats_->AddTimedVariable("B", "some group");
  stats_->Init(true, &handler_);

  b->IncBy(42);
  EXPECT_EQ(0, a->Get());
  EXPECT_EQ(42, b->Get(TimedVariable::START));
}

GoogleString SharedMemStatisticsTestBase::CreateHistogramDataResponse(
    const GoogleString & histogram_name, bool is_long_response) {
  GoogleString histogram_data = "histogram#" + histogram_name;
  if (is_long_response) {
    histogram_data += "#0.000000#5.000000#2.000000"
                      "#10.000000#15.000000#1.000000"
                      "#20.000000#25.000000#1.000000"
                      "#100.000000#105.000000#1.000000"
                      "#200.000000#205.000000#1.000000"
                      "#1000.000000#1005.000000#1.000000"
                      "#2000.000000#2005.000000#1.000000\n";
  } else {
    histogram_data += "#0.000000#5.000000#2.000000"
                      "#10.000000#15.000000#1.000000"
                      "#20.000000#25.000000#1.000000"
                      "#100.000000#105.000000#1.000000"
                      "#200.000000#205.000000#1.000000\n";
  }
  return histogram_data;
}

GoogleString SharedMemStatisticsTestBase::CreateVariableDataResponse(
  bool has_unused_variable, bool first) {
  GoogleString var_data;
  if (first) {
    var_data = "num_flushes: 300\n"
               "cache_hits: 400\n"
               "cache_misses: 500\n"
               "slurp_404_count: 600\n";
  } else {
    var_data = "num_flushes: 310\n"
               "cache_hits: 410\n"
               "cache_misses: 510\n"
               "slurp_404_count: 610\n";
  }
  if (has_unused_variable) {
    var_data += "random_unused_var: 700\n";
  }
  return var_data;
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
  h2->Add(5000);  // bigger than max
  GoogleString logger_output;
  StringWriter logger_writer(&logger_output);
  stats_->DumpConsoleVarsToWriter(timer_->NowMs(), &logger_writer, &handler_);
  GoogleString result = "timestamp: 1342567288560\n"
                        "num_flushes: 300\n"
                        "histogram#Html Time us Histogram"
                        "#0.000000#5.000000#2.000000"
                        "#10.000000#15.000000#1.000000"
                        "#20.000000#25.000000#1.000000"
                        "#100.000000#105.000000#1.000000"
                        "#200.000000#205.000000#1.000000"
                        "#1000.000000#1005.000000#1.000000"
                        "#2000.000000#2005.000000#1.000000"
                        "#2500.000000#inf#1.000000\n";
  EXPECT_EQ(result, logger_output);
}

// Output parameters: variable data and corresponding set of variable titles,
// histogram data and corresponding set of histogram titles, and returns a
// logfile-formatted data string using this data. Used for easy creation of
// parsing material in testing functions.
GoogleString SharedMemStatisticsTestBase::CreateFakeLogfile(
    GoogleString* var_data, GoogleString* hist_data,
    std::set<GoogleString>* var_titles, std::set<GoogleString>* hist_titles) {
  GoogleString hist_names[4] = {"Html Time us Histogram",
                               "Pagespeed Resource Latency Histogram",
                               "Backend Fetch First Byte Latency Histogram",
                               "Rewrite Latency Histogram"};
  StrAppend(hist_data, CreateHistogramDataResponse(hist_names[0], false),
            CreateHistogramDataResponse(hist_names[1], false),
            CreateHistogramDataResponse(hist_names[2], false),
            CreateHistogramDataResponse(hist_names[3], false));
  for (size_t i = 0; i < 4; i++) {
    hist_titles->insert(hist_names[i]);
  }

  // Populate variable data.
  *var_data = CreateVariableDataResponse(false, true);
  var_titles->insert("num_flushes");
  var_titles->insert("slurp_404_count");
  var_titles->insert("cache_hits");
  var_titles->insert("cache_misses");

  GoogleString last_timestamp_hist_data =
      StrCat(CreateHistogramDataResponse(hist_names[0], true),
             StrCat(CreateHistogramDataResponse(hist_names[1], true),
                    StrCat(CreateHistogramDataResponse(hist_names[2], true),
                           CreateHistogramDataResponse(hist_names[3], true))));
  GoogleString data = StrCat(*var_data, *hist_data);
  GoogleString logfile_input =
      StrCat(StrCat(StrCat("timestamp: 1300000000005\n", data),
             StrCat("timestamp: 1300000000010\n", data)),
             StrCat(StrCat("timestamp: 1300000000015\n", data),
                    StrCat("timestamp: 1300000000020\n",
                           StrCat(*var_data, last_timestamp_hist_data))));
  return logfile_input;
}

// Using TEST_F because several of the methods that need to be tested are
// private methods.
// Tests that, given a ConsoleStatisticsLogfileReader, data is accurately parsed
// into a VarMap, HistMap, and list of timestamps.
TEST_F(SharedMemStatisticsTestBase, TestParseDataFromReader) {
  SharedMemConsoleStatisticsLogger* console =
      reinterpret_cast<SharedMemConsoleStatisticsLogger*>
      (stats_->console_logger());
  GoogleString hist_data, var_data;
  std::set<GoogleString> hist_titles, var_titles;
  GoogleString logfile_input = CreateFakeLogfile(&var_data, &hist_data,
                                                 &var_titles, &hist_titles);
  StringPiece logfile_input_piece(logfile_input);
  GoogleString file_name;
  FileSystem* file_system = file_system_.get();
  bool success = file_system->WriteTempFile(kPrefix, logfile_input_piece,
                                            &file_name, &handler_);
  EXPECT_TRUE(success);

  FileSystem::InputFile* log_file =
      file_system->OpenInputFile(file_name.c_str(), &handler_);
  GoogleString output;
  int64 start_time = 1300000000000;
  int64 end_time = 1400000000000;
  int64 granularity_ms = 2;
  ConsoleStatisticsLogfileReader reader(log_file, start_time, end_time,
                                        granularity_ms, &handler_);
  std::vector<int64> list_of_timestamps;
  SharedMemConsoleStatisticsLogger::VarMap parsed_var_data;
  SharedMemConsoleStatisticsLogger::HistMap parsed_hist_data;
  console->ParseDataFromReader(var_titles, hist_titles, &reader,
                               &list_of_timestamps, &parsed_var_data,
                               &parsed_hist_data);
  // Test that the entire logfile was parsed correctly.
  EXPECT_EQ(4, parsed_var_data.size());
  EXPECT_EQ(4, parsed_hist_data.size());
  EXPECT_EQ(4, list_of_timestamps.size());

  // Test that the correct histograms was retrieved.
  EXPECT_EQ(7, parsed_hist_data["Html Time us Histogram"].size());

  file_system->Close(log_file, &handler_);
}

// Creates fake logfile data and tests that ReadNextDataBlock accurately
// extracts data from logfile-formatted text.
TEST_F(SharedMemStatisticsTestBase, TestNextDataBlock) {
  GoogleString histogram_data =
      CreateHistogramDataResponse("Html Time us Histogram", true);
  int64 start_time = 1300000000000;  // Randomly chosen times.
  int64 end_time = 1400000000000;
  int64 granularity_ms = 5;
  int64 initial_timestamp = 1342567288560;
  // Add two working cases.
  GoogleString input = "timestamp: " +
                       Integer64ToString(initial_timestamp) + "\n";
  const GoogleString first_var_data =  "num_flushes: 300\n" + histogram_data;
  input += first_var_data;
  input += "timestamp: " + Integer64ToString(initial_timestamp + 20) + "\n";
  const GoogleString second_var_data = "num_flushes: 305\n" + histogram_data;
  input += second_var_data;

  // Add case that purposefully fails granularity requirements (The difference
  // between this timestamp and the previous one is only 2ms, whereas the
  // desired granularity is 5ms).
  input += "timestamp: " + Integer64ToString(initial_timestamp + 22) + "\n";
  const GoogleString third_var_data = "num_flushes: 310\n" + histogram_data;
  input += third_var_data;

  // Add case that purposefully fails start_time requirements.
  input += "timestamp: 1200000000000\n";
  input += third_var_data;
  // Add case that purposefully fails end_time requirements.
  input += "timestamp: 1500000000000\n";
  input += third_var_data;
  // Add working case to make sure data output continues despite previous
  // requirements failing.
  input += "timestamp: " + Integer64ToString(initial_timestamp + 50) + "\n";
  input += third_var_data;
  StringPiece input_piece(input);
  GoogleString file_name;

  FileSystem* file_system = file_system_.get();
  bool success = file_system->WriteTempFile(kPrefix, input_piece, &file_name,
                                            &handler_);
  EXPECT_TRUE(success);

  FileSystem::InputFile* log_file =
      file_system->OpenInputFile(file_name.c_str(), &handler_);
  GoogleString output;
  ConsoleStatisticsLogfileReader reader(log_file, start_time, end_time,
                                        granularity_ms, &handler_);
  int64 timestamp = -1;
  // Test that the first data block is read correctly.
  success = reader.ReadNextDataBlock(&timestamp, &output);
  EXPECT_TRUE(success);
  EXPECT_EQ(first_var_data, output);
  EXPECT_EQ(initial_timestamp, timestamp);

  // Test that the second data block is read correctly.
  success = reader.ReadNextDataBlock(&timestamp, &output);
  EXPECT_TRUE(success);
  EXPECT_EQ(second_var_data, output);
  EXPECT_EQ(initial_timestamp + 20, timestamp);

  // Test that granularity, start_time, and end_time filters are working.
  success = reader.ReadNextDataBlock(&timestamp, &output);
  EXPECT_TRUE(success);
  EXPECT_EQ(third_var_data, output);
  EXPECT_EQ(initial_timestamp + 50, timestamp);

  file_system->Close(log_file, &handler_);
}

// Creates fake logfile data and tests that the data containing the variable
// timeseries information is accurately parsed.
TEST_F(SharedMemStatisticsTestBase, TestParseVarData) {
  SharedMemConsoleStatisticsLogger* console =
      reinterpret_cast<SharedMemConsoleStatisticsLogger*>
      (stats_->console_logger());
  SharedMemConsoleStatisticsLogger::VarMap parsed_var_data;
  GoogleString var_data = CreateVariableDataResponse(true, true);
  const StringPiece var_data_piece(var_data);
  std::set<GoogleString> var_titles;
  var_titles.insert("num_flushes");
  var_titles.insert("slurp_404_count");
  var_titles.insert("not_a_variable");
  console->ParseVarDataIntoMap(var_data_piece, var_titles, &parsed_var_data);

  EXPECT_NE(parsed_var_data.end(), parsed_var_data.find("num_flushes"));
  EXPECT_NE(parsed_var_data.end(), parsed_var_data.find("slurp_404_count"));

  // Test that map does not update variables that are not queried.
  EXPECT_EQ(parsed_var_data.end(), parsed_var_data.find("cache_hits"));
  EXPECT_EQ(parsed_var_data.end(), parsed_var_data.find("not_a_variable"));
  EXPECT_EQ(parsed_var_data.end(), parsed_var_data.find("random_unused_var"));

  // Test that map correctly adds data on initial run.
  EXPECT_EQ(1, parsed_var_data["num_flushes"].size());
  EXPECT_EQ("300", parsed_var_data["num_flushes"][0]);

  // Test that map is updated correctly when new data is added.
  GoogleString var_data_2 = CreateVariableDataResponse(true, false);
  const StringPiece var_data_piece_2(var_data_2);
  console->ParseVarDataIntoMap(var_data_piece_2, var_titles, &parsed_var_data);
  EXPECT_EQ(2, parsed_var_data["num_flushes"].size());
  EXPECT_EQ("300", parsed_var_data["num_flushes"][0]);
  EXPECT_EQ("310", parsed_var_data["num_flushes"][1]);
}

// Creates fake logfile data and tests that the data containing the histogram
// information is accurately parsed.
TEST_F(SharedMemStatisticsTestBase, TestParseHistData) {
  SharedMemConsoleStatisticsLogger* console =
      reinterpret_cast<SharedMemConsoleStatisticsLogger*>
      (stats_->console_logger());
  GoogleString hist_data =
      CreateHistogramDataResponse("Html Time us Histogram", true) +
      CreateHistogramDataResponse("Unused Histogram", true) +
      CreateHistogramDataResponse("Backend Fetch First Byte Latency Histogram",
                                  false) +
      CreateHistogramDataResponse("Rewrite Latency Histogram", true);

  std::set<GoogleString> hist_titles;
  hist_titles.insert("Html Time us Histogram");
  hist_titles.insert("random histogram name");
  hist_titles.insert("Pagespeed Resource Latency Histogram");
  hist_titles.insert("Backend Fetch First Byte Latency Histogram");
  StringPiece hist_data_piece(hist_data);
  SharedMemConsoleStatisticsLogger::HistMap parsed_hist_data =
      console->ParseHistDataIntoMap(hist_data_piece, hist_titles);

  // Test that unqueried/ignored histograms are not generated.
  EXPECT_NE(parsed_hist_data.end(),
            parsed_hist_data.find("Html Time us Histogram"));
  EXPECT_NE(
      parsed_hist_data.end(),
      parsed_hist_data.find("Backend Fetch First Byte Latency Histogram"));
  EXPECT_EQ(parsed_hist_data.end(), parsed_hist_data.find("Unused Histogram"));
  EXPECT_EQ(parsed_hist_data.end(),
            parsed_hist_data.find("Rewrite Latency Histogram"));
  EXPECT_EQ(parsed_hist_data.end(),
            parsed_hist_data.find("Pagespeed Resource Latency Histogram"));
  EXPECT_EQ(parsed_hist_data.end(),
            parsed_hist_data.find("random histogram name"));

  // Test that the first bar of the first histogram is generated correctly.
  SharedMemConsoleStatisticsLogger::HistInfo first_histogram =
      parsed_hist_data["Html Time us Histogram"];
  EXPECT_EQ(7, first_histogram.size());
  EXPECT_EQ("0.000000", first_histogram[0].first.first);
  EXPECT_EQ("5.000000", first_histogram[0].first.second);
  EXPECT_EQ("2.000000", first_histogram[0].second);

  // Test that the last bar of the first histogram is generated correctly.
  EXPECT_EQ("2000.000000", first_histogram[6].first.first);
  EXPECT_EQ("2005.000000", first_histogram[6].first.second);
  EXPECT_EQ("1.000000", first_histogram[6].second);

  // Test that the first bar of the last histogram is generated correctly.
  SharedMemConsoleStatisticsLogger::HistInfo last_histogram =
      parsed_hist_data["Backend Fetch First Byte Latency Histogram"];
  SharedMemConsoleStatisticsLogger::HistBarInfo first_bar_of_last_histogram =
      last_histogram[0];
  EXPECT_EQ(5, last_histogram.size());
  EXPECT_EQ("0.000000", first_bar_of_last_histogram.first.first);
  EXPECT_EQ("5.000000", first_bar_of_last_histogram.first.second);
  EXPECT_EQ("2.000000", first_bar_of_last_histogram.second);

  // Test that the last bar of the last histogram is generated correctly.
  SharedMemConsoleStatisticsLogger::HistBarInfo last_bar_of_last_histogram =
      parsed_hist_data["Backend Fetch First Byte Latency Histogram"][4];
  EXPECT_EQ("200.000000", last_bar_of_last_histogram.first.first);
  EXPECT_EQ("205.000000", last_bar_of_last_histogram.first.second);
  EXPECT_EQ("1.000000", last_bar_of_last_histogram.second);
}

// Takes fake logfile data and parses it. It then checks that PrintJSONResponse
// accurately outputs a valid JSON object given the parsed variable and
// histogram data.
TEST_F(SharedMemStatisticsTestBase, TestPrintJSONResponse) {
  SharedMemConsoleStatisticsLogger* console =
      reinterpret_cast<SharedMemConsoleStatisticsLogger*>
      (stats_->console_logger());
  GoogleString hist_data, var_data, var_data_2;
  std::set<GoogleString> hist_titles, var_titles;
  CreateFakeLogfile(&var_data, &hist_data, &var_titles, &hist_titles);

  StringPiece hist_data_piece(hist_data);
  SharedMemConsoleStatisticsLogger::HistMap parsed_hist_data =
      console->ParseHistDataIntoMap(hist_data_piece, hist_titles);

  SharedMemConsoleStatisticsLogger::VarMap parsed_var_data;
  console->ParseVarDataIntoMap(var_data, var_titles, &parsed_var_data);

  var_data_2 = CreateVariableDataResponse(false, false);
  console->ParseVarDataIntoMap(var_data_2, var_titles, &parsed_var_data);

  // Populate timestamp data.
  std::vector<int64> list_of_timestamps;
  int64 starting_timestamp = 1342567288580;
  for (int i = 0; i < 5; ++i) {
    list_of_timestamps.push_back(starting_timestamp + i*5);
  }
  GoogleString dump;
  StringWriter writer(&dump);
  console->PrintJSON(list_of_timestamps, parsed_var_data, parsed_hist_data,
                     &writer, &handler_);
  Json::Value complete_json;
  Json::Reader json_reader;
  EXPECT_TRUE(json_reader.parse(dump.c_str(), complete_json));
}


}  // namespace net_instaweb
