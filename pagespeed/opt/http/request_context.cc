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
      request_id_(0),
      options_set_(true),
      options_(options) {
  Init();
}

RequestContext::RequestContext(AbstractMutex* logging_mutex, Timer* timer)
    : log_record_(new LogRecord(logging_mutex)),
      // TODO(gee): Move ownership of mutex to TimingInfo.
      timing_info_(timer, logging_mutex),
      request_id_(0),
      options_set_(false),
      // Note: We use default here, just in case, even though we expect
      // set_options to be called
      options_(kDeprecatedDefaultHttpOptions) {
  Init();
}

RequestContext::RequestContext(const HttpOptions& options,
                               AbstractMutex* mutex,
                               Timer* timer,
                               AbstractLogRecord* log_record)
    : log_record_(log_record),
      // TODO(gee): Move ownership of mutex to TimingInfo.
      timing_info_(timer, mutex),
      options_set_(true),
      options_(options) {
  Init();
}

void RequestContext::Init() {
  using_http2_ = false;
  accepts_webp_ = false;
  accepts_gzip_ = false;
  frozen_ = false;
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

void RequestContext::SetAcceptsGzip(bool x) {
  if (x != accepts_gzip_) {
    // TODO(jmarantz): Rather than recalculating the RequestContext
    // bits multiple times and making sure they don't change,
    // calculate them once, e.g. before putting them into a
    // RewriteDriver.
    DCHECK(!frozen_);
    accepts_gzip_ = x;
  }
}

void RequestContext::SetAcceptsWebp(bool x) {
  if (x != accepts_webp_) {
    DCHECK(!frozen_);
    accepts_webp_ = x;
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

void RequestContext::SetHttp2SupportFromViaHeader(StringPiece header) {
  // The header (in it's combined form) is a comma-separated list of proxies,
  // with the later proxies closer to the end.
  // We only look at the first one, since that's the one the user talks to.

  // Strip leading whitespace. Not using ready-built methods for this since
  // they use HTML whitespace rather than HTTP whitespace.
  while (!header.empty() && (header[0] == ' ' || header[0] == '\t')) {
    header.remove_prefix(1);
  }

  size_t sep_pos = header.find_first_of(" \t");
  if (sep_pos != StringPiece::npos) {
    header = header.substr(0, sep_pos);
  }

  if (header == "2" || StringCaseEqual(header, "http/2")) {
    set_using_http2(true);
  }
}

}  // namespace net_instaweb
