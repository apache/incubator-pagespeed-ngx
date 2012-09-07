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

// Author: mmohabey@google.com (Megha Mohabey)

#ifndef NET_INSTAWEB_AUTOMATIC_PUBLIC_FLUSH_EARLY_FLOW_H_
#define NET_INSTAWEB_AUTOMATIC_PUBLIC_FLUSH_EARLY_FLOW_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

class AsyncFetch;
class FlushEarlyInfo;
class Histogram;
class MessageHandler;
class ProxyFetchPropertyCallbackCollector;
class ProxyFetchFactory;
class ServerContext;
class RewriteDriver;
class Statistics;
class TimedVariable;

// FlushEarlyFlow manages the flow for the rewriters which flush a response to
// the client before receiving a response from the origin server. If a request
// can be responded to early, then FlushEarlyFlow is initiated. It also has
// helper functions to update the property cache with the response headers which
// are used when a request is responded to early.
class FlushEarlyFlow {
 public:
  static const char kNumRequestsFlushedEarly[];
  static const char kNumResourcesFlushedEarly[];
  static const char kFlushEarlyRewriteLatencyMs[];

  static void Start(
      const GoogleString& url,
      AsyncFetch* base_fetch,
      RewriteDriver* driver,
      ProxyFetchFactory* factory,
      ProxyFetchPropertyCallbackCollector* property_callback);

  static void Initialize(Statistics* stats);

  virtual ~FlushEarlyFlow();

  // Checks whether the request can be flushed early.
  static bool CanFlushEarly(const GoogleString& url,
                            const AsyncFetch* async_fetch,
                            const RewriteDriver* driver);

 private:
  // Flush some response for this request before receiving the fetch response
  // from the origin server.
  void FlushEarly();

  FlushEarlyFlow(const GoogleString& url,
                 AsyncFetch* base_fetch,
                 RewriteDriver* driver,
                 ProxyFetchFactory* factory,
                 ProxyFetchPropertyCallbackCollector* property_cache_callback);

  // Generates a dummy head with subresources and counts the number of resources
  // which can be flused early.
  void GenerateDummyHeadAndCountResources(
      const FlushEarlyInfo& flush_early_info);

  // Generates response headers from previous values stored in property cache.
  void GenerateResponseHeaders(const FlushEarlyInfo& flush_early_info);

  GoogleString GetHeadString(const FlushEarlyInfo& flush_early_info,
                             const char* css_format,
                             const char* js_format);

  // Callback that is invoked after we rewrite the early head.
  // start_time_ms indicates the time we started rewriting the flush early
  // head. This is set to -1 if is_experimental_hit is false.
  void FlushEarlyRewriteDone(int64 start_time_ms);

  // Triggers ProxyFetchFactory::StartNewProxyFetch.
  void TriggerProxyFetch();

  void Write(const StringPiece& val);

  // Write the script content to base_fetch.
  void WriteScript(const GoogleString& script_content);

  GoogleString url_;
  GoogleString dummy_head_;
  StringWriter dummy_head_writer_;
  int num_resources_flushed_;

  AsyncFetch* base_fetch_;
  AsyncFetch* flush_early_fetch_;
  RewriteDriver* driver_;
  ProxyFetchFactory* factory_;
  ServerContext* manager_;
  ProxyFetchPropertyCallbackCollector* property_cache_callback_;
  bool should_flush_early_lazyload_script_;
  bool should_flush_early_js_defer_script_;
  MessageHandler* handler_;

  TimedVariable* num_requests_flushed_early_;
  TimedVariable* num_resources_flushed_early_;
  Histogram* flush_early_rewrite_latency_ms_;

  DISALLOW_COPY_AND_ASSIGN(FlushEarlyFlow);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_AUTOMATIC_PUBLIC_FLUSH_EARLY_FLOW_H_
