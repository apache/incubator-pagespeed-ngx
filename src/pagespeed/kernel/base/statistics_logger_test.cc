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
      : timer_(MockTimer::kApr_5_2010_ms),
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

  void CreateFakeLogfile(std::set<GoogleString>* var_titles, int64* start_time,
                         int64* end_time, int64* granularity_ms) {
    // Populate variable data.
    var_titles->insert("num_flushes");
    var_titles->insert("slurp_404_count");
    var_titles->insert("cache_hits");
    var_titles->insert("cache_misses");

    *start_time = MockTimer::kApr_5_2010_ms;
    *granularity_ms = kLoggingIntervalMs;
    *end_time = *start_time + 4 * (*granularity_ms);

    GoogleString var_data = CreateVariableDataResponse(false, true);

    GoogleString log;
    for (int64 time = *start_time; time < *end_time; time += *granularity_ms) {
      StrAppend(&log, "timestamp: ", Integer64ToString(time), "\n", var_data);
    }
    file_system_.WriteFile(kLogFile, log, &handler_);
  }

  // Methods defined here to get around private access restrictions.
  void ParseDataFromReader(const std::set<GoogleString>& var_titles,
                           StatisticsLogfileReader* reader,
                           std::vector<int64>* list_of_timestamps,
                           VarMap* parsed_var_data) {
    logger_.ParseDataFromReader(var_titles, reader, list_of_timestamps,
                                parsed_var_data);
  }
  void ParseVarDataIntoMap(
      StringPiece logfile_var_data,
      std::map<StringPiece, StringPiece>* parsed_var_data) {
    logger_.ParseVarDataIntoMap(logfile_var_data, parsed_var_data);
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
  std::set<GoogleString> var_titles;
  int64 start_time, end_time, granularity_ms;
  CreateFakeLogfile(&var_titles, &start_time, &end_time, &granularity_ms);

  FileSystem::InputFile* log_file =
      file_system_.OpenInputFile(kLogFile, &handler_);
  GoogleString output;
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
  int64 initial_timestamp = MockTimer::kApr_5_2010_ms;
  int64 start_time = initial_timestamp - Timer::kDayMs;
  int64 end_time = initial_timestamp + Timer::kDayMs;
  int64 granularity_ms = 5;
  GoogleString input;
  // Add two working cases.
  // Test without histogram.
  GoogleString first_var_data = "num_flushes: 300\n";
  StrAppend(&input,
            "timestamp: ", Integer64ToString(initial_timestamp), "\n",
            first_var_data);
  // Test with histogram.
  GoogleString second_var_data = StrCat("num_flushes: 305\n", histogram_data);
  StrAppend(&input,
            "timestamp: ", Integer64ToString(initial_timestamp + 20), "\n",
            second_var_data);

  // Add case that purposefully fails granularity requirements (The difference
  // between this timestamp and the previous one is only 2ms, whereas the
  // desired granularity is 5ms).
  const GoogleString third_var_data =
      StrCat("num_flushes: 310\n", histogram_data);
  StrAppend(&input,
            "timestamp: ", Integer64ToString(initial_timestamp + 22), "\n",
            third_var_data);

  // Add case that purposefully fails start_time requirements.
  StrAppend(&input,
            "timestamp: ", Integer64ToString(start_time - Timer::kDayMs), "\n",
            third_var_data);
  // Add case that purposefully fails end_time requirements.
  StrAppend(&input,
            "timestamp: ", Integer64ToString(end_time + Timer::kDayMs), "\n",
            third_var_data);
  // Add working case to make sure data output continues despite previous
  // requirements failing.
  StrAppend(&input,
            "timestamp: ", Integer64ToString(initial_timestamp + 50), "\n",
            third_var_data);
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
  std::map<StringPiece, StringPiece> parsed_var_data;
  GoogleString var_data = CreateVariableDataResponse(true, true);

  ParseVarDataIntoMap(var_data, &parsed_var_data);

  // All 5 variables get set in parsed_var_data.
  EXPECT_EQ(5, parsed_var_data.size());
  EXPECT_NE(parsed_var_data.end(), parsed_var_data.find("num_flushes"));
  EXPECT_NE(parsed_var_data.end(), parsed_var_data.find("cache_hits"));
  EXPECT_NE(parsed_var_data.end(), parsed_var_data.find("cache_misses"));
  EXPECT_NE(parsed_var_data.end(), parsed_var_data.find("slurp_404_count"));
  // Including random_unused_var, which we won't care about.
  EXPECT_NE(parsed_var_data.end(), parsed_var_data.find("random_unused_var"));

  // Variables not in the log do not get added.
  EXPECT_EQ(parsed_var_data.end(), parsed_var_data.find("not_a_variable"));

  // Test that map correctly adds data on initial run.
  EXPECT_EQ("300", parsed_var_data["num_flushes"]);

  // Test that map is updated correctly when new data is added.
  GoogleString var_data_2 = CreateVariableDataResponse(true, false);
  parsed_var_data.clear();
  ParseVarDataIntoMap(var_data_2, &parsed_var_data);
  EXPECT_EQ("310", parsed_var_data["num_flushes"]);
}

// Using fake logfile, make sure JSON output is not malformed.
TEST_F(StatisticsLoggerTest, NoMalformedJson) {
  std::set<GoogleString> var_titles;
  int64 start_time, end_time, granularity_ms;
  CreateFakeLogfile(&var_titles, &start_time, &end_time, &granularity_ms);

  GoogleString json_dump;
  StringWriter writer(&json_dump);
  logger_.DumpJSON(var_titles, start_time, end_time, granularity_ms,
                   &writer, &handler_);

  Json::Value complete_json;
  Json::Reader json_reader;
  EXPECT_TRUE(json_reader.parse(json_dump.c_str(), complete_json)) << json_dump;
}

// Make sure we return sensible results when there is data missing from log.
// This is not just to deal with data corruption, but any time the set of
// logged variables changes.
TEST_F(StatisticsLoggerTest, ConsistentNumberArgs) {
  // foo and bar only recorded at certain timestamps.
  file_system_.WriteFile(kLogFile,
                         "timestamp: 1000\n"
                         "timestamp: 2000\n"
                         "foo: 2\n"
                         "bar: 20\n"
                         "timestamp: 3000\n"
                         "bar: 30\n"
                         "timestamp: 4000\n"
                         "foo: 4\n",
                         &handler_);

  GoogleString json_dump;
  StringWriter writer(&json_dump);

  std::set<GoogleString> var_titles;
  var_titles.insert("foo");
  var_titles.insert("bar");
  logger_.DumpJSON(var_titles, 1000, 4000, 1000, &writer, &handler_);

  // The notable check here is that all the arrays are the same length.
  EXPECT_EQ("{\"timestamps\": [1000, 2000, 3000, 4000],\"variables\": {"
            "\"bar\": [0, 20, 30, 0],"
            "\"foo\": [0, 2, 0, 4]}}", json_dump);
}

}  // namespace net_instaweb
