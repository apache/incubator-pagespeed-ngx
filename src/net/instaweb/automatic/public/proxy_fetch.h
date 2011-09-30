/*
 * Copyright 2011 Google Inc.
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

// Author: sligocki@google.com (Shawn Ligocki)
//
// NOTE: This interface is actively under development and may be
// changed extensively. Contact us at mod-pagespeed-discuss@googlegroups.com
// if you are interested in using it.

#ifndef NET_INSTAWEB_AUTOMATIC_PUBLIC_PROXY_FETCH_H_
#define NET_INSTAWEB_AUTOMATIC_PUBLIC_PROXY_FETCH_H_

#include <deque>

#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class CacheUrlAsyncFetcher;
class Histogram;
class QueuedWorker;
class ResourceManager;
class RewriteDriver;
class RewriteOptions;
class TimedVariable;
class Timer;

class ProxyFetch;

// Factory for creating and starting ProxyFetches. Must outlive all
// ProxyFetches it creates.
class ProxyFetchFactory {
 public:
  ProxyFetchFactory(ResourceManager* manager);
  ~ProxyFetchFactory();

  // Pass custom_options = NULL to use default options.
  //
  // Takes ownership of custom_options.
  void StartNewProxyFetch(const GoogleString& url,
                          const RequestHeaders& request_headers,
                          RewriteOptions* custom_options,
                          ResponseHeaders* response_headers,
                          Writer* base_writer,
                          UrlAsyncFetcher::Callback* callback);

 private:
  friend class ProxyFetch;

  // Helps track the status of in-flight ProxyFetches.  These are intended for
  // use only by ProxyFetch.
  //
  // TODO(jmarantz): Enumerate outstanding fetches in server status page.
  void Start(ProxyFetch* proxy_fetch);
  void Finish(ProxyFetch* proxy_fetch);

  ResourceManager* manager_;
  Timer* timer_;
  MessageHandler* handler_;

  // Used to support caching input HTML and un-rewritten resources.
  scoped_ptr<CacheUrlAsyncFetcher> cache_fetcher_;

  scoped_ptr<AbstractMutex> outstanding_proxy_fetches_mutex_;
  std::set<ProxyFetch*> outstanding_proxy_fetches_;

  DISALLOW_COPY_AND_ASSIGN(ProxyFetchFactory);
};

// Manages a single fetch of an HTML or resource file from the original server.
// If it is an HTML file, it is rewritten.
// Fetch is initialized by calling ProxyFetchFactory::StartNewProxyFetch().
// For fetching pagespeed rewritten resources, use ResourceFetch.
// This is only meant to be used by ProxyInterface.
//
// Takes ownership of custom_options.
//
// The ProxyFetch passes through non-HTML directly to base_writer.
//
// For HTML, the sequence is this:
//    1. HeadersComplete is called, allowing us to establish we've got HTML.
//    2. Some number of calls to Write occur.
//    3. Optional: Flush is called, followed by more Writes.  Repeat.
//    4. Done is called.
// These virtual methods are called from some arbitrary thread, e.g. a
// dedicated fetcher thread.  We use a QueuedWorkerPool::Sequence to
// offload them to a worker-thread.  This implementation bundles together
// multiple Writes, and depending on the timing, may move Flushes to
// follow Writes and collapse multiple Flushes into one.
class ProxyFetch : public AsyncFetch {
 public:
  // Public interface from AsyncFetch.
  virtual void HeadersComplete();
  virtual bool Write(const StringPiece& content, MessageHandler* handler);
  virtual bool Flush(MessageHandler* handler);
  virtual void Done(bool success);

 private:
  friend class ProxyFetchFactory;

  ProxyFetch(const GoogleString& url, const RequestHeaders& request_headers,
             RewriteOptions* custom_options,
             ResponseHeaders* response_headers,
             Writer* base_writer, ResourceManager* manager,
             Timer* timer, UrlAsyncFetcher::Callback* callback,
             ProxyFetchFactory* factory);
  virtual ~ProxyFetch();

  // Sets up driver_, registering the writer and start parsing url.
  // Returns whether we started parsing successfully or not.
  bool StartParse();
  bool IsHtml();
  const RewriteOptions* Options();

  // Handles buffered HTML writes, flushes, and done calls
  // in a QueuedWorkerPool::Sequence.
  void ExecuteQueued();

  // Schedules the task to run any buffered work, if needed. Assumes mutex
  // held.
  void ScheduleQueueExecutionIfNeeded();

  // Frees up the RewriteDriver (via FinishParse or ReleaseRewriteDriver),
  // calls the callback (nulling out callback_ to ensure that we don't
  // do it again), notifies the ProxyInterface that the fetch is
  // complete, and deletes the ProxyFetch.
  void Finish(bool success);

  // Used to wrap up the FinishParseAsync invocation.
  void CompleteFinishParse(bool success);

  // Callback we give to ExecuteFlushIfRequestedAsync to notify us when
  // it's done with its work.
  void FlushDone();

  const GoogleString url_;
  ResponseHeaders* response_headers_;
  Writer* base_writer_;
  ResourceManager* resource_manager_;
  Timer* timer_;
  UrlAsyncFetcher::Callback* callback_;

  bool pass_through_;
  bool started_parse_;

  // Statistics
  int64 start_time_us_;

  // If we're given custom options, we store them here until
  // we hand them over to the rewrite driver.
  scoped_ptr<RewriteOptions> custom_options_;

  // Similarly if we have a UA string in RequestHeaders, we'll store it here.
  // (If not, this will be null).
  scoped_ptr<GoogleString> request_user_agent_;

  // ProxyFetch is responsible for getting RewriteDrivers from the pool and
  // putting them back.
  RewriteDriver* driver_;

  // True if we have queued up ExecuteQueued but did not
  // execute it yet.
  bool queue_run_job_created_;

  // As the UrlAsyncFetcher calls our Write & Flush methods, we collect
  // the text in text_queue, and note the Flush call in
  // network_flush_requested_, returning control to the fetcher as quickly
  // as possible so it can continue to process incoming network traffic.
  //
  // We offload the handling of the incoming text events to a
  // QueuedWorkerPool::Sequence.  Note that we may receive a new chunk
  // of text while we are still processing an old chunk.  The sequentiality
  // is preserved by QueuedWorkerPool::Sequence.
  //
  // The Done callback is also indirected through this Sequence.
  scoped_ptr<AbstractMutex> mutex_;
  StringStarVector text_queue_;
  bool network_flush_outstanding_;
  QueuedWorkerPool::Sequence* sequence_;

  // done_oustanding_ will be true if we got called with ::Done but didn't
  // invoke Finish yet.
  bool done_outstanding_;

  // done_result_ is used to store the result of ::Done if we're deferring
  // handling it until the driver finishes handling a Flush.
  bool done_result_;

  // We may also end up receiving new events in between calling FlushAsync
  // and getting the callback called. In that case, we want to hold off
  // on actually dispatching things queued up above.
  bool waiting_for_flush_to_finish_;

  ProxyFetchFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyFetch);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_AUTOMATIC_PUBLIC_PROXY_FETCH_H_
