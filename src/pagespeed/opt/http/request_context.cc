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

// Author: piatek@google.com (Michael Piatek)

#include "pagespeed/opt/http/request_context.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/request_trace.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/http/http_options.h"
#include "pagespeed/opt/logging/log_record.h"

namespace net_instaweb {

// TODO(gee): Deprecate this.
RequestContext::RequestContext(const HttpOptions& options,
                               AbstractMutex* logging_mutex, Timer* timer)
    : log_record_(new LogRecord(logging_mutex)),
      // TODO(gee): Move ownership of mutex to TimingInfo.
      timing_info_(timer, logging_mutex),
      using_spdy_(false),
      accepts_webp_(false),
      split_request_type_(SPLIT_FULL),
      request_id_(0),
      options_set_(true),
      options_(options) {
}

RequestContext::RequestContext(AbstractMutex* logging_mutex, Timer* timer)
    : log_record_(new LogRecord(logging_mutex)),
      // TODO(gee): Move ownership of mutex to TimingInfo.
      timing_info_(timer, logging_mutex),
      using_spdy_(false),
      accepts_webp_(false),
      split_request_type_(SPLIT_FULL),
      request_id_(0),
      options_set_(false),
      // Note: We use default here, just in case, even though we expect
      // set_options to be called
      options_(kDeprecatedDefaultHttpOptions) {
}

RequestContext::RequestContext(const HttpOptions& options,
                               AbstractMutex* mutex,
                               Timer* timer,
                               AbstractLogRecord* log_record)
    : log_record_(log_record),
      // TODO(gee): Move ownership of mutex to TimingInfo.
      timing_info_(timer, mutex),
      using_spdy_(false),
      accepts_webp_(false),
      split_request_type_(SPLIT_FULL),
      options_set_(true),
      options_(options) {
}

RequestContext::~RequestContext() {
  // Please do not add non-diagnostic functionality here.
  //
  // RequestContexts are reference counted, and doing work in the dtor will
  // result in actions being taken at unpredictable times, leading to difficult
  // to diagnose performance and correctness bugs.
}

RequestContextPtr RequestContext::NewTestRequestContextWithTimer(
    ThreadSystem* thread_system, Timer* timer) {
  return RequestContextPtr(
      new RequestContext(kDefaultHttpOptionsForTests,
                         thread_system->NewMutex(), timer));
}

RequestContextPtr RequestContext::NewTestRequestContext(
    AbstractLogRecord* log_record) {
  return RequestContextPtr(
      new RequestContext(kDefaultHttpOptionsForTests,
                         log_record->mutex(), NULL, log_record));
}

AbstractLogRecord* RequestContext::NewSubordinateLogRecord(
    AbstractMutex* logging_mutex) {
  return new LogRecord(logging_mutex);
}

void RequestContext::set_root_trace_context(RequestTrace* x) {
  root_trace_context_.reset(x);
}

AbstractLogRecord* RequestContext::log_record() {
  DCHECK(log_record_.get() != NULL);
  return log_record_.get();
}

void RequestContext::PrepareLogRecordForOutput() {
  log_record()->SetTimingInfo(timing_info_);
}

void RequestContext::WriteBackgroundRewriteLog() {
  if (background_rewrite_log_record_.get() != NULL) {
    background_rewrite_log_record_->WriteLog();
  }
}

AbstractLogRecord* RequestContext::GetBackgroundRewriteLog(
    ThreadSystem* thread_system,
    bool log_urls,
    bool log_url_indices,
    int max_rewrite_info_log_size) {
  // The mutex of the main log record is purposefully used to synchronize the
  // creation of background log record.
  ScopedMutex lock(log_record()->mutex());
  AbstractLogRecord* log_record = background_rewrite_log_record_.get();
  if (log_record == NULL) {
    // We need to create a new log record.
    log_record = NewSubordinateLogRecord(thread_system->NewMutex());
    log_record->SetBackgroundRewriteInfo(log_urls, log_url_indices,
         max_rewrite_info_log_size);
    background_rewrite_log_record_.reset(log_record);
  }
  return log_record;
}

void RequestContext::ReleaseDependentTraceContext(RequestTrace* t) {
  if (t != NULL) {
    delete t;
  }
}

}  // namespace net_instaweb
