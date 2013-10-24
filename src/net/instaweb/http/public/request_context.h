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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_REQUEST_CONTEXT_H_
#define NET_INSTAWEB_HTTP_PUBLIC_REQUEST_CONTEXT_H_

#include <set>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class AbstractLogRecord;
class AbstractMutex;
class RequestContext;
class RequestTrace;
class ThreadSystem;
class Timer;

typedef RefCountedPtr<RequestContext> RequestContextPtr;

// A class which wraps state associated with a request.
//
// This object should be reference counted, wrapped in a RequestContextPtr. We
// use reference counting because, depending on the timing of asynchronous
// rewrites, RPC calls, and so on, a RequestContext may outlive the original
// HTTP request serving, or not. Reference counting avoids the complexity of
// explicit transfer of ownership in these cases.
class RequestContext : public RefCounted<RequestContext> {
 public:
  // Types of split html request.
  enum SplitRequestType {
    SPLIT_FULL,
    SPLIT_ABOVE_THE_FOLD,
    SPLIT_BELOW_THE_FOLD,
  };

  // |logging_mutex| will be passed to the request context's AbstractLogRecord,
  // which will take ownership of it. If you will be doing logging in a real
  // (threaded) environment, pass in a real mutex. If not, a NullMutex is fine.
  // |timer| will be passed to the TimingInfo, which will *not* take ownership.
  // Passing NULL for |timer| is allowed.
  explicit RequestContext(AbstractMutex* logging_mutex, Timer* timer);

  // TODO(marq): Move this test context factory to a test-specific file.
  //             Makes a request context for running tests.
  static RequestContextPtr NewTestRequestContext(ThreadSystem* thread_system) {
    return NewTestRequestContextWithTimer(thread_system, NULL);
  }
  static RequestContextPtr NewTestRequestContextWithTimer(
      ThreadSystem* thread_system, Timer* timer);
  static RequestContextPtr NewTestRequestContext(AbstractLogRecord* log_record);

  // Creates a new, unowned AbstractLogRecord, for use by some subordinate
  // action.  Also useful in case of background activity where logging is
  // required after the response is written out, e.g., blink flow.
  virtual AbstractLogRecord* NewSubordinateLogRecord(
      AbstractMutex* logging_mutex);

  // The root trace context is associated with the user request which we
  // are attempting to serve. If this is a request with constituent resources
  // that we rewrite, there may be several dependent fetches synthesized
  // by PSOL during rewrites. Those are traced separately.
  RequestTrace* root_trace_context() { return root_trace_context_.get(); }
  // Takes ownership of the given context.
  void set_root_trace_context(RequestTrace* x);

  // Creates a new RequestTrace associated with a request depending on the
  // root user request; e.g., a subresource fetch for an HTML page.
  //
  // This implementation is a no-op. Subclasses should customize this based
  // on their underlying tracing system. A few interface notes:
  // - The caller is not responsible for releasing memory or managing the
  //   lifecycle of the RequestTrace.
  // - A call to CreateDependentTraceContext() need not be matched by a call
  //   to ReleaseDependentTraceContext(). Cleanup should be automatic and
  //   managed by RequestContext subclass implementations.
  virtual RequestTrace* CreateDependentTraceContext(const StringPiece& label) {
    return NULL;
  }

  // Releases this object's reference to the given context and frees memory.
  // Calls to CreateDependentTraceContext need not be matched by
  // calls to this function. If a dependent trace span is not released when
  // the request context reference count drops to zero, this object will clean
  // all dependent traces.
  //
  // Note that automatic cleanup of dependent traces is provided for safety.
  // To provide meaningful performance statistics, cleanup should be
  // coupled with the completion of the event being traced.
  //
  // Subclasses should customize this based on their underlying tracing system.
  virtual void ReleaseDependentTraceContext(RequestTrace* t);

  // The log record for the this request, created when the request context is.
  virtual AbstractLogRecord* log_record();

  // Determines whether this request is using the SPDY protocol.
  bool using_spdy() const { return using_spdy_; }
  void set_using_spdy(bool x) { using_spdy_ = x; }

  // Indicates the type of split html request.
  SplitRequestType split_request_type() const {
    return split_request_type_;
  }
  void set_split_request_type(SplitRequestType type) {
    split_request_type_ = type;
  }

  int64 request_id() const {
    return request_id_;
  }
  void set_request_id(int64 x) {
    request_id_ = x;
  }

  // Authorized a particular external domain to be fetched from. The caller of
  // this method MUST ensure that the domain is not some internal site within
  // the firewall/LAN hosting the server. Note that this doesn't affect
  // rewriting at all.
  // TODO(morlovich): It's not clearly this is the appropriate mechanism
  // for all the authorizations --- we may want to scope this to a request
  // only.
  void AddSessionAuthorizedFetchOrigin(const GoogleString& origin) {
    session_authorized_fetch_origins_.insert(origin);
  }

  // Returns true for exactly the origins that were authorized for this
  // particular session by calls to AddSessionAuthorizedFetchOrigin()
  bool IsSessionAuthorizedFetchOrigin(const GoogleString& origin) const {
    return session_authorized_fetch_origins_.find(origin)
           != session_authorized_fetch_origins_.end();
  }

  // Prepare the AbstractLogRecord for a subsequent call to WriteLog.  This
  // might include propagating information collected in the RequestContext,
  // TimingInfo for example, to the underlying logging infrastructure.
  void PrepareLogRecordForOutput();

  // Write the log for background rewriting into disk.
  void WriteBackgroundRewriteLog();

  // Return the log record for background rewrites. If it doesn't exist, create
  // a new one.
  AbstractLogRecord* GetBackgroundRewriteLog(
      ThreadSystem* thread_system,
      bool log_urls,
      bool log_url_indices,
      int max_rewrite_info_log_size);

  // TimingInfo tracks various event timestamps over the lifetime of a request.
  // The timeline looks (roughly) like the following, with the associated
  // TimingInfo calls.
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
  class TimingInfo {
   public:
    // Initialize the TimingInfo with the specified Timer.  Sets init_ts_ to
    // Timer::NowMs, from which GetElapsedMs is based.
    // NOTE: Timer and mutex are not owned by TimingInfo.
    TimingInfo(Timer* timer, AbstractMutex* mutex);

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

    DISALLOW_COPY_AND_ASSIGN(TimingInfo);
  };

  const TimingInfo& timing_info() const { return timing_info_; }
  TimingInfo* mutable_timing_info() { return &timing_info_; }

 protected:
  // TODO(gee): Fix this, it sucks.
  // The default constructor will not create a LogRecord. Subclass constructors
  // must do this explicitly.
  RequestContext(AbstractMutex* mutex, Timer* timer,
                 AbstractLogRecord* log_record);
  // Destructors in refcounted classes should be protected.
  virtual ~RequestContext();
  REFCOUNT_FRIEND_DECLARATION(RequestContext);

 private:
  // Always non-NULL.
  scoped_ptr<AbstractLogRecord> log_record_;

  TimingInfo timing_info_;

  // Logs tracing events associated with the root request.
  scoped_ptr<RequestTrace> root_trace_context_;

  // Log for recording background rewritings.
  scoped_ptr<AbstractLogRecord> background_rewrite_log_record_;

  StringSet session_authorized_fetch_origins_;

  bool using_spdy_;
  SplitRequestType split_request_type_;;
  int64 request_id_;

  DISALLOW_COPY_AND_ASSIGN(RequestContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_REQUEST_CONTEXT_H_
