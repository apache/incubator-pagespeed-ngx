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

#ifndef PAGESPEED_AUTOMATIC_PROXY_FETCH_H_
#define PAGESPEED_AUTOMATIC_PROXY_FETCH_H_

#include <map>
#include <set>
#include <vector>

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/util/public/fallback_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/automatic/html_detector.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest_prod.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"

namespace net_instaweb {

class CacheUrlAsyncFetcher;
class GoogleUrl;
class MessageHandler;
class ProxyFetch;
class ProxyFetchPropertyCallbackCollector;
class QueuedAlarm;
class ServerContext;
class RewriteDriver;
class RewriteOptions;
class Timer;

// Factory for creating and starting ProxyFetches. Must outlive all
// ProxyFetches it creates.
class ProxyFetchFactory {
 public:
  explicit ProxyFetchFactory(ServerContext* server_context);
  ~ProxyFetchFactory();

  // Convenience method that calls CreateNewProxyFetch and then StartFetch() on
  // the resulting fetch.
  void StartNewProxyFetch(
      const GoogleString& url,
      AsyncFetch* async_fetch,
      RewriteDriver* driver,
      ProxyFetchPropertyCallbackCollector* property_callback,
      AsyncFetch* original_content_fetch);

  // Creates a new proxy fetch and passes it to the fetcher to start it.  If the
  // UrlNamer doesn't authorize this url it calls CleanUp() on the driver,
  // Detach() on the property callback, Done() on the async_fetch and
  // original_content_fetch, and returns NULL.
  //
  // If you're using a fetcher for the original request content you should use
  // StartNewProxyFetch() instead. CreateNewProxyFetch is for callers who will
  // not be calling StartFetch() and instead will call HeadersComplete(),
  // Write(), Flush(), and Done() as they get data in from another source.
  ProxyFetch* CreateNewProxyFetch(
      const GoogleString& url,
      AsyncFetch* async_fetch,
      RewriteDriver* driver,
      ProxyFetchPropertyCallbackCollector* property_callback,
      AsyncFetch* original_content_fetch);

  // Initiates the PropertyCache lookup.  See ngx_pagespeed.cc or
  // proxy_interface.cc for example usage.
  static ProxyFetchPropertyCallbackCollector* InitiatePropertyCacheLookup(
      bool is_resource_fetch,
      const GoogleUrl& request_url,
      ServerContext* server_context,
      RewriteOptions* options,
      AsyncFetch* async_fetch);

  MessageHandler* message_handler() const { return handler_; }

 private:
  friend class ProxyFetch;

  // Helps track the status of in-flight ProxyFetches.  These are intended for
  // use only by ProxyFetch.
  //
  // TODO(jmarantz): Enumerate outstanding fetches in server status page.
  void RegisterNewFetch(ProxyFetch* proxy_fetch);
  void RegisterFinishedFetch(ProxyFetch* proxy_fetch);

  ServerContext* server_context_;
  Timer* timer_;
  MessageHandler* handler_;

  scoped_ptr<AbstractMutex> outstanding_proxy_fetches_mutex_;
  std::set<ProxyFetch*> outstanding_proxy_fetches_;

  DISALLOW_COPY_AND_ASSIGN(ProxyFetchFactory);
};

// Tracks a single property-cache lookup. These lookups are initiated
// immediately upon handling the request, in parallel with determining
// domain-specific RewriteOptions and fetching the HTTP headers for the HTML.
//
// Request handling can proceed in parallel with the property-cache lookups,
// including RewriteOptions lookup and initiating the HTTP fetch. However,
// handling incoming bytes will be blocked waiting for property-cache lookups
// to complete.
class ProxyFetchPropertyCallback : public PropertyPage {
 public:
  ProxyFetchPropertyCallback(PageType page_type,
                             PropertyCache* property_cache,
                             const StringPiece& url,
                             const StringPiece& options_signature_hash,
                             UserAgentMatcher::DeviceType device_type,
                             ProxyFetchPropertyCallbackCollector* collector,
                             AbstractMutex* mutex);

  PageType page_type() const { return page_type_; }

  // Delegates to collector_'s IsCacheValid.
  virtual bool IsCacheValid(int64 write_timestamp_ms) const;

  virtual void Done(bool success);

 private:
  PageType page_type_;
  UserAgentMatcher::DeviceType device_type_;
  ProxyFetchPropertyCallbackCollector* collector_;
  GoogleString url_;
  DISALLOW_COPY_AND_ASSIGN(ProxyFetchPropertyCallback);
};

// Tracks a collection of property-cache lookups occurring in parallel.
// Sequence is used to execute various functions in an orderly fashion to
// avoid any kind of race between Done(), ConnectProxyFetch(), Detach() and
// AddPostLookupTask().  When any function is called, it is added to the
// sequence and added function will be executed immediately if sequence is
// free, otherwise it will wait for its turn.
//
// Order of events:
// InitiatePropertyCacheLookup-->AddPostLookupTask-->Initiate Html Fetch
//            |                 (Added to Sequence)         |
//            |                                        Fetch Done
//        Lookup Done()                                     |
//    (Added to Sequence)                           -------------------
//                                          is html |           !html |
//                                        ConnectProxyFetch()     Detach()
//                                                 (Added to Sequence)
//
// This will also wait for RequestHeadersComplete() to be called before
// invoking any post-completion callbacks (but not before canceling them
// due to Detach).
class ProxyFetchPropertyCallbackCollector {
 public:
  ProxyFetchPropertyCallbackCollector(ServerContext* server_context,
                                      const StringPiece& url,
                                      const RequestContextPtr& req_ctx,
                                      const RewriteOptions* options,
                                      UserAgentMatcher::DeviceType device_type);
  virtual ~ProxyFetchPropertyCallbackCollector();

  // Add a callback to be handled by this collector.
  // Transfers ownership of the callback to the collector.
  void AddCallback(ProxyFetchPropertyCallback* callback);

  // Must be called once request headers have been resolved from configuration,
  // Gates successful post-lookup callback invocation.
  void RequestHeadersComplete();

  // In our flow, we initiate the property-cache lookup prior to
  // creating a proxy-fetch, so that RewriteOptions lookup can proceed
  // in parallel.  If/when we determine that ProxyFetch is associated
  // with HTML content, we connect it to this callback.  Note that if
  // the property cache lookups have completed, this will result in
  // a direct call into proxy_fetch->PropertyCacheComplete.
  void ConnectProxyFetch(ProxyFetch* proxy_fetch);

  // If for any reason we decide *not* to initiate a ProxyFetch for a
  // request, then we need to 'detach' this request so that we can
  // delete it once it completes, rather than waiting for a
  // ProxyFetch to be inserted. The status code of the response is passed from
  // ProxyFetch to the Collector. In case the status code is unknown then pass
  // RewriteDriver::kStatusCodeUnknown.
  void Detach(HttpStatus::Code status_code);

  // Returns the actual property page.
  PropertyPage* property_page() {
    return fallback_property_page_ == NULL ?
        NULL : fallback_property_page_->actual_property_page();
  }

  // Returns the fallback property page.
  FallbackPropertyPage* fallback_property_page() {
    return fallback_property_page_.get();
  }

  // Returns the collected PropertyPage with the corresponding page_type.
  // Ownership of the object is transferred to the caller.
  PropertyPage* ReleasePropertyPage(
      ProxyFetchPropertyCallback::PageType page_type);

  // Releases the ownership of fallback property page.
  FallbackPropertyPage* ReleaseFallbackPropertyPage() {
    return fallback_property_page_.release();
  }

  // Releases the ownership of origin property page.
  PropertyPage* ReleaseOriginPropertyPage() {
    return origin_property_page_.release();
  }

  // In our flow, property-page will be available via RewriteDriver only after
  // ProxyFetch is set. But there may be instances where the result may be
  // required even before proxy-fetch is created. Any task that depends on the
  // PropertyCache result will be executed as soon as PropertyCache lookup is
  // done and RequestHeadersComplete() has been called.
  //
  // func is guaranteed to execute after PropertyCache lookup has completed, as
  // long as ProxyFetch is not set before PropertyCache lookup is done. One
  // should use PropertyCache result via RewriteDriver if some other thread can
  // initiate SetProxyFetch().
  void AddPostLookupTask(Function* func);

  // If options_ is NULL returns true.  Else, returns true if (url_,
  // write_timestamp_ms) is valid as per URL cache invalidation entries is
  // options_.
  bool IsCacheValid(int64 write_timestamp_ms) const;

  // Called by a ProxyFetchPropertyCallback when the former is complete.
  void Done(ProxyFetchPropertyCallback* callback);

  const RequestContextPtr& request_context() { return request_context_; }

  // Returns DeviceType from device property page.
  UserAgentMatcher::DeviceType device_type() { return device_type_; }

 private:
  friend class ProxyFetchPropertyCallbackCollectorTest;
  void ExecuteDone(ProxyFetchPropertyCallback* callback);
  void ExecuteAddPostLookupTask(Function* func);
  void ExecuteConnectProxyFetch(ProxyFetch* proxy_fetch);
  void ExecuteDetach(HttpStatus::Code status_code);
  void ExecuteRequestHeadersComplete();

  void RunPostLookupsAndCleanupIfSafe();

  // Updates the status code of response in property cache.
  void UpdateStatusCodeInPropertyCache();

  std::set<ProxyFetchPropertyCallback*> pending_callbacks_;
  std::map<ProxyFetchPropertyCallback::PageType, PropertyPage*>
  property_pages_;
  scoped_ptr<AbstractMutex> mutex_;
  ServerContext* const server_context_;
  QueuedWorkerPool::Sequence* const sequence_;
  const GoogleString url_;
  const RequestContextPtr request_context_;
  const UserAgentMatcher::DeviceType device_type_;
  bool is_options_valid_;     // protected by mutex_.
  // Unless guarded by mutex_, the fields are only accessed by code serialized
  // via sequence_.
  bool detached_;
  bool done_;
  bool request_headers_ok_;
  ProxyFetch* proxy_fetch_;
  std::vector<Function*> post_lookup_task_vector_;
  const RewriteOptions* options_;  // protected by mutex_;
  HttpStatus::Code status_code_;  // status_code_ of the response.
  scoped_ptr<FallbackPropertyPage> fallback_property_page_;
  scoped_ptr<PropertyPage> origin_property_page_;

  DISALLOW_COPY_AND_ASSIGN(ProxyFetchPropertyCallbackCollector);
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
class ProxyFetch : public SharedAsyncFetch {
 public:
  // These strings identify sync-points for reproducing races between
  // PropertyCache lookup completion and Origin HTML Fetch completion.
  static const char kCollectorConnectProxyFetchFinish[];
  static const char kCollectorDetachFinish[];
  static const char kCollectorDoneFinish[];
  static const char kCollectorFinish[];
  static const char kCollectorDetachStart[];
  static const char kCollectorRequestHeadersCompleteFinish[];

  // These strings identify sync-points for introducing races between
  // PropertyCache lookup completion and HeadersComplete.
  static const char kHeadersSetupRaceAlarmQueued[];
  static const char kHeadersSetupRaceDone[];
  static const char kHeadersSetupRaceFlush[];
  static const char kHeadersSetupRacePrefix[];
  static const char kHeadersSetupRaceWait[];

  // Number of milliseconds to wait, in a test, for an event that we
  // are hoping does not occur, specifically an inappropriate call to
  // base_fetch()->HeadersComplete() while we are still mutating
  // response headers in SetupForHtml.
  //
  // This is used only for testing.
  static const int kTestSignalTimeoutMs = 200;

  void set_trusted_input(bool trusted_input) { trusted_input_ = trusted_input; }

 protected:
  // protected interface from AsyncFetch.
  virtual void HandleHeadersComplete();
  virtual bool HandleWrite(const StringPiece& content, MessageHandler* handler);
  virtual bool HandleFlush(MessageHandler* handler);
  virtual void HandleDone(bool success);
  virtual bool IsCachedResultValid(const ResponseHeaders& headers);

 private:
  friend class ProxyFetchFactory;
  friend class ProxyFetchPropertyCallbackCollector;
  friend class MockProxyFetch;
  FRIEND_TEST(ProxyFetchTest, TestInhibitParsing);
  FRIEND_TEST(ProxyFetchTest, TestFollowFlushes);

  // Called by ProxyFetchPropertyCallbackCollector when all property-cache
  // fetches are complete.  This function takes ownership of collector.
  virtual void PropertyCacheComplete(
      ProxyFetchPropertyCallbackCollector* collector);

  // If cross_domain is true, we're requested under a domain different from
  // the underlying host, using proxy mode in UrlNamer.
  ProxyFetch(const GoogleString& url,
             bool cross_domain,
             ProxyFetchPropertyCallbackCollector* property_cache_callback,
             AsyncFetch* async_fetch,
             AsyncFetch* original_content_fetch,
             RewriteDriver* driver,
             ServerContext* server_context,
             Timer* timer,
             ProxyFetchFactory* factory);
  virtual ~ProxyFetch();

  const RewriteOptions* Options();

  // Once we have decided this is HTML, begin parsing and set headers.
  void SetupForHtml();

  // Adds a pagespeed header to response_headers if enabled.
  void AddPagespeedHeader();

  // Sets up driver_, registering the writer and start parsing url.
  // Returns whether we started parsing successfully or not.
  bool StartParse();

  // Start the fetch which includes preparing the request.
  void StartFetch();

  // Actually do the fetch, called from callback of StartFetch.
  // "prepare_success" represents whether the request was prepared successfully
  // by the UrlNamer.
  void DoFetch(bool prepare_success);

  // Handles buffered HTML writes, flushes, and done calls
  // in the QueuedWorkerPool::Sequence sequence_.
  void ExecuteQueued();

  // Schedules the task to run any buffered work, if needed. Assumes mutex
  // held.
  void ScheduleQueueExecutionIfNeeded();

  // Frees up the RewriteDriver (via FinishParse or Cleanup),
  // calls the callback (nulling out callback_ to ensure that we don't
  // do it again), notifies the ProxyInterface that the fetch is
  // complete, and deletes the ProxyFetch.
  void Finish(bool success);

  // Used to wrap up the FinishParseAsync invocation.
  void CompleteFinishParse(bool success);

  // Callback we give to ExecuteFlushIfRequestedAsync to notify us when
  // it's done with its work.
  void FlushDone();

  // Management functions for idle_alarm_. Must only be called from
  // within sequence_.

  // Cancels any previous alarm.
  void CancelIdleAlarm();

  // Cancels previous alarm and starts next one.
  void QueueIdleAlarm();

  // Handler for the alarm; run in sequence_.
  void HandleIdleAlarm();

  GoogleString url_;
  ServerContext* server_context_;
  Timer* timer_;

  scoped_ptr<CacheUrlAsyncFetcher> cache_fetcher_;

  // True if we're handling a cross-domain request in proxy mode, which
  // should do some additional checking.
  bool cross_domain_;

  // Does page claim to be "Content-Type: text/html"? (It may be lying)
  bool claims_html_;

  // Has a call to StartParse succeeded? We'll only do this if we actually
  // decide it is HTML.
  bool started_parse_;

  // Has a call to RewriteDriver::ParseText been made yet.
  bool parse_text_called_;

  // Tracks whether Done() has been called.
  bool done_called_;

  HtmlDetector html_detector_;

  // Tracks a set of outstanding property-cache lookups.  This is NULLed
  // when the property-cache completes or when we detach it.  We use
  // this to detach the callback if we decide we don't care about the
  // property-caches because we discovered we are not working with HTML.
  ProxyFetchPropertyCallbackCollector* property_cache_callback_;

  // Fetch where raw original headers and contents are sent.
  // To contrast, base_fetch() is sent rewritten contents and headers.
  // If NULL, original_content_fetch_ is ignored.
  AsyncFetch* original_content_fetch_;

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

  // Finish is true if we started Finish, perhaps doing FinishParseAsync.
  // Accessed only from within context of sequence_.
  bool finishing_;

  // done_result_ is used to store the result of ::Done if we're deferring
  // handling it until the driver finishes handling a Flush.
  bool done_result_;

  // We may also end up receiving new events in between calling FlushAsync
  // and getting the callback called. In that case, we want to hold off
  // on actually dispatching things queued up above.
  bool waiting_for_flush_to_finish_;

  // Alarm used to keep track of inactivity, in order to help issue
  // flushes. Must only be accessed from the thread context of sequence_
  QueuedAlarm* idle_alarm_;

  ProxyFetchFactory* factory_;

  // Set to true if this proxy_fetch is actually operating on trusted
  // (non-proxied) content.
  bool trusted_input_;

  DISALLOW_COPY_AND_ASSIGN(ProxyFetch);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_AUTOMATIC_PROXY_FETCH_H_
