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

  RequestTrace* trace_context() { return trace_context_.get(); }
  // Takes ownership of the given context.
  void set_trace_context(RequestTrace* x);

  // The log record for the this request, created when the request context is.
  LogRecord* log_record();

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

  // Logs tracing events.
  scoped_ptr<RequestTrace> trace_context_;

  DISALLOW_COPY_AND_ASSIGN(RequestContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_REQUEST_CONTEXT_H_
