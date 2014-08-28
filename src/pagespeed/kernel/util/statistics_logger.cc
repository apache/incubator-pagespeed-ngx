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

#include "pagespeed/kernel/util/statistics_logger.h"

#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <utility>                      // for pair
#include <vector>

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/escaping.h"
#include "pagespeed/kernel/base/file_writer.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/writer.h"
#include "pagespeed/kernel/html/html_keywords.h"

namespace net_instaweb {

namespace {

// Note that some of the stastistics named below are really
// UpDownCounters.  For now, we don't segregate them, but we just
// figure out at initialization time which is which.

// Variables used in /pagespeed_console. These will all be logged and
// are the default set of variables sent back in JSON requests.
const char* const kConsoleVars[] = {
  "serf_fetch_failure_count", "serf_fetch_request_count",
  "resource_url_domain_rejections", "resource_url_domain_acceptances",
  "num_cache_control_not_rewritable_resources",
  "num_cache_control_rewritable_resources",
  "cache_backend_misses", "cache_backend_hits", "cache_expirations",
  "css_filter_parse_failures", "css_filter_blocks_rewritten",
  "javascript_minification_failures", "javascript_blocks_minified",
  "image_rewrites", "image_rewrites_dropped_nosaving_resize",
  "image_rewrites_dropped_nosaving_noresize",
  "image_norewrites_high_resolution",
  "image_rewrites_dropped_decode_failure",
  "image_rewrites_dropped_server_write_fail",
  "image_rewrites_dropped_mime_type_unknown",
  "image_norewrites_high_resolution",
  "css_combine_opportunities", "css_file_count_reduction",
};

// Other variables we want to log.
const char* const kOtherLoggedVars[] = {
  // Variables used by /mod_pagespeed_temp_statistics_graphs
  // Note: It's fine that there are duplicates here.
  // TODO(sligocki): Remove this in favor of the /pagespeed_console vars.
  // Should we also add other stats for future/other use?
  "num_flushes", "cache_hits", "cache_misses",
  "num_fallback_responses_served", "slurp_404_count", "page_load_count",
  "total_page_load_ms", "num_rewrites_executed", "num_rewrites_dropped",
  "resource_404_count", "serf_fetch_request_count",
  "serf_fetch_bytes_count", "image_ongoing_rewrites",
  "javascript_total_bytes_saved", "css_filter_total_bytes_saved",
  "image_rewrite_total_bytes_saved", "image_norewrites_high_resolution",
  "image_rewrites_dropped_due_to_load", "image_rewrites_dropped_intentionally",
  "flatten_imports_charset_mismatch", "flatten_imports_invalid_url",
  "flatten_imports_limit_exceeded", "flatten_imports_minify_failed",
  "flatten_imports_recursion", "css_filter_parse_failures",
  "converted_meta_tags", "javascript_minification_failures",
};

const char* const kGraphsVars[] = {
  // Variables used in /pagespeed_admin/graphs.
  // Note: It's fine that some variables here are already listed above.
  "pcache-cohorts-dom_deletes", "pcache-cohorts-beacon_cohort_misses",
  "pcache-cohorts-dom_inserts", "pcache-cohorts-dom_misses",
  "pcache-cohorts-beacon_cohort_deletes", "pcache-cohorts-beacon_cohort_hits",
  "pcache-cohorts-beacon_cohort_inserts", "pcache-cohorts-dom_hits",
  "rewrite_cached_output_missed_deadline", "rewrite_cached_output_hits",
  "rewrite_cached_output_misses", "url_input_resource_hit",
  "url_input_resource_recent_fetch_failure", "serf_fetch_bytes_count",
  "url_input_resource_recent_uncacheable_miss",
  "url_input_resource_recent_uncacheable_failure",
  "url_input_resource_miss", "serf_fetch_request_count", "lru_cache_hits",
  "serf_fetch_time_duration_ms", "serf_fetch_cancel_count",
  "serf_fetch_timeout_count", "serf_fetch_failure_count", "http_bytes_fetched",
  "serf_fetch_active_count", "lru_cache_deletes", "serf_fetch_cert_errors",
  "lru_cache_inserts", "lru_cache_misses", "file_cache_bytes_freed_in_cleanup",
  "file_cache_cleanups", "file_cache_disk_checks", "file_cache_evictions",
  "file_cache_write_errors", "file_cache_deletes", "file_cache_hits",
  "file_cache_inserts", "file_cache_misses", "http_fetches",
  "http_approx_header_bytes_fetched", "image_rewrite_total_bytes_saved",
  "image_rewrite_total_original_bytes", "image_rewrite_uses",
  "image_rewrite_latency_total_ms", "image_rewrites_dropped_intentionally",
  "image_rewrites_dropped_decode_failure", "cache_misses", "ipro_not_in_cache",
  "image_rewrites_dropped_mime_type_unknown", "cache_fallbacks",
  "image_rewrites_dropped_server_write_fail", "cache_inserts",
  "image_rewrites_dropped_nosaving_resize", "cache_flush_timestamp_ms",
  "image_rewrites_dropped_nosaving_noresize", "ipro_served",
  "ipro_not_rewritable", "ipro_recorder_resources", "cache_deletes",
  "ipro_recorder_inserted_into_cache", "ipro_recorder_not_cacheable",
  "ipro_recorder_failed", "ipro_recorder_dropped_due_to_load",
  "ipro_recorder_dropped_due_to_size", "shm_cache_deletes", "shm_cache_hits",
  "shm_cache_inserts", "shm_cache_misses", "memcached_async_deletes",
  "memcached_async_hits", "memcached_async_inserts", "memcached_async_misses",
  "memcached_blocking_deletes", "memcached_blocking_hits", "cache_expirations",
  "memcached_blocking_inserts", "memcached_blocking_misses", "cache_time_us",
  "cache_hits", "cache_backend_hits", "cache_backend_misses",
  "cache_extensions", "cache_batcher_dropped_gets", "cache_flush_count",
};

}  // namespace

StatisticsLogger::StatisticsLogger(
    int64 update_interval_ms, int64 max_logfile_size_kb,
    const StringPiece& logfile_name, MutexedScalar* last_dump_timestamp,
    MessageHandler* message_handler, Statistics* stats,
    FileSystem* file_system, Timer* timer)
    : last_dump_timestamp_(last_dump_timestamp),
      message_handler_(message_handler),
      statistics_(stats),
      file_system_(file_system),
      timer_(timer),
      update_interval_ms_(update_interval_ms),
      max_logfile_size_kb_(max_logfile_size_kb) {
  logfile_name.CopyToString(&logfile_name_);
}

StatisticsLogger::~StatisticsLogger() {
}

void StatisticsLogger::Init() {
  variables_to_log_.clear();

  // List of statistics to log.
  for (int i = 0, n = arraysize(kConsoleVars); i < n; ++i) {
    AddVariable(kConsoleVars[i]);
  }
  for (int i = 0, n = arraysize(kOtherLoggedVars); i < n; ++i) {
    AddVariable(kOtherLoggedVars[i]);
  }
  for (int i = 0, n = arraysize(kGraphsVars); i < n; ++i) {
    AddVariable(kGraphsVars[i]);
  }
}

void StatisticsLogger::InitStatsForTest() {
  // List of statistics to log.
  for (int i = 0, n = arraysize(kConsoleVars); i < n; ++i) {
    statistics_->AddVariable(kConsoleVars[i]);
  }
  for (int i = 0, n = arraysize(kOtherLoggedVars); i < n; ++i) {
    statistics_->AddVariable(kOtherLoggedVars[i]);
  }
  for (int i = 0, n = arraysize(kGraphsVars); i < n; ++i) {
    statistics_->AddVariable(kGraphsVars[i]);
  }
  Init();
}

void StatisticsLogger::AddVariable(StringPiece var_name) {
  VariableOrCounter var_or_counter;
  var_or_counter.first = statistics_->FindVariable(var_name);
  if (var_or_counter.first == NULL) {
    var_or_counter.second = statistics_->GetUpDownCounter(var_name);
  }
  variables_to_log_[var_name] = var_or_counter;
}

void StatisticsLogger::UpdateAndDumpIfRequired() {
  int64 current_time_ms = timer_->NowMs();
  AbstractMutex* mutex = last_dump_timestamp_->mutex();
  if (mutex == NULL) {
    return;
  }
  // Avoid blocking if the dump is already happening in another thread/process.
  if (mutex->TryLock()) {
    if (current_time_ms >=
        (last_dump_timestamp_->GetLockHeld() + update_interval_ms_)) {
      // It's possible we'll need to do some of the following here for
      // cross-process consistency:
      // - flush the logfile before unlock to force out buffered data
      FileSystem::OutputFile* statistics_log_file =
          file_system_->OpenOutputFileForAppend(
              logfile_name_.c_str(), message_handler_);
      if (statistics_log_file != NULL) {
        FileWriter statistics_writer(statistics_log_file);
        DumpConsoleVarsToWriter(current_time_ms, &statistics_writer);
        statistics_writer.Flush(message_handler_);
        file_system_->Close(statistics_log_file, message_handler_);

        // Trim logfile if it's over max size.
        TrimLogfileIfNeeded();
      } else {
        message_handler_->Message(kError,
                                  "Error opening statistics log file %s.",
                                  logfile_name_.c_str());
      }
      // Update timestamp regardless of file write so we don't hit the same
      // error many times in a row.
      last_dump_timestamp_->SetLockHeld(current_time_ms);
    }
    mutex->Unlock();
  }
}

void StatisticsLogger::DumpConsoleVarsToWriter(
    int64 current_time_ms, Writer* writer) {
  writer->Write(StringPrintf("timestamp: %s\n",
      Integer64ToString(current_time_ms).c_str()), message_handler_);

  for (VariableMap::const_iterator iter = variables_to_log_.begin();
       iter != variables_to_log_.end(); ++iter) {
    StringPiece var_name = iter->first;
    VariableOrCounter var_or_counter = iter->second;
    int64 val = (var_or_counter.first != NULL) ? var_or_counter.first->Get()
        : var_or_counter.second->Get();
    writer->Write(StrCat(var_name, ": ", Integer64ToString(val), "\n"),
                  message_handler_);
  }

  writer->Flush(message_handler_);
}

void StatisticsLogger::TrimLogfileIfNeeded() {
  int64 size_bytes;
  if (file_system_->Size(logfile_name_, &size_bytes, message_handler_) &&
      size_bytes > max_logfile_size_kb_ * 1024) {
    // TODO(sligocki): Rotate a set of logfiles and just delete the
    // oldest one each time instead of deleting the entire log.
    // NOTE: Update SharedMemStatisticsTestBase.TestLogfileTrimming when we
    // make this change so that it correctly tests the total log size.
    file_system_->RemoveFile(logfile_name_.c_str(), message_handler_);
  }
}

void StatisticsLogger::DumpJSON(
    bool dump_for_graphs, const StringSet& var_titles,
    int64 start_time, int64 end_time, int64 granularity_ms,
    Writer* writer, MessageHandler* message_handler) const {
  FileSystem::InputFile* log_file =
      file_system_->OpenInputFile(logfile_name_.c_str(), message_handler);
  if (log_file == NULL) {
    // If logfile_name_ represents a file that doesn't exist, OpenInputFile
    // logged an error and log_file will be null.  Return an empty json object.
    writer->Write("{}", message_handler);
    return;
  }
  VarMap parsed_var_data;
  std::vector<int64> list_of_timestamps;
  StatisticsLogfileReader reader(log_file, start_time, end_time,
                                 granularity_ms, message_handler);
  if (dump_for_graphs) {
    ParseDataForGraphs(&reader, &list_of_timestamps, &parsed_var_data);
  } else {
    ParseDataFromReader(var_titles, &reader, &list_of_timestamps,
                        &parsed_var_data);
  }
  PrintJSON(list_of_timestamps, parsed_var_data, writer, message_handler);
  file_system_->Close(log_file, message_handler);
}

void StatisticsLogger::ParseDataFromReader(
    const StringSet& var_titles,
    StatisticsLogfileReader* reader,
    std::vector<int64>* timestamps, VarMap* var_values) const {
  // curr_timestamp starts as 0 because we need to compare it to the first
  // timestamp pulled from the file. The current timestamp should always be
  // less than the timestamp after it due to the fact that the logfile dumps
  // data periodically. This makes sure that all timestamps are >= 0.
  int64 curr_timestamp = 0;
  // Stores data associated with current timestamp.
  GoogleString data;
  while (reader->ReadNextDataBlock(&curr_timestamp, &data)) {
    // Parse variable data.
    std::map<StringPiece, StringPiece> parsed_var_data;
    ParseVarDataIntoMap(data, &parsed_var_data);

    timestamps->push_back(curr_timestamp);
    // Push all variable values. Note: We only save the variables listed in
    // var_titles, the rest are disregarded.
    for (StringSet::const_iterator iter = var_titles.begin();
         iter != var_titles.end(); ++iter) {
      const GoogleString& var_title = *iter;

      std::map<StringPiece, StringPiece>::const_iterator value_iter =
          parsed_var_data.find(var_title);
      if (value_iter != parsed_var_data.end()) {
        (*var_values)[var_title].push_back(value_iter->second.as_string());
      } else {
        // If data is not available in this segment, we just push 0 as a place
        // holder. We must push something or else it will be ambiguous which
        // timestamp corresponds to which variable values.
        (*var_values)[var_title].push_back("0");
      }
    }
  }
}

void StatisticsLogger::ParseDataForGraphs(StatisticsLogfileReader* reader,
                                          std::vector<int64>* timestamps,
                                          VarMap* var_values) const {
  int64 curr_timestamp = 0;
  GoogleString data;
  while (reader->ReadNextDataBlock(&curr_timestamp, &data)) {
    std::map<StringPiece, StringPiece> parsed_var_data;
    ParseVarDataIntoMap(data, &parsed_var_data);
    timestamps->push_back(curr_timestamp);
    for (int i = 0, n = arraysize(kGraphsVars); i < n; ++i) {
      const GoogleString& var_title = kGraphsVars[i];
      std::map<StringPiece, StringPiece>::const_iterator value_iter =
          parsed_var_data.find(var_title);
      if (value_iter != parsed_var_data.end()) {
        value_iter->second.CopyToString(
            StringVectorAdd(&((*var_values)[var_title])));
      } else {
        (*var_values)[var_title].push_back("0");
      }
    }
  }
}

void StatisticsLogger::ParseVarDataIntoMap(
    StringPiece logfile_var_data,
    std::map<StringPiece, StringPiece>* parsed_var_data) const {
  std::vector<StringPiece> lines;
  SplitStringPieceToVector(logfile_var_data, "\n", &lines, true);
  for (size_t i = 0; i < lines.size(); ++i) {
    size_t end_index_of_name = lines[i].find_first_of(":");
    StringPiece var_name = lines[i].substr(0, end_index_of_name);
    StringPiece var_value_string = lines[i].substr(end_index_of_name + 2);
    (*parsed_var_data)[var_name] = var_value_string;
  }
}

void StatisticsLogger::PrintJSON(
    const std::vector<int64>& list_of_timestamps,
    const VarMap& parsed_var_data,
    Writer* writer, MessageHandler* message_handler) const {
  writer->Write("{", message_handler);
  writer->Write("\"timestamps\": [", message_handler);
  PrintTimestampListAsJSON(list_of_timestamps, writer, message_handler);
  writer->Write("],", message_handler);
  writer->Write("\"variables\": {", message_handler);
  PrintVarDataAsJSON(parsed_var_data, writer, message_handler);
  writer->Write("}", message_handler);
  writer->Write("}", message_handler);
}

void StatisticsLogger::PrintTimestampListAsJSON(
    const std::vector<int64>& list_of_timestamps, Writer* writer,
    MessageHandler* message_handler) const {
  for (size_t i = 0; i < list_of_timestamps.size(); ++i) {
    writer->Write(Integer64ToString(list_of_timestamps[i]), message_handler);
    if (i != list_of_timestamps.size() - 1) {
      writer->Write(", ", message_handler);
    }
  }
}

void StatisticsLogger::PrintVarDataAsJSON(
    const VarMap& parsed_var_data, Writer* writer,
    MessageHandler* message_handler) const {
  for (VarMap::const_iterator iterator =
      parsed_var_data.begin(); iterator != parsed_var_data.end();
      ++iterator) {
    StringPiece var_name = iterator->first;
    VariableInfo info = iterator->second;
    // If we are at the last entry in the map, we do not
    // want the trailing comma as per JSON format.
    if (iterator != parsed_var_data.begin()) {
      writer->Write(",", message_handler);
    }
    GoogleString html_name, json_name;
    HtmlKeywords::Escape(var_name, &html_name);
    EscapeToJsStringLiteral(html_name, true /* add_quotes*/, &json_name);

    writer->Write(json_name, message_handler);
    writer->Write(": [", message_handler);
    for (size_t i = 0; i < info.size(); ++i) {
      writer->Write(info[i], message_handler);
      // If we are at the last recorded variable, we do not want the
      // trailing comma as per JSON format.
      if (i != info.size() - 1) {
        writer->Write(", ", message_handler);
      }
    }
    writer->Write("]", message_handler);
  }
}

StatisticsLogfileReader::StatisticsLogfileReader(
    FileSystem::InputFile* file, int64 start_time, int64 end_time,
    int64 granularity_ms, MessageHandler* message_handler)
        : file_(file),
          start_time_(start_time),
          end_time_(end_time),
          granularity_ms_(granularity_ms),
          message_handler_(message_handler) {
}

StatisticsLogfileReader::~StatisticsLogfileReader() {
}

bool StatisticsLogfileReader::ReadNextDataBlock(int64* timestamp,
                                                GoogleString* data) {
  if (buffer_.size() < 1) {
    FeedBuffer();
  }
  size_t offset = 0;
  // The first line should always be "timestamp: xxx".
  // If it's not, we're done; otherwise, we grab the timestamp value.
  while (StringPiece(buffer_).substr(offset).starts_with("timestamp: ")) {
    int64 old_timestamp = *timestamp;
    // If the timestamp was cut off in the middle of the buffer, we need to
    // read more into the buffer.
    size_t newline_pos = BufferFind("\n", offset);
    // Separate the current timestamp from the rest of the data in the buffer.
    size_t timestamp_size = STATIC_STRLEN("timestamp: ");
    StringPiece timestamp_str = StringPiece(buffer_).substr(
        offset + timestamp_size, newline_pos - timestamp_size);
    StringToInt64(timestamp_str, timestamp);
    // Before ReadNextDataBlock returns, it finds the next occurrence of the
    // full string "timestamp: " so that it knows it contains the full data
    // block. It then separates the data block from the rest of the data,
    // meaning that "timestamp:  " should always be the first thing in the
    // buffer.
    size_t next_timestamp_pos = BufferFind("timestamp: ",
                                           offset + newline_pos + 1);
    // Check to make sure that this timestamp fits the criteria. If it doesn't,
    // the loop restarts.
    if (*timestamp >= start_time_ && *timestamp <= end_time_ &&
        *timestamp >= old_timestamp + granularity_ms_) {
      *data = buffer_.substr(newline_pos + 1,
                             next_timestamp_pos - (newline_pos + 1));
      buffer_.erase(0, next_timestamp_pos);
      return true;
    } else {
      *timestamp = old_timestamp;
      offset = next_timestamp_pos;
    }
  }
  return false;
}

// Finds the string constant search_for in the buffer, continually reading more
// into the buffer if search_for is not found and trying again. Returns the
// position of first occurrence of search_for. If search_for is not found,
// npos is returned.
size_t StatisticsLogfileReader::BufferFind(const char* search_for,
                                           size_t start_at) {
  size_t position = buffer_.find(search_for, start_at);
  while (position == buffer_.npos) {
    int read = FeedBuffer();
    if (read == 0) {
      return buffer_.npos;
    }
    position = buffer_.find(search_for,
                            buffer_.size() - read - strlen(search_for));
  }
  return position;
}

int StatisticsLogfileReader::FeedBuffer() {
  const int kChunkSize = 3000;
  char buf[kChunkSize];
  int num_read = file_->Read(buf, kChunkSize, message_handler_);
  StrAppend(&buffer_, StringPiece(buf, num_read));
  return num_read;
}

}  // namespace net_instaweb
