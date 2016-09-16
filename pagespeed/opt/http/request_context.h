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

#ifndef PAGESPEED_OPT_HTTP_REQUEST_CONTEXT_H_
#define PAGESPEED_OPT_HTTP_REQUEST_CONTEXT_H_

#include <set>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/http_options.h"
#include "pagespeed/opt/logging/request_timing_info.h"

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
  // |logging_mutex| will be passed to the request context's AbstractLogRecord,
  // which will take ownership of it. If you will be doing logging in a real
  // (threaded) environment, pass in a real mutex. If not, a NullMutex is fine.
  // |timer| will be passed to the RequestTimingInfo, which will *not* take
  // ownership. Passing NULL for |timer| is allowed.
  RequestContext(const HttpOptions& options, AbstractMutex* logging_mutex,
                 Timer* timer);
  // If you use this constructor, you MUST set_options() later.
  RequestContext(AbstractMutex* logging_mutex, Timer* timer);

  // TODO(marq): Move this test context factory to a test-specific file.
  //             Makes a request context for running tests.
  // Note: Test RequestContexts do not pay attention to options.
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

  // Determines whether this request is using the HTTP2 protocol.
  bool using_http2() const { return using_http2_; }
  void set_using_http2(bool x) { using_http2_ = x; }

  // Checks to see if the passed in Via: header indicates this connection
  // was terminated by an HTTP/2 proxy, and if so, sets the using_http2 bit.
  // (If there are multiple proxies, this looks only at the one closest to the
  //  user)
  //
  // This assumes that all the Via: headers are combined here, with the usual
  // comma separation.
  void SetHttp2SupportFromViaHeader(StringPiece header);

  // The minimal private suffix for the hostname specified in this request.
  // This should be calculated from the hostname by considering the list of
  // public suffixes and including one additional component.  So if a host is
  // "a.b.c.d.e.f.g" and "e.f.g" is on the public suffix list then the minimal
  // private suffix is "d.e.f.g".
  //
  // There are two ways of specifying the host -- with the Host header, or on
  // the initial request line.  The caller should make sure to look in both
  // places.
  //
  // If a system doesn't want to fragment the cache by minimal private suffix,
  // it may set value to the empty string.
  const GoogleString& minimal_private_suffix() const {
    return minimal_private_suffix_;
  }
  void set_minimal_private_suffix(StringPiece minimal_private_suffix) {
    minimal_private_suffix.CopyToString(&minimal_private_suffix_);
  }

  // Indicates whether the request-headers tell us that a browser can
  // render webp images.
  void SetAcceptsWebp(bool x);
  bool accepts_webp() const { return accepts_webp_; }

  // Indicates whether the request-headers tell us that a browser can extract
  // gzip compressed data.
  void SetAcceptsGzip(bool x);
  bool accepts_gzip() const { return accepts_gzip_; }

  int64 request_id() const {
    return request_id_;
  }
  void set_request_id(int64 x) {
    request_id_ = x;
  }

  const GoogleString& sticky_query_parameters_token() const {
    return sticky_query_parameters_token_;
  }
  void set_sticky_query_parameters_token(StringPiece x) {
    x.CopyToString(&sticky_query_parameters_token_);
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
  // RequestTimingInfo for example, to the underlying logging infrastructure.
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

  const RequestTimingInfo& timing_info() const { return timing_info_; }
  RequestTimingInfo* mutable_timing_info() { return &timing_info_; }

  void set_options(const HttpOptions& options) {
    DCHECK(!options_set_);
    options_set_ = true;
    options_ = options;
  }
  // This allows changing options already set.
  // TODO(sligocki): It would be nice if we could make sure options are only
  // set once. Is it worth the complexity to force that to be true?
  void ResetOptions(const HttpOptions& options) {
    options_set_ = true;
    options_ = options;
  }
  const HttpOptions& options() const {
    DCHECK(options_set_);
    return options_;
  }

  void Freeze() { frozen_ = true; }
  bool frozen() const { return frozen_; }

 protected:
  // TODO(gee): Fix this, it sucks.
  // The default constructor will not create a LogRecord. Subclass constructors
  // must do this explicitly.
  RequestContext(const HttpOptions& options, AbstractMutex* mutex,
                 Timer* timer, AbstractLogRecord* log_record);
  // Destructors in refcounted classes should be protected.
  virtual ~RequestContext();
  REFCOUNT_FRIEND_DECLARATION(RequestContext);

 private:
  void Init();

  // Always non-NULL.
  scoped_ptr<AbstractLogRecord> log_record_;

  RequestTimingInfo timing_info_;

  // Logs tracing events associated with the root request.
  scoped_ptr<RequestTrace> root_trace_context_;

  // Log for recording background rewritings.
  scoped_ptr<AbstractLogRecord> background_rewrite_log_record_;

  StringSet session_authorized_fetch_origins_;

  bool using_http2_;
  bool accepts_webp_;
  bool accepts_gzip_;
  bool frozen_;
  GoogleString minimal_private_suffix_;

  int64 request_id_;

  // The token specified by query parameter or header that must match the
  // configured value for options to be converted to cookies.
  GoogleString sticky_query_parameters_token_;

  bool options_set_;
  HttpOptions options_;

  DISALLOW_COPY_AND_ASSIGN(RequestContext);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_OPT_HTTP_REQUEST_CONTEXT_H_
