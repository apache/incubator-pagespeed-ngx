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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_LOGGER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_LOGGER_H_

#include <map>
#include <set>
#include <vector>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/gtest_prod.h"  // for FRIEND_TEST

namespace net_instaweb {

class ConsoleStatisticsLogfileReader;
class FileSystem;
class MessageHandler;
class SharedMemVariable;
class Timer;
class Writer;

// TODO(sligocki): Break dependence on shared memory (only needed for
// SharedMemVariable::mutex()) and rename to StatisticsLogger, getting
// rid of unneeded abstract base class.
class SharedMemConsoleStatisticsLogger : public ConsoleStatisticsLogger {
 public:
  SharedMemConsoleStatisticsLogger(
      int64 update_interval_ms, int64 max_logfile_size_kb,
      const StringPiece& log_file, SharedMemVariable* last_dump_timestamp,
      MessageHandler* message_handler, Statistics* stats,
      FileSystem* file_system, Timer* timer);
  virtual ~SharedMemConsoleStatisticsLogger();

  // Writes filtered variable data in JSON format to the given writer.
  // Variable data is a time series collected from with data points from
  // start_time to end_time. Granularity is the minimum time difference
  // between each successive data point.
  virtual void DumpJSON(const std::set<GoogleString>& var_titles,
                        int64 start_time, int64 end_time, int64 granularity_ms,
                        Writer* writer, MessageHandler* message_handler) const;

  // If it's been longer than kStatisticsDumpIntervalMs, update the
  // timestamp to now and dump the current state of the Statistics.
  void UpdateAndDumpIfRequired();

  // Trim file down if it gets above max_logfile_size_kb.
  void TrimLogfileIfNeeded();

 private:
  friend class SharedMemStatisticsLoggerTest;
  FRIEND_TEST(SharedMemStatisticsLoggerTest, TestNextDataBlock);
  FRIEND_TEST(SharedMemStatisticsLoggerTest, TestParseVarData);
  FRIEND_TEST(SharedMemStatisticsLoggerTest, TestPrintJSONResponse);
  FRIEND_TEST(SharedMemStatisticsLoggerTest, TestParseDataFromReader);

  typedef std::vector<GoogleString> VariableInfo;
  typedef std::map<GoogleString, VariableInfo> VarMap;
  void ParseDataFromReader(const std::set<GoogleString>& var_titles,
                           ConsoleStatisticsLogfileReader* reader,
                           std::vector<int64>* list_of_timestamps,
                           VarMap* parsed_var_data) const;
  void ParseVarDataIntoMap(StringPiece logfile_var_data,
                           const std::set<GoogleString>& var_titles,
                           VarMap* parsed_var_data) const;
  void PrintVarDataAsJSON(const VarMap& parsed_var_data, Writer* writer,
                          MessageHandler* message_handler) const;
  void PrintTimestampListAsJSON(const std::vector<int64>& list_of_timestamps,
                                Writer* writer,
                                MessageHandler* message_handler) const;
  void PrintJSON(const std::vector<int64>& list_of_timestamps,
                 const VarMap& parsed_var_data,
                 Writer* writer, MessageHandler* message_handler) const;

  // The last_dump_timestamp not only contains the time of the last dump,
  // it also controls locking so that multiple threads can't dump at once.
  SharedMemVariable* last_dump_timestamp_;
  MessageHandler* message_handler_;
  Statistics* statistics_;  // Needed so we can dump the stats contained here.
  // file_system_ and timer_ are owned by someone who called the constructor
  // (usually Apache's ServerContext).
  FileSystem* file_system_;
  Timer* timer_;    // Used to retrieve timestamps
  const int64 update_interval_ms_;
  const int64 max_logfile_size_kb_;
  GoogleString logfile_name_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemConsoleStatisticsLogger);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_STATISTICS_LOGGER_H_
