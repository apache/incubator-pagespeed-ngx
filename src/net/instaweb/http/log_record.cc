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

#include <set>

#include "base/logging.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

LogRecord::LogRecord(AbstractMutex* mutex) : finalized_(false), mutex_(mutex) {
  InitLogging();
}

// Non-initializing constructor for subclasses to invoke.
LogRecord::LogRecord()
    : logging_info_(NULL),
      finalized_(false),
      mutex_(NULL) {
}

void LogRecord::InitLogging() {
  logging_info_.reset(new LoggingInfo);
}

LogRecord::~LogRecord() {
  mutex_->DCheckUnlocked();
  // Please do not add non-diagnostic functionality here.
  //
  // LogRecords are typically owned by reference counted objects, and
  // doing work in the dtor will result in actions being taken at
  // unpredictable times, leading to difficult to diagnose performance
  // and correctness bugs.
}

void LogRecord::set_mutex(AbstractMutex* m) {
  CHECK(mutex_.get() == NULL);
  mutex_.reset(m);
}

LoggingInfo* LogRecord::logging_info() {
  return logging_info_.get();
}

void LogRecord::LogAppliedRewriter(const char* rewriter_id) {
  ScopedMutex lock(mutex_.get());
  LogAppliedRewriterImpl(rewriter_id);
}

void LogRecord::LogAppliedRewriterImpl(const char* rewriter_id) {
  mutex_->DCheckLocked();
  if (!finalized()) {
    applied_rewriters_.insert(rewriter_id);
  }
}

void LogRecord::Finalize() {
  ScopedMutex lock(mutex_.get());
  FinalizeImpl();
}

void LogRecord::FinalizeImpl() {
  mutex_->DCheckLocked();
  if (!finalized()) {
    logging_info()->set_applied_rewriters(ConcatenatedRewriterString());
    finalized_ = true;
  }
}

void LogRecord::SetBlinkRequestFlow(int flow) {
  ScopedMutex lock(mutex_.get());
  logging_info()->mutable_blink_info()->set_blink_request_flow(
      static_cast<BlinkInfo::BlinkRequestFlow>(flow));
}

void LogRecord::SetIsOriginalResourceCacheable(bool cacheable) {
  ScopedMutex lock(mutex_.get());
  logging_info()->set_is_original_resource_cacheable(cacheable);
}

void LogRecord::SetTimingRequestStartMs(int64 ms) {
  ScopedMutex lock(mutex_.get());
  logging_info()->mutable_timing_info()->set_request_start_ms(ms);
}

void LogRecord::SetTimingFetchMs(int64 ms) {
  ScopedMutex lock(mutex_.get());
  logging_info()->mutable_timing_info()->set_fetch_ms(ms);
}

void LogRecord::SetBlinkInfo(const GoogleString& user_agent) {
  ScopedMutex lock(mutex_.get());
  SetBlinkInfoImpl(user_agent);
}

bool LogRecord::WriteLog() {
  ScopedMutex lock(mutex_.get());
  return WriteLogImpl();
}

GoogleString LogRecord::ConcatenatedRewriterString() {
  GoogleString rewriters_str;
  StringSet::iterator iter;
  for (iter = applied_rewriters_.begin(); iter != applied_rewriters_.end();
      ++iter) {
    if (iter != applied_rewriters_.begin()) {
      StrAppend(&rewriters_str, ",");
    }
    StrAppend(&rewriters_str, *iter);
  }
  return rewriters_str;
}


}  // namespace net_instaweb
