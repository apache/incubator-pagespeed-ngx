// Copyright 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_SYSTEM_PUBLIC_IN_PLACE_RESOURCE_RECORDER_H_
#define NET_INSTAWEB_SYSTEM_PUBLIC_IN_PLACE_RESOURCE_RECORDER_H_

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/inflating_fetch.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"
#include "pagespeed/kernel/base/atomic_int32.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

class HTTPCache;
class MessageHandler;
class Statistics;
class Variable;

// Records a copy of a resource streamed through it and saves the result to
// the cache if it's cacheable. Used in the In-Place Resource Optimization
// (IPRO) flow to get resources into the cache.
class InPlaceResourceRecorder : public Writer {
 public:
  enum HeadersKind {
    // Headers should only be used to determine if context was gzip'd by
    // a reverse proxy.
    kPreliminaryHeaders,

    // Headers are complete.
    kFullHeaders
  };

  // Does not take ownership of request_headers, cache nor handler.
  // Like other callbacks, InPlaceResourceRecorder is self-owned and will
  // delete itself when DoneAndSetHeaders() is called.
  InPlaceResourceRecorder(
      const RequestContextPtr& request_context,
      StringPiece url, StringPiece fragment,
      RequestHeaders::Properties request_properties,
      bool respect_vary, int max_response_bytes, int max_concurrent_recordings,
      int64 implicit_cache_ttl_ms, HTTPCache* cache, Statistics* statistics,
      MessageHandler* handler);

  // Normally you should use DoneAndSetHeaders rather than deleting this
  // directly.
  virtual ~InPlaceResourceRecorder();

  static void InitStats(Statistics* statistics);

  // These take a handler for compatibility with the Writer API, but the handler
  // is not used.
  virtual bool Write(const StringPiece& contents, MessageHandler* handler);

  // Flush is a no-op because we have to buffer up the whole contents before
  // writing to cache.
  virtual bool Flush(MessageHandler* handler) { return true; }

  // Sometimes the response headers prohibit IPRO:
  //  * If it's not an IPRO content type.
  //  * If it's not served as cacheable.
  //  * If there's a content length, and it's over our max.
  // In these cases, shift into the failed state and stop any resource
  // recording.
  //
  // At this time we might also realize that there are too many IPRO recordings
  // going on and skip IPRO for that reason.  In that case we don't mark the
  // resource as not ipro-cacheable.
  //
  // You must call ConsiderResponseHeaders() with whatever information is
  // available before payload. If it's only enough to determine if content
  // is gzip'ed, pass in kPreliminaryHeaders. If it's the complete final
  // headers, pass in kFullHeaders.
  //
  // Call DoneAndSetHeaders() after the entire payload and headers are
  // available. Note that only Content-Encoding: from ConsiderResponseHeaders
  // will be used to determine whether to gunzip content or not. This is done
  // since in Apache we can only capture the full headers after mod_deflate has
  // already run, while content is captured before.
  //
  // Does not take ownership of response_headers.
  void ConsiderResponseHeaders(HeadersKind headers_kind,
                               ResponseHeaders* response_headers);

  // Call if something went wrong. The results will not be added to cache.  You
  // still need to call DoneAndSetHeaders().
  void Fail() { failure_ = true; }

  // Call when finished and the final response headers are known.
  // Because of Apache's quirky filter order, we cannot get both the
  // uncompressed final contents and the complete headers at the same time.
  //
  // Does not take ownership of response_headers.
  //
  // Deletes itself. Do not use object after calling DoneAndSetHeaders().
  void DoneAndSetHeaders(ResponseHeaders* response_headers);

  const GoogleString& url() const { return url_; }
  MessageHandler* handler() { return handler_; }

  bool failed() { return failure_; }
  bool limit_active_recordings() { return max_concurrent_recordings_ != 0; }

  int64 implicit_cache_ttl_ms() { return implicit_cache_ttl_ms_; }

 private:
  class HTTPValueFetch : public AsyncFetchUsingWriter {
   public:
    HTTPValueFetch(const RequestContextPtr& request_context, HTTPValue* value)
        : AsyncFetchUsingWriter(request_context, value) {}
    virtual void HandleDone(bool /*ok*/) {}
    virtual void HandleHeadersComplete() {}
  };

  bool IsIproContentType(ResponseHeaders* response_headers);

  void DroppedDueToSize();

  const GoogleString url_;
  const GoogleString fragment_;
  const RequestHeaders::Properties request_properties_;
  const ResponseHeaders::VaryOption respect_vary_;
  const unsigned int max_response_bytes_;
  const int max_concurrent_recordings_;
  const int64 implicit_cache_ttl_ms_;

  HTTPValue resource_value_;
  HTTPValueFetch write_to_resource_value_;
  InflatingFetch inflating_fetch_;

  HTTPCache* cache_;
  MessageHandler* handler_;

  Variable* num_resources_;
  Variable* num_inserted_into_cache_;
  Variable* num_not_cacheable_;
  Variable* num_failed_;
  Variable* num_dropped_due_to_load_;
  Variable* num_dropped_due_to_size_;

  // Track how many simultaneous recordings are underway in this process.  Not
  // used when max_concurrent_recordings_ == 0 (unlimited).
  static AtomicInt32 active_recordings_;
  // The status code from the response headers for RememberNotCacheable.
  int status_code_;
  // Something went wrong and this resource shouldn't be saved.
  bool failure_;

  // Track that ConsiderResponseHeaders() is called with full headers
  // exactly once.
  bool full_response_headers_considered_;

  // Track that ConsiderResponseHeaders() is called before DoneAndSetHeaders()
  bool consider_response_headers_called_;

  DISALLOW_COPY_AND_ASSIGN(InPlaceResourceRecorder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SYSTEM_PUBLIC_IN_PLACE_RESOURCE_RECORDER_H_
