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

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class LogRecord;
class RequestContext;
class RequestTrace;
class ThreadSystem;

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
  // |logging_mutex| will be passed to the request context's LogRecord, which
  // will take ownership of it. If you will be doing logging in a real
  // (threaded) environment, pass in a real mutex. If not, a NullMutex is fine.
  explicit RequestContext(AbstractMutex* logging_mutex);

  // TODO(marq): Move this test context factory to a test-specific file.
  //             Makes a request context for running tests.
  static RequestContextPtr NewTestRequestContext(ThreadSystem* thread_system);

  // Creates a new, unowned LogRecord, for use by some subordinate action.
  // Also useful in case of background activity where logging is required after
  // the response is written out, e.g., blink flow.
  virtual LogRecord* NewSubordinateLogRecord(AbstractMutex* logging_mutex);

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
  LogRecord* log_record();

  // Determines whether this request is using the SPDY protocol.
  bool using_spdy() const { return using_spdy_; }
  void set_using_spdy(bool x) { using_spdy_ = x; }

 protected:
  // The default constructor will not create a LogRecord. Subclass constructors
  // must do this explicitly.
  RequestContext();

  // The log record can only be set once. This should only be used by a subclass
  // during initialization.
  void set_log_record(LogRecord* l);

  // Destructors in refcounted classes should be protected.
  virtual ~RequestContext();
  REFCOUNT_FRIEND_DECLARATION(RequestContext);

 private:
  // Always non-NULL.
  scoped_ptr<LogRecord> log_record_;

  // Logs tracing events associated with the root request.
  scoped_ptr<RequestTrace> root_trace_context_;

  bool using_spdy_;

  DISALLOW_COPY_AND_ASSIGN(RequestContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_REQUEST_CONTEXT_H_
