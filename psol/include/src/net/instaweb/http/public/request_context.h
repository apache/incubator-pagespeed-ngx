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

class BaseTraceContext;

// A class which wraps state associated with a request.
//
// This object should be reference counted, wrapped in a RequestContextPtr. We
// use reference counting because, depending on the timing of asynchronous
// rewrites, RPC calls, and so on, a RequestContext may outlive the original
// HTTP request serving, or not. Reference counting avoids the complexity of
// explicit transfer of ownership in these cases.
class RequestContext : public RefCounted<RequestContext> {
 public:
  RequestContext();
  virtual ~RequestContext();

  // The trace context. Takes ownership of the given context.
  BaseTraceContext* trace_context() { return trace_context_.get(); }
  void set_trace_context(BaseTraceContext* x);

 private:
  // Logs tracing events.
  scoped_ptr<BaseTraceContext> trace_context_;

  DISALLOW_COPY_AND_ASSIGN(RequestContext);
};

typedef RefCountedPtr<RequestContext> RequestContextPtr;

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_REQUEST_CONTEXT_H_
