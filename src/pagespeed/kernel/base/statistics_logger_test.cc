// Copyright 2013 Google Inc. All Rights Reserved.
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
// Author: bvb@google.com (Ben VanBerkum)
// Author: sligocki@google.com (Shawn Ligocki)

#include "pagespeed/kernel/base/statistics_logger.h"

#include <map>
#include <vector>

#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/json.h"
#include "pagespeed/kernel/base/mem_file_system.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

namespace {

const int64 kLoggingIntervalMs = 3 * Timer::kSecondMs;
const int64 kMaxLogfileSizeKb = 10;
const char kLogFile[] = "mod_pagespeed_stats.log";

}  // namespace

class StatisticsLoggerTest : public ::testing::Test {
 protected:
  typedef StatisticsLogger::VarMap VarMap;

  StatisticsLoggerTest()
      // This time is in the afternoon of 17 July 2012.
      : timer_(1342567288560ULL),
        thread_system_(Platform::CreateThreadSystem()),
        handler_(thread_system_->NewMutex()),
        file_system_(thread_system_.get(), &timer_),
        // Note: These unit tests don't need access to timestamp variable or
        // statistics. There are integration tests in
        // SharedMemStatisticsTestBase which test those interactions.
        logger_(kLoggingIntervalMs, kMaxLogfileSizeKb, kLogFile,
                NULL /* timestamp_var */, &handler_,
                NULL /* statistics */, &file_system_, &timer_) {}

  GoogleString CreateVariableDataResponse(bool has_unused_variable,
                                          bool first) {
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

  GoogleString CreateFakeLogfile(GoogleString* var_data,
                                 std::set<GoogleString>* var_titles) {
    // Populate variable data.
    *var_data = CreateVariableDataResponse(false, true);
    var_titles->insert("num_flushes");
    var_titles->insert("slurp_404_count");
    var_titles->insert("cache_hits");
    var_titles->insert("cache_misses");

    GoogleString logfile_input;
    StrAppend(&logfile_input, "timestamp: 1300000000005\n", *var_data);
    StrAppend(&logfile_input, "timestamp: 1300000000010\n", *var_data);
    StrAppend(&logfile_input, "timestamp: 1300000000015\n", *var_data);
    StrAppend(&logfile_input, "timestamp: 1300000000020\n", *var_data);
    return logfile_input;
  }

  void ParseDataFromReader(const std::set<GoogleString>& var_titles,
                           StatisticsLogfileReader* reader,
                           std::vector<int64>* list_of_timestamps,
                           VarMap* parsed_var_data) {
    logger_.ParseDataFromReader(var_titles, reader, list_of_timestamps,
                                parsed_var_data);
  }
  void ParseVarDataIntoMap(StringPiece logfile_var_data,
                           const std::set<GoogleString>& var_titles,
                           VarMap* parsed_var_data) {
    logger_.ParseVarDataIntoMap(logfile_var_data, var_titles, parsed_var_data);
  }
  void PrintJSON(const std::vector<int64>& list_of_timestamps,
                 const VarMap& parsed_var_data,
                 Writer* writer, MessageHandler* message_handler) {
    logger_.PrintJSON(list_of_timestamps, parsed_var_data, writer,
                      message_handler);
  }

  MockTimer timer_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockMessageHandler handler_;
  MemFileSystem file_system_;
  StatisticsLogger logger_;
};

TEST_F(StatisticsLoggerTest, TestParseDataFromReader) {
  GoogleString var_data;
  std::set<GoogleString> var_titles;
  GoogleString logfile_input = CreateFakeLogfile(&var_data, &var_titles);
  StringPiece logfile_input_piece(logfile_input);
  GoogleString file_name;
  bool success = file_system_.WriteTempFile("/prefix/", logfile_input_piece,
                                            &file_name, &handler_);
  EXPECT_TRUE(success);

  FileSystem::InputFile* log_file =
      file_system_.OpenInputFile(file_name.c_str(), &handler_);
  GoogleString output;
  int64 start_time = 1300000000000LL;
  int64 end_time = 1400000000000LL;
  int64 granularity_ms = 2;
  StatisticsLogfileReader reader(log_file, start_time, end_time,
                                 granularity_ms, &handler_);
  std::vector<int64> list_of_timestamps;
  VarMap parsed_var_data;
  ParseDataFromReader(var_titles, &reader,
                      &list_of_timestamps, &parsed_var_data);
  // Test that the entire logfile was parsed correctly.
  EXPECT_EQ(4, parsed_var_data.size());
  EXPECT_EQ(4, list_of_timestamps.size());

  file_system_.Close(log_file, &handler_);
}

// Creates fake logfile data and tests that ReadNextDataBlock accurately
// extracts data from logfile-formatted text.
TEST_F(StatisticsLoggerTest, TestNextDataBlock) {
  // Note: We no longer write or read histograms, but we must still be able
  // to parse around them in old logfiles, so add for coverage.
  GoogleString histogram_data =
      "histogram#Html Time us Histogram"
      "#0.000000#5.000000#2.000000"
      "#10.000000#15.000000#1.000000"
      "#20.000000#25.000000#1.000000"
      "#100.000000#105.000000#1.000000"
      "#200.000000#205.000000#1.000000"
      "#1000.000000#1005.000000#1.000000"
      "#2000.000000#2005.000000#1.000000\n";
  int64 start_time = 1300000000000LL;  // Randomly chosen times.
  int64 end_time = 1400000000000LL;
  int64 granularity_ms = 5;
  int64 initial_timestamp = 1342567288560LL;
  // Add two working cases.
  GoogleString input = "timestamp: " +
                       Integer64ToString(initial_timestamp) + "\n";
  // Test without histogram.
  const GoogleString first_var_data =  "num_flushes: 300\n";
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

  bool success = file_system_.WriteTempFile("/prefix/", input_piece, &file_name,
                                            &handler_);
  EXPECT_TRUE(success);

  FileSystem::InputFile* log_file =
      file_system_.OpenInputFile(file_name.c_str(), &handler_);
  GoogleString output;
  StatisticsLogfileReader reader(log_file, start_time, end_time,
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

  file_system_.Close(log_file, &handler_);
}

// Creates fake logfile data and tests that the data containing the variable
// timeseries information is accurately parsed.
TEST_F(StatisticsLoggerTest, TestParseVarData) {
  VarMap parsed_var_data;
  GoogleString var_data = CreateVariableDataResponse(true, true);
  const StringPiece var_data_piece(var_data);
  std::set<GoogleString> var_titles;
  var_titles.insert("num_flushes");
  var_titles.insert("slurp_404_count");
  var_titles.insert("not_a_variable");
  ParseVarDataIntoMap(var_data_piece, var_titles, &parsed_var_data);

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
  ParseVarDataIntoMap(var_data_piece_2, var_titles, &parsed_var_data);
  EXPECT_EQ(2, parsed_var_data["num_flushes"].size());
  EXPECT_EQ("300", parsed_var_data["num_flushes"][0]);
  EXPECT_EQ("310", parsed_var_data["num_flushes"][1]);
}

// Takes fake logfile data and parses it. It then checks that PrintJSONResponse
// accurately outputs a valid JSON object given the parsed variable data.
TEST_F(StatisticsLoggerTest, TestPrintJSONResponse) {
  GoogleString var_data, var_data_2;
  std::set<GoogleString> var_titles;
  CreateFakeLogfile(&var_data, &var_titles);

  VarMap parsed_var_data;
  ParseVarDataIntoMap(var_data, var_titles, &parsed_var_data);

  var_data_2 = CreateVariableDataResponse(false, false);
  ParseVarDataIntoMap(var_data_2, var_titles, &parsed_var_data);

  // Populate timestamp data.
  std::vector<int64> list_of_timestamps;
  int64 starting_timestamp = 1342567288580LL;
  for (int i = 0; i < 5; ++i) {
    list_of_timestamps.push_back(starting_timestamp + i*5);
  }
  GoogleString dump;
  StringWriter writer(&dump);
  PrintJSON(list_of_timestamps, parsed_var_data, &writer, &handler_);
  Json::Value complete_json;
  Json::Reader json_reader;
  EXPECT_TRUE(json_reader.parse(dump.c_str(), complete_json)) << dump;
}

}  // namespace net_instaweb
