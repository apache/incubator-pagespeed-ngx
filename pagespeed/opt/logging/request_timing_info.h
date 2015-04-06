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

#ifndef PAGESPEED_OPT_LOGGING_REQUEST_TIMING_INFO_H_
#define PAGESPEED_OPT_LOGGING_REQUEST_TIMING_INFO_H_

#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

class AbstractMutex;
class Timer;

// RequestTimingInfo tracks various event timestamps over the lifetime of a
// request. The timeline looks (roughly) like the following, with the associated
// RequestTimingInfo calls.
// - Request Received/Context created: Init
// <queueing delay>
// - Trigger: RequestStarted
// <option lookup>
// - Start Processing: ProcessingStarted
// - Lookup Properties?: PropertyCacheLookup*
// - Fetch?: Fetch*
// - Start parsing?: ParsingStarted
// - First byte sent to client: FirstByteReturned.
// - Finish: RequestFinished
// NOTE: This class is thread safe.
class RequestTimingInfo {
 public:
  // Initialize the TimingInfo with the specified Timer.  Sets init_ts_ to
  // Timer::NowMs, from which GetElapsedMs is based.
  // NOTE: Timer and mutex are not owned by TimingInfo.
  RequestTimingInfo(Timer* timer, AbstractMutex* mutex);

  // This should be called when the request "starts", potentially after
  // queuing. It denotes the request "start time", which "elapsed" timing
  // values are relative to.
  void RequestStarted();

  // This should be called once the options are available and PSOL can start
  // doing meaningful work.
  void ProcessingStarted() { SetToNow(&processing_start_ts_ms_); }

  // This should be called if/when HTML parsing begins.
  void ParsingStarted() { SetToNow(&parsing_start_ts_ms_); }

  // Called when the first byte is sent back to the user.
  void FirstByteReturned();

  // This should be called when a PropertyCache lookup is initiated.
  void PropertyCacheLookupStarted() {
    SetToNow(&pcache_lookup_start_ts_ms_);
  }

  // This should be called when a PropertyCache lookup completes.
  void PropertyCacheLookupFinished() { SetToNow(&pcache_lookup_end_ts_ms_); }

  // Called when the request is finished, i.e. the response has been sent to
  // the client.
  void RequestFinished() { SetToNow(&end_ts_ms_); }

  // Fetch related timing events.
  // Note:  Only the first call to FetchStarted will have an effect,
  // subsequent calls are silent no-ops.
  // TODO(gee): Fetch and cache timing is busted for reconstructing resources
  // with multiple inputs.
  void FetchStarted();
  void FetchHeaderReceived();
  void FetchFinished();

  // TODO(gee): I'd really prefer these to be start/end calls, but the
  // WriteThroughCache design pattern will not allow for this.
  void SetHTTPCacheLatencyMs(int64 latency_ms);
  void SetL2HTTPCacheLatencyMs(int64 latency_ms);

  // Milliseconds since Init.
  int64 GetElapsedMs() const;

  // Milliseconds from request start to processing start.
  bool GetTimeToStartProcessingMs(int64* elapsed_ms) const {
    return GetTimeFromStart(processing_start_ts_ms_, elapsed_ms);
  }

  // Milliseconds spent "processing": end time - start time - fetch time.
  // TODO(gee): This naming is somewhat misleading since it is from request
  // start not processing start.  Leaving as is for historical reasons, at
  // least for the time being.
  bool GetProcessingElapsedMs(int64* elapsed_ms) const;

  // Milliseconds from request start to pcache lookup start.
  bool GetTimeToPropertyCacheLookupStartMs(int64* elapsed_ms) const {
    return GetTimeFromStart(pcache_lookup_start_ts_ms_, elapsed_ms);
  }

  // Milliseconds from request start to pcache lookup end.
  bool GetTimeToPropertyCacheLookupEndMs(int64* elapsed_ms) const {
    return GetTimeFromStart(pcache_lookup_end_ts_ms_, elapsed_ms);
  }

  // HTTP Cache latencies.
  bool GetHTTPCacheLatencyMs(int64* latency_ms) const;
  bool GetL2HTTPCacheLatencyMs(int64* latency_ms) const;

  // Milliseconds from request start to fetch start.
  bool GetTimeToStartFetchMs(int64* elapsed_ms) const;

  // Milliseconds from fetch start to header received.
  bool GetFetchHeaderLatencyMs(int64* latency_ms) const;

  // Milliseconds from fetch start to fetch end.
  bool GetFetchLatencyMs(int64* latency_ms) const;

  // Milliseconds from receiving the request (Init) to responding with the
  // first byte of data.
  bool GetTimeToFirstByte(int64* latency_ms) const;

  // Milliseconds from request start to parse start.
  bool GetTimeToStartParseMs(int64* elapsed_ms) const {
    return GetTimeFromStart(parsing_start_ts_ms_, elapsed_ms);
  }

  int64 init_ts_ms() const { return init_ts_ms_; }

  int64 start_ts_ms() const { return start_ts_ms_; }

 private:
  int64 NowMs() const;

  // Set "ts_ms" to NowMs().
  void SetToNow(int64* ts_ms) const;

  // Set "elapsed_ms" to "ts_ms" - start_ms_ and returns true on success.
  // Returns false if either start_ms_ or "ts_ms" have not been set (< 0).
  bool GetTimeFromStart(int64 ts_ms, int64* elapsed_ms) const;

  Timer* timer_;

  // Event Timestamps.  These should appear in (roughly) chronological order.
  // These need not be protected by mu_ as they are only accessed by a single
  // thread at any given time, and subsequent accesses are made through
  // paths which are synchronized by other locks (pcache callback collector,
  // sequences, etc.).
  int64 init_ts_ms_;
  int64 start_ts_ms_;
  int64 processing_start_ts_ms_;
  int64 pcache_lookup_start_ts_ms_;
  int64 pcache_lookup_end_ts_ms_;
  int64 parsing_start_ts_ms_;
  int64 end_ts_ms_;

  AbstractMutex* mu_;  // Not owned by TimingInfo.
  // The following members are protected by mu_;
  int64 fetch_start_ts_ms_;
  int64 fetch_header_ts_ms_;
  int64 fetch_end_ts_ms_;
  int64 first_byte_ts_ms_;

  // Latencies.
  int64 http_cache_latency_ms_;
  int64 l2http_cache_latency_ms_;

  DISALLOW_COPY_AND_ASSIGN(RequestTimingInfo);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_OPT_LOGGING_REQUEST_TIMING_INFO_H_
