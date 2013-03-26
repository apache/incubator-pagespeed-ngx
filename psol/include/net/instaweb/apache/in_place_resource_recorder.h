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

#ifndef NET_INSTAWEB_APACHE_IN_PLACE_RESOURCE_RECORDER_H_
#define NET_INSTAWEB_APACHE_IN_PLACE_RESOURCE_RECORDER_H_

#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

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
  // delete itself when DoneAndSetHeaders(). is called.
  InPlaceResourceRecorder(StringPiece url, RequestHeaders* request_headers,
                          bool respect_vary, HTTPCache* cache,
                          Statistics* statistics, MessageHandler* handler);
  virtual ~InPlaceResourceRecorder();

  static void InitStats(Statistics* statistics);

  virtual bool Write(const StringPiece& contents, MessageHandler* handler);
  virtual bool Flush(MessageHandler* handler);

  // Call if something went wrong. The results will not be added to cache.
  void Fail() { success_ = false; }

  // Call when finished and the final response headers are known.
  // Because of Apache's quirky filter order, we cannot get both the
  // uncompressed final contents and the complete headers at the same time.
  // Does not take ownership of response_headers.
  //
  // Deletes itself. Do not use object after calling DoneAndSetHeaders().
  void DoneAndSetHeaders(ResponseHeaders* response_headers);

  const GoogleString& url() const { return url_; }
  MessageHandler* handler() { return handler_; }

 private:
  const GoogleString url_;
  const scoped_ptr<RequestHeaders> request_headers_;
  const bool respect_vary_;

  HTTPValue resource_value_;
  bool success_;

  HTTPCache* cache_;
  MessageHandler* handler_;

  Variable* num_resources_;
  Variable* num_inserted_into_cache_;
  Variable* num_not_cacheable_;
  Variable* num_failed_;

  DISALLOW_COPY_AND_ASSIGN(InPlaceResourceRecorder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_IN_PLACE_RESOURCE_RECORDER_H_
