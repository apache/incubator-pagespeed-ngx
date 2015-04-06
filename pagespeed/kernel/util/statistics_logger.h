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

#ifndef PAGESPEED_KERNEL_BASE_STATISTICS_LOGGER_H_
#define PAGESPEED_KERNEL_BASE_STATISTICS_LOGGER_H_

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class MessageHandler;
class MutexedScalar;
class Statistics;
class StatisticsLogfileReader;
class Timer;
class UpDownCounter;
class Variable;
class Writer;

class StatisticsLogger {
 public:
  // Does not take ownership of any objects passed in.
  StatisticsLogger(
      int64 update_interval_ms, int64 max_logfile_size_kb,
      const StringPiece& log_file, MutexedScalar* last_dump_timestamp,
      MessageHandler* message_handler, Statistics* stats,
      FileSystem* file_system, Timer* timer);
  ~StatisticsLogger();

  // Writes filtered variable data in JSON format to the given writer.
  // Variable data is a time series collected from with data points from
  // start_time to end_time. Granularity is the minimum time difference
  // between each successive data point.
  void DumpJSON(bool dump_for_graphs, const StringSet& var_titles,
                int64 start_time, int64 end_time, int64 granularity_ms,
                Writer* writer, MessageHandler* message_handler) const;

  // If it's been longer than kStatisticsDumpIntervalMs, update the
  // timestamp to now and dump the current state of the Statistics.
  void UpdateAndDumpIfRequired();

  // Trim file down if it gets above max_logfile_size_kb.
  void TrimLogfileIfNeeded();

  // Preload all the variables required for statistics logging.  This
  // must be called after statistics have been established, and
  // before any logging is done.
  //
  // It is OK to call this multiple times (e.g. before & after a fork).
  void Init();

 private:
  friend class StatisticsLoggerTest;

  typedef std::vector<GoogleString> VariableInfo;
  typedef std::map<GoogleString, VariableInfo> VarMap;

  // Note that exactly one of these will be non-null; this is really
  // a union, but I'm too lazy to make the enum tag, and there's no
  // space advantage to doing so when there are only two choices.
  typedef std::pair<Variable*, UpDownCounter*> VariableOrCounter;
  typedef std::map<StringPiece, VariableOrCounter> VariableMap;

  // Export statistics to a writer. Only export stats needed for console.
  // current_time_ms: The time at which the dump was triggered.
  void DumpConsoleVarsToWriter(int64 current_time_ms, Writer* writer);
  // Save the variables listed in var_titles to the map.
  void ParseDataFromReader(const StringSet& var_titles,
                           StatisticsLogfileReader* reader,
                           std::vector<int64>* list_of_timestamps,
                           VarMap* parsed_var_data) const;
  // Save the variables needed by graphs page to the map.
  void ParseDataForGraphs(StatisticsLogfileReader* reader,
                          std::vector<int64>* list_of_timestamps,
                          VarMap* parsed_var_data) const;
  // Parse a string into a map of variable name -> value.
  // Note: parsed_var_data StringPieces point into logfile_var_data and thus
  // have same lifetime as it.
  void ParseVarDataIntoMap(StringPiece logfile_var_data,
                           std::map<StringPiece, StringPiece>* parsed_var_data)
      const;
  void PrintVarDataAsJSON(const VarMap& parsed_var_data, Writer* writer,
                          MessageHandler* message_handler) const;
  void PrintTimestampListAsJSON(const std::vector<int64>& list_of_timestamps,
                                Writer* writer,
                                MessageHandler* message_handler) const;
  void PrintJSON(const std::vector<int64>& list_of_timestamps,
                 const VarMap& parsed_var_data,
                 Writer* writer, MessageHandler* message_handler) const;
  void AddVariable(StringPiece var_name);

  // Initializes all stats that will be needed for logging. Only call this in
  // tests to make sure getting those stats will work.
  void InitStatsForTest();

  // The last_dump_timestamp not only contains the time of the last dump,
  // it also controls locking so that multiple threads can't dump at once.
  MutexedScalar* last_dump_timestamp_;
  MessageHandler* message_handler_;
  Statistics* statistics_;  // Needed so we can dump the stats contained here.
  // file_system_ and timer_ are owned by someone who called the constructor
  // (usually Apache's ServerContext).
  FileSystem* file_system_;
  Timer* timer_;    // Used to retrieve timestamps
  const int64 update_interval_ms_;
  const int64 max_logfile_size_kb_;
  GoogleString logfile_name_;
  VariableMap variables_to_log_;

  DISALLOW_COPY_AND_ASSIGN(StatisticsLogger);
};

// Handles reading the logfile created by StatisticsLogger.
class StatisticsLogfileReader {
 public:
  StatisticsLogfileReader(FileSystem::InputFile* file, int64 start_time,
                          int64 end_time, int64 granularity_ms,
                          MessageHandler* message_handler);
  ~StatisticsLogfileReader();

  // Reads the next timestamp in the file into timestamp and the corresponding
  // chunk of data into data. Returns true if new data has been read.
  // TODO(sligocki): Use a StringPiece* here to avoid extra copies. We need
  // to guarantee that the data pointed to by the StringPiece will be valid
  // for the right lifetime first.
  bool ReadNextDataBlock(int64* timestamp, GoogleString* data);
  int64 end_time() { return end_time_; }

 private:
  // TODO(sligocki): Use StringPiece here instead of char*.
  size_t BufferFind(const char* search_for, size_t start_at);
  int FeedBuffer();

  FileSystem::InputFile* file_;
  int64 start_time_;
  int64 end_time_;
  int64 granularity_ms_;
  MessageHandler* message_handler_;
  // Logfile buffer.
  GoogleString buffer_;

  DISALLOW_COPY_AND_ASSIGN(StatisticsLogfileReader);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_STATISTICS_LOGGER_H_
