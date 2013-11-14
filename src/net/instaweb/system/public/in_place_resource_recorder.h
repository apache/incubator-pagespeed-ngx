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

#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"
#include "pagespeed/kernel/base/atomic_int32.h"

namespace net_instaweb {

class HTTPCache;
class MessageHandler;
class RequestHeaders;
class ResponseHeaders;
class Statistics;
class Variable;

// Records a copy of a resource streamed through it and saves the result to
// the cache if it's cacheable. Used in the In-Place Resource Optimization
// (IPRO) flow to get resources into the cache.
class InPlaceResourceRecorder : public Writer {
 public:
  // Takes ownership of request_headers, but not cache nor handler.
  // Like other callbacks, InPlaceResourceRecorder is self-owned and will
  // delete itself when DoneAndSetHeaders() is called.
  InPlaceResourceRecorder(
      StringPiece url, RequestHeaders* request_headers, bool respect_vary,
      int max_response_bytes, int max_concurrent_recordings, HTTPCache* cache,
      Statistics* statistics, MessageHandler* handler);

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
  // If you have the final headers before processing the content you should call
  // ConsiderResponseHeaders() yourself.  Otherwise DoneAndSetHeaders() will
  // call it for you.
  //
  // Does not take ownership of response_headers.
  void ConsiderResponseHeaders(ResponseHeaders* response_headers);

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

 private:
  bool IsIproContentType(ResponseHeaders* response_headers);

  const GoogleString url_;
  const scoped_ptr<RequestHeaders> request_headers_;
  const bool respect_vary_;
  const unsigned int max_response_bytes_;
  const int max_concurrent_recordings_;

  HTTPValue resource_value_;

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

  // Track that ConsiderResponseHeaders() is called exactly once.
  bool response_headers_considered_;

  DISALLOW_COPY_AND_ASSIGN(InPlaceResourceRecorder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SYSTEM_PUBLIC_IN_PLACE_RESOURCE_RECORDER_H_
