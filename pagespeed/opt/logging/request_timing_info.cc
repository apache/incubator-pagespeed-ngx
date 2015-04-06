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

#include "pagespeed/opt/logging/request_timing_info.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/timer.h"

namespace net_instaweb {

namespace {

bool SetValueIfGEZero(int64 in, int64* out) {
  if (in < 0) {
    return false;
  }

  *out = in;
  return true;
}

}  // namespace


RequestTimingInfo::RequestTimingInfo(Timer* timer, AbstractMutex* mutex)
    : timer_(timer),
      init_ts_ms_(-1),
      start_ts_ms_(-1),
      processing_start_ts_ms_(-1),
      pcache_lookup_start_ts_ms_(-1),
      pcache_lookup_end_ts_ms_(-1),
      parsing_start_ts_ms_(-1),
      end_ts_ms_(-1),
      mu_(mutex),
      fetch_start_ts_ms_(-1),
      fetch_header_ts_ms_(-1),
      fetch_end_ts_ms_(-1),
      first_byte_ts_ms_(-1),
      http_cache_latency_ms_(-1),
      l2http_cache_latency_ms_(-1) {
  init_ts_ms_ = NowMs();
}

void RequestTimingInfo::RequestStarted() {
  SetToNow(&start_ts_ms_);
  VLOG(2) << "RequestStarted: " << start_ts_ms_;
}

void RequestTimingInfo::FirstByteReturned() {
  ScopedMutex l(mu_);
  SetToNow(&first_byte_ts_ms_);
}

void RequestTimingInfo::FetchStarted() {
  ScopedMutex l(mu_);
  if (fetch_start_ts_ms_ > 0) {
    // It's possible this is called more than once, just ignore subsequent
    // calls.
    return;
  }

  SetToNow(&fetch_start_ts_ms_);
}

void RequestTimingInfo::FetchHeaderReceived() {
  ScopedMutex l(mu_);
  SetToNow(&fetch_header_ts_ms_);
}

void RequestTimingInfo::FetchFinished() {
  ScopedMutex l(mu_);
  SetToNow(&fetch_end_ts_ms_);
}

void RequestTimingInfo::SetHTTPCacheLatencyMs(int64 latency_ms) {
  ScopedMutex l(mu_);
  SetValueIfGEZero(latency_ms, &http_cache_latency_ms_);
}

void RequestTimingInfo::SetL2HTTPCacheLatencyMs(int64 latency_ms) {
  ScopedMutex l(mu_);
  SetValueIfGEZero(latency_ms, &l2http_cache_latency_ms_);
}

int64 RequestTimingInfo::GetElapsedMs() const {
  DCHECK_GE(init_ts_ms_, 0);
  return NowMs() - init_ts_ms_;
}

bool RequestTimingInfo::GetProcessingElapsedMs(
    int64* processing_elapsed_ms) const {
  if (end_ts_ms_ < 0 || start_ts_ms_ < 0) {
    return false;
  }
  int64 elapsed_ms = end_ts_ms_ - start_ts_ms_;
  int64 fetch_elapsed_ms = 0;
  if (GetFetchLatencyMs(&fetch_elapsed_ms)) {
    elapsed_ms -= fetch_elapsed_ms;
  }

  *processing_elapsed_ms = elapsed_ms;
  return true;
}

bool RequestTimingInfo::GetTimeToStartFetchMs(
    int64* elapsed_ms) const {
  ScopedMutex l(mu_);
  return GetTimeFromStart(fetch_start_ts_ms_, elapsed_ms);
}

bool RequestTimingInfo::GetFetchHeaderLatencyMs(
    int64* elapsed_ms) const {
  ScopedMutex l(mu_);
  if (fetch_header_ts_ms_ < 0 || fetch_start_ts_ms_< 0) {
    return false;
  }

  const int64 tmp_elapsed_ms = fetch_header_ts_ms_ - fetch_start_ts_ms_;
  if (tmp_elapsed_ms < 0) {
    return false;
  }

  *elapsed_ms = tmp_elapsed_ms;
  return true;
}

bool RequestTimingInfo::GetFetchLatencyMs(int64* elapsed_ms) const {
  ScopedMutex l(mu_);
  if (fetch_end_ts_ms_ < 0 || fetch_start_ts_ms_ < 0) {
    return false;
  }

  *elapsed_ms = fetch_end_ts_ms_ - fetch_start_ts_ms_;
  return true;
}

bool RequestTimingInfo::GetTimeToFirstByte(int64* latency_ms) const {
  ScopedMutex l(mu_);
  if (first_byte_ts_ms_ < 0) {
    return false;
  }

  *latency_ms = first_byte_ts_ms_ - init_ts_ms_;
  return true;
}

int64 RequestTimingInfo::NowMs() const {
  if (timer_ == NULL) {
    return 0;
  }

  return timer_->NowMs();
}

void RequestTimingInfo::SetToNow(int64* ts) const {
  DCHECK_GE(*ts, -1);
  *ts = NowMs();
}

bool RequestTimingInfo::GetTimeFromStart(
    int64 ts_ms, int64* elapsed_ms) const {
  if (ts_ms < 0 || start_ts_ms_ < 0) {
    return false;
  }

  *elapsed_ms = ts_ms - start_ts_ms_;
  return true;
}

bool RequestTimingInfo::GetHTTPCacheLatencyMs(
    int64* latency_ms) const {
  ScopedMutex l(mu_);
  return SetValueIfGEZero(http_cache_latency_ms_, latency_ms);
}

bool RequestTimingInfo::GetL2HTTPCacheLatencyMs(
    int64* latency_ms) const {
  ScopedMutex l(mu_);
  return SetValueIfGEZero(l2http_cache_latency_ms_, latency_ms);
}

}  // namespace net_instaweb
