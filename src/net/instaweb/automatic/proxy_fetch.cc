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

#include "net/instaweb/automatic/public/proxy_fetch.h"

#include <algorithm>
#include <cstddef>

#include "base/logging.h"
#include "net/instaweb/config/rewrite_options_manager.h"
#include "net/instaweb/http/public/cache_url_async_fetcher.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/experiment_matcher.h"
#include "net/instaweb/rewriter/public/experiment_util.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/fallback_property_page.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/queued_alarm.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/request_trace.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/thread_synchronizer.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/http/content_type.h"

namespace net_instaweb {

const char ProxyFetch::kCollectorConnectProxyFetchFinish[] =
    "CollectorConnectProxyFetchFinish";
const char ProxyFetch::kCollectorDetachFinish[] = "CollectorDetachFinish";
const char ProxyFetch::kCollectorDoneFinish[] = "CollectorDoneFinish";
const char ProxyFetch::kCollectorFinish[] = "CollectorFinish";
const char ProxyFetch::kCollectorDetachStart[] = "CollectorDetachStart";
const char ProxyFetch::kCollectorRequestHeadersCompleteFinish[] =
    "kCollectorRequestHeadersCompleteFinish";

const char ProxyFetch::kHeadersSetupRaceAlarmQueued[] =
    "HeadersSetupRace:AlarmQueued";
const char ProxyFetch::kHeadersSetupRaceDone[] = "HeadersSetupRace:Done";
const char ProxyFetch::kHeadersSetupRaceFlush[] = "HeadersSetupRace:Flush";
const char ProxyFetch::kHeadersSetupRacePrefix[] = "HeadersSetupRace:";
const char ProxyFetch::kHeadersSetupRaceWait[] = "HeadersSetupRace:Wait";

ProxyFetchFactory::ProxyFetchFactory(ServerContext* server_context)
    : server_context_(server_context),
      timer_(server_context->timer()),
      handler_(server_context->message_handler()),
      outstanding_proxy_fetches_mutex_(
          server_context->thread_system()->NewMutex()) {
}

ProxyFetchFactory::~ProxyFetchFactory() {
  // Factory should outlive all fetches.
  DCHECK(outstanding_proxy_fetches_.empty());
  // Note: access to the set-size is not mutexed but in theory we should
  // be quiesced by this point.
  LOG(INFO) << "ProxyFetchFactory exiting with "
            << outstanding_proxy_fetches_.size()
            << " outstanding requests.";
}

ProxyFetch* ProxyFetchFactory::CreateNewProxyFetch(
    const GoogleString& url_in, AsyncFetch* async_fetch,
    RewriteDriver* driver,
    ProxyFetchPropertyCallbackCollector* property_callback,
    AsyncFetch* original_content_fetch) {
  const GoogleString* url_to_fetch = &url_in;

  // Check whether this an encoding of a non-rewritten resource served
  // from a non-transparently proxied domain.
  UrlNamer* namer = server_context_->url_namer();
  GoogleString decoded_resource;
  GoogleUrl gurl(url_in), request_origin;
  DCHECK(!server_context_->IsPagespeedResource(gurl))
      << "expect ResourceFetch called for pagespeed resources, not ProxyFetch";

  bool cross_domain = false;
  if (gurl.IsWebValid()) {
    if (namer->Decode(gurl, driver->options(), &request_origin,
                      &decoded_resource)) {
      const RewriteOptions* options = driver->options();
      if (namer->IsAuthorized(gurl, *options)) {
        // The URL is proxied, but is not rewritten as a pagespeed resource,
        // so don't try to do the cache-lookup or URL fetch without stripping
        // the proxied portion.
        url_to_fetch = &decoded_resource;
        cross_domain = true;
      } else {
        async_fetch->response_headers()->SetStatusAndReason(
            HttpStatus::kForbidden);
        driver->Cleanup();
        if (property_callback != NULL) {
          property_callback->Detach(HttpStatus::kForbidden);
        }
        async_fetch->Done(false);
        if (original_content_fetch != NULL) {
          original_content_fetch->Done(false);
        }
        return NULL;
      }
    }
  }

  ProxyFetch* fetch = new ProxyFetch(
      *url_to_fetch, cross_domain, property_callback, async_fetch,
      original_content_fetch, driver, server_context_, timer_, this);
  if (cross_domain) {
    // If we're proxying resources from a different domain, the host header is
    // likely set to the proxy host rather than the origin host.  Depending on
    // the origin, this will not work: it will not expect to see
    // the Proxy Host in its headers.
    fetch->request_headers()->RemoveAll(HttpAttributes::kHost);

    // The domain is also supposed to be cookieless, so enforce not
    // sending any cookies to origin, as a precaution against contamination.
    fetch->request_headers()->RemoveAll(HttpAttributes::kCookie);
    fetch->request_headers()->RemoveAll(HttpAttributes::kCookie2);

    // Similarly we don't want to forward authorization, since we may end up
    // forwarding it to wrong host. For proxy-authorization, we remove it here
    // since if our own server implements it, it should do so before touching
    // ProxyInterface, and this prevents it from accidentally leaking.
    // TODO(morlovich): Should we also change 401 and 407 into a 403 on
    // response?
    fetch->request_headers()->RemoveAll(HttpAttributes::kAuthorization);
    fetch->request_headers()->RemoveAll(HttpAttributes::kProxyAuthorization);
  } else {
    // If we didn't already remove all the cookies, remove the experiment
    // ones so we don't confuse the origin.
    experiment::RemoveExperimentCookie(fetch->request_headers());
  }
  RegisterNewFetch(fetch);
  return fetch;
}

void ProxyFetchFactory::StartNewProxyFetch(
    const GoogleString& url_in, AsyncFetch* async_fetch,
    RewriteDriver* driver,
    ProxyFetchPropertyCallbackCollector* property_callback,
    AsyncFetch* original_content_fetch) {
  ProxyFetch* fetch = CreateNewProxyFetch(
      url_in, async_fetch, driver, property_callback, original_content_fetch);
  if (fetch != NULL) {
    fetch->StartFetch();
  }
}

void ProxyFetchFactory::RegisterNewFetch(ProxyFetch* fetch) {
  ScopedMutex lock(outstanding_proxy_fetches_mutex_.get());
  outstanding_proxy_fetches_.insert(fetch);
}

void ProxyFetchFactory::RegisterFinishedFetch(ProxyFetch* fetch) {
  ScopedMutex lock(outstanding_proxy_fetches_mutex_.get());
  outstanding_proxy_fetches_.erase(fetch);
}

ProxyFetchPropertyCallback::ProxyFetchPropertyCallback(
    PageType page_type,
    PropertyCache* property_cache,
    const StringPiece& url,
    const StringPiece& options_signature_hash,
    UserAgentMatcher::DeviceType device_type,
    ProxyFetchPropertyCallbackCollector* collector,
    AbstractMutex* mutex)
    : PropertyPage(
          page_type,
          url,
          options_signature_hash,
          UserAgentMatcher::DeviceTypeSuffix(device_type),
          collector->request_context(),
          mutex,
          property_cache),
      page_type_(page_type),
      collector_(collector) {
}

bool ProxyFetchPropertyCallback::IsCacheValid(int64 write_timestamp_ms) const {
  return collector_->IsCacheValid(write_timestamp_ms);
}

void ProxyFetchPropertyCallback::Done(bool success) {
  collector_->Done(this);
}

ProxyFetchPropertyCallbackCollector::ProxyFetchPropertyCallbackCollector(
    ServerContext* server_context, const StringPiece& url,
    const RequestContextPtr& request_ctx, const RewriteOptions* options,
    UserAgentMatcher::DeviceType device_type)
    : mutex_(server_context->thread_system()->NewMutex()),
      server_context_(server_context),
      sequence_(server_context_->html_workers()->NewSequence()),
      url_(url.data(), url.size()),
      request_context_(request_ctx),
      device_type_(device_type),
      is_options_valid_(true),
      detached_(false),
      done_(false),
      request_headers_ok_(false),
      proxy_fetch_(NULL),
      options_(options),
      status_code_(HttpStatus::kUnknownStatusCode) {
}

ProxyFetchPropertyCallbackCollector::~ProxyFetchPropertyCallbackCollector() {
  ThreadSynchronizer* sync = server_context_->thread_synchronizer();
  server_context_->html_workers()->FreeSequence(sequence_);
  if (!post_lookup_task_vector_.empty()) {
    LOG(DFATAL) << "ProxyFetchPropertyCallbackCollector function vector is not "
                << "empty.";
  }
  STLDeleteElements(&pending_callbacks_);
  STLDeleteValues(&property_pages_);

  // Following sync point is added to make sure that thread in which unit-tests
  // are running will not get finished before deleting
  // ProxyFetchPropertyCallbackCollector.  In production binaries, these are
  // no-op.
  sync->Signal(ProxyFetch::kCollectorFinish);
}

void ProxyFetchPropertyCallbackCollector::AddCallback(
    ProxyFetchPropertyCallback* callback) {
  ScopedMutex lock(mutex_.get());
  pending_callbacks_.insert(callback);
}

PropertyPage* ProxyFetchPropertyCallbackCollector::ReleasePropertyPage(
    ProxyFetchPropertyCallback::PageType page_type) {
  ScopedMutex lock(mutex_.get());
  if (property_pages_.find(page_type) != property_pages_.end()) {
    PropertyPage* page = property_pages_[page_type];
    property_pages_[page_type] = NULL;
    return page;
  }
  return NULL;
}

bool ProxyFetchPropertyCallbackCollector::IsCacheValid(
    int64 write_timestamp_ms) const {
  ScopedMutex lock(mutex_.get());
  // Since PropertyPage::CallDone is not yet called, we know that
  // ProxyFetchPropertyCallbackCollector::Done is not called and hence done_ is
  // false and hence this has not yet been deleted. We can't DCHECK this though
  // since we're not on sequence_.
  // But Detach might have been called already and then options_ is not valid.
  if (!is_options_valid_) {
    return false;
  }
  return (options_ == NULL ||
          options_->IsUrlCacheValid(url_, write_timestamp_ms,
                                    true /* search_wildcards */));
}

// Calls to Done(), RequestHeadersComplete(), ConnectProxyFetch(), and Detach()
// may occur on different threads.  But they are scheduled on a sequence to
// avoid races across these functions.
void ProxyFetchPropertyCallbackCollector::Done(
    ProxyFetchPropertyCallback* callback) {
  ThreadSynchronizer* sync = server_context_->thread_synchronizer();
  sequence_->Add(MakeFunction(
      this, &ProxyFetchPropertyCallbackCollector::ExecuteDone, callback));

  // No class variable is safe to use beyond this point.
  // Used in tests to block the test thread after Done() is called.
  sync->Wait(ProxyFetch::kCollectorDoneFinish);
}

void ProxyFetchPropertyCallbackCollector::ExecuteDone(
    ProxyFetchPropertyCallback* callback) {
  ThreadSynchronizer* sync = server_context_->thread_synchronizer();
  pending_callbacks_.erase(callback);
  property_pages_[callback->page_type()] = callback;
  if (pending_callbacks_.empty()) {
    DCHECK(request_context_.get() != NULL);
    request_context_->mutable_timing_info()->PropertyCacheLookupFinished();
    PropertyPage* actual_page = ReleasePropertyPage(
        ProxyFetchPropertyCallback::kPropertyCachePage);
    if (actual_page != NULL) {
      // TODO(jmarantz): Now that there is no more client property cache,
      // is it necessary to do this test?
      // Compose the primary and fallback property pages into a
      // FallbackPropertyPage, so filters can use the fallback property in the
      // absence of the primary.
      PropertyPage* fallback_page = ReleasePropertyPage(
          ProxyFetchPropertyCallback::kPropertyCacheFallbackPage);
      fallback_property_page_.reset(
          new FallbackPropertyPage(actual_page, fallback_page));
    }

    done_ = true;

    // This should be called only after fallback property page is set because
    // there can be post lookup task which requires fallback_property_page.
    RunPostLookupsAndCleanupIfSafe();
  }

  // No class variable is safe to use beyond this point.
  sync->Signal(ProxyFetch::kCollectorDoneFinish);
}

void ProxyFetchPropertyCallbackCollector::RunPostLookupsAndCleanupIfSafe() {
  if (!done_ || !request_headers_ok_) {
    return;
  }

  for (int i = 0, n = post_lookup_task_vector_.size(); i < n; ++i) {
    post_lookup_task_vector_[i]->CallRun();
  }
  post_lookup_task_vector_.clear();

  if (proxy_fetch_ != NULL) {
    // ConnectProxyFetch() is already called.
    proxy_fetch_->PropertyCacheComplete(this);  // deletes this.
  } else if (detached_) {
    // Detach() is already called.
    UpdateStatusCodeInPropertyCache();
    delete this;
  }
}

void ProxyFetchPropertyCallbackCollector::RequestHeadersComplete() {
  ThreadSynchronizer* sync = server_context_->thread_synchronizer();
  sequence_->Add(MakeFunction(
      this,
      &ProxyFetchPropertyCallbackCollector::ExecuteRequestHeadersComplete));

  // No class variable is safe to use beyond this point.
  // Simulate this method being synchronous in unit tests
  sync->Wait(ProxyFetch::kCollectorRequestHeadersCompleteFinish);
}

void ProxyFetchPropertyCallbackCollector::ExecuteRequestHeadersComplete() {
  ThreadSynchronizer* sync = server_context_->thread_synchronizer();
  request_headers_ok_ = true;
  RunPostLookupsAndCleanupIfSafe();

  // No class variable is safe to use beyond this point.
  sync->Signal(ProxyFetch::kCollectorRequestHeadersCompleteFinish);
}

void ProxyFetchPropertyCallbackCollector::ConnectProxyFetch(
    ProxyFetch* proxy_fetch) {
  ThreadSynchronizer* sync = server_context_->thread_synchronizer();
  sequence_->Add(MakeFunction(
      this,
      &ProxyFetchPropertyCallbackCollector::ExecuteConnectProxyFetch,
      proxy_fetch));
  // Used in tests to block the test thread after ConnectProxyFetch() is called.
  sync->Wait(ProxyFetch::kCollectorConnectProxyFetchFinish);
}

void ProxyFetchPropertyCallbackCollector::ExecuteConnectProxyFetch(
    ProxyFetch* proxy_fetch) {
  DCHECK(proxy_fetch_ == NULL);
  DCHECK(!detached_);
  proxy_fetch_ = proxy_fetch;

  // Use global options in case options is NULL.
  const RewriteOptions* options =
      options_ != NULL ? options_ : server_context_->global_options();

  if (!options->await_pcache_lookup()) {
    std::set<ProxyFetchPropertyCallback*>::iterator iter;
    for (iter = pending_callbacks_.begin(); iter != pending_callbacks_.end();
         ++iter) {
      // Finish all the PropertyCache lookups as soon as possible as origin
      // starts sending content.
      (*iter)->FastFinishLookup();
    }
  }
  ThreadSynchronizer* sync = server_context_->thread_synchronizer();
  if (done_) {
    // Done() is already called.
    proxy_fetch->PropertyCacheComplete(this);  // deletes this.
  }

  // No class variable is safe to use beyond this point.
  sync->Signal(ProxyFetch::kCollectorConnectProxyFetchFinish);
}

void ProxyFetchPropertyCallbackCollector::UpdateStatusCodeInPropertyCache() {
  // If we have not transferred the ownership of PagePropertyCache to
  // ProxyFetch yet, and we have the status code, then write the status_code in
  // PropertyCache.
  AbstractPropertyPage* page = fallback_property_page();
  if (page == NULL || status_code_ == HttpStatus::kUnknownStatusCode) {
    return;
  }
  page->UpdateValue(
      server_context_->dom_cohort(), RewriteDriver::kStatusCodePropertyName,
      IntegerToString(status_code_));
  page->WriteCohort(server_context_->dom_cohort());
}

void ProxyFetchPropertyCallbackCollector::Detach(HttpStatus::Code status_code) {
  ThreadSynchronizer* sync = server_context_->thread_synchronizer();
  {
    ScopedMutex lock(mutex_.get());
    is_options_valid_ = false;
  }
  sequence_->Add(MakeFunction(
      this, &ProxyFetchPropertyCallbackCollector::ExecuteDetach, status_code));
  // Used in tests to block the test thread after Detach() is called.
  sync->Wait(ProxyFetch::kCollectorDetachFinish);
}

void ProxyFetchPropertyCallbackCollector::ExecuteDetach(
    HttpStatus::Code status_code) {
  ThreadSynchronizer* sync = server_context_->thread_synchronizer();
  sync->Wait(ProxyFetch::kCollectorDetachStart);

  DCHECK(!detached_);
  detached_ = true;
  proxy_fetch_ = NULL;
  status_code_ = status_code;

  for (int i = 0, n = post_lookup_task_vector_.size(); i < n; ++i) {
    post_lookup_task_vector_[i]->CallCancel();
  }
  post_lookup_task_vector_.clear();

  if (done_) {
    // Done is already called.
    UpdateStatusCodeInPropertyCache();
    delete this;
  }
  // No class variable is safe to use beyond this point.
  sync->Signal(ProxyFetch::kCollectorDetachFinish);
}

void ProxyFetchPropertyCallbackCollector::AddPostLookupTask(Function* func) {
  sequence_->Add(MakeFunction(
      this,
      &ProxyFetchPropertyCallbackCollector::ExecuteAddPostLookupTask,
      func));
}

void ProxyFetchPropertyCallbackCollector::ExecuteAddPostLookupTask(
    Function* func) {
  DCHECK(!detached_);
  if (done_ && request_headers_ok_) {
    // Already done is called, run the task immediately.
    func->CallRun();
    return;
  }

  // Queue the task.
  post_lookup_task_vector_.push_back(func);
}

ProxyFetch::ProxyFetch(
    const GoogleString& url,
    bool cross_domain,
    ProxyFetchPropertyCallbackCollector* property_cache_callback,
    AsyncFetch* async_fetch,
    AsyncFetch* original_content_fetch,
    RewriteDriver* driver,
    ServerContext* server_context,
    Timer* timer,
    ProxyFetchFactory* factory)
    : SharedAsyncFetch(async_fetch),
      url_(url),
      server_context_(server_context),
      timer_(timer),
      cross_domain_(cross_domain),
      claims_html_(false),
      started_parse_(false),
      parse_text_called_(false),
      done_called_(false),
      property_cache_callback_(property_cache_callback),
      original_content_fetch_(original_content_fetch),
      driver_(driver),
      queue_run_job_created_(false),
      mutex_(server_context->thread_system()->NewMutex()),
      network_flush_outstanding_(false),
      sequence_(NULL),
      done_outstanding_(false),
      finishing_(false),
      done_result_(false),
      waiting_for_flush_to_finish_(false),
      idle_alarm_(NULL),
      factory_(factory),
      distributed_fetch_(false) {
  driver_->SetWriter(async_fetch);
  set_request_headers(async_fetch->request_headers());
  set_response_headers(async_fetch->response_headers());

  // Was this proxy_fetch created on behalf of a distributed rewrite? Note: We
  // don't verify the distributed rewrite key because we want to be conservative
  // about when we apply rewriting.
  if (request_headers()->Has(HttpAttributes::kXPsaDistributedRewriteFetch) ||
      request_headers()->Has(HttpAttributes::kXPsaDistributedRewriteForHtml)) {
    distributed_fetch_ = true;
  }

  DCHECK(driver_->request_headers() != NULL);

  // Set the user agent in the rewrite driver if it is not set already.
  if (driver_->user_agent().empty()) {
    const char* user_agent = request_headers()->Lookup1(
        HttpAttributes::kUserAgent);
    if (user_agent != NULL) {
      VLOG(1) << "Setting user-agent to " << user_agent;
      driver_->SetUserAgent(user_agent);
    } else {
      VLOG(1) << "User-agent empty";
    }
  }

  driver_->EnableBlockingRewrite(request_headers());

  // Set the implicit cache ttl and the min cache ttl for the response headers
  // based on the value specified in the options.
  response_headers()->set_implicit_cache_ttl_ms(
      Options()->implicit_cache_ttl_ms());
  response_headers()->set_min_cache_ttl_ms(
      Options()->min_cache_ttl_ms());

  VLOG(1) << "Attaching RewriteDriver " << driver_
          << " to HtmlRewriter " << this;
}

ProxyFetch::~ProxyFetch() {
  DCHECK(done_called_) << "Callback should be called before destruction";
  DCHECK(!queue_run_job_created_);
  DCHECK(!network_flush_outstanding_);
  DCHECK(!done_outstanding_);
  DCHECK(!waiting_for_flush_to_finish_);
  DCHECK(text_queue_.empty());
  DCHECK(property_cache_callback_ == NULL);
}

bool ProxyFetch::StartParse() {
  // The response headers get munged between when we initially determine
  // which rewrite options we need (in proxy_interface.cc) and here.
  // Therefore, we can not set the Set-Cookie header there, and must
  // do it here instead.
  if (Options()->need_to_store_experiment_data() &&
      Options()->running_experiment()) {
    int experiment_value = Options()->experiment_id();
    server_context_->experiment_matcher()->StoreExperimentData(
        experiment_value, url_,
        server_context_->timer()->NowMs() +
            Options()->experiment_cookie_duration_ms(),
        response_headers());
  }
  driver_->set_response_headers_ptr(response_headers());
  {
    // PropertyCacheComplete checks sequence_ to see whether it should
    // start processing queued text, so we need to mutex-protect it.
    // Often we expect the PropertyCache lookup to complete before
    // StartParse is called, but that is not guaranteed.
    ScopedMutex lock(mutex_.get());
    sequence_ = driver_->html_worker();
  }

  // Start parsing.
  // TODO(sligocki): Allow calling StartParse with GoogleUrl.
  if (!driver_->StartParse(url_)) {
    // We don't expect this to ever fail.
    LOG(ERROR) << "StartParse failed for URL: " << url_;
    return false;
  } else {
    VLOG(1) << "Parse successfully started.";
    return true;
  }
}

const RewriteOptions* ProxyFetch::Options() {
  return driver_->options();
}

void ProxyFetch::HandleHeadersComplete() {
  // If domain rewrite filter is enabled we need to also rewrite the location
  // headers when origin is serving redirects.
  // TODO(matterbury): Consider other 3xx responses.
  // [but note that doing this for 304 Not Modified is probably a dumb idea]
  if (response_headers() != NULL &&
      (response_headers()->status_code() == HttpStatus::kFound ||
       response_headers()->status_code() == HttpStatus::kMovedPermanently)) {
    const char* loc = response_headers()->Lookup1(HttpAttributes::kLocation);
    if (loc != NULL && !driver_->pagespeed_query_params().empty()) {
      GoogleUrl base_url(url_);
      GoogleUrl locn_url(base_url, loc);
      // Only add them back if we're being redirected back to the same domain.
      if (base_url.Origin() == locn_url.Origin()) {
        // TODO(jmarantz): Add a method to GoogleUrl that makes this easy.
        GoogleString new_loc(loc);
        StrAppend(&new_loc, locn_url.has_query() ? "&" : "?",
                  driver_->pagespeed_query_params());
        response_headers()->Replace(HttpAttributes::kLocation, new_loc);
        response_headers()->ComputeCaching();
      }
    }
  }

  // Set or clear sticky option cookies as appropriate.
  if (response_headers() != NULL) {
    GoogleUrl gurl(url_);
    driver_->SetOrClearPageSpeedOptionCookies(gurl, response_headers());
  }

  // Figure out semantic info from response_headers_
  claims_html_ = response_headers()->IsHtmlLike();
  if (original_content_fetch_ != NULL) {
    ResponseHeaders* headers = original_content_fetch_->response_headers();
    headers->CopyFrom(*response_headers());

    if (!server_context_->ProxiesHtml() && claims_html_) {
      LOG(DFATAL) << "Investigate how servers that don't proxy HTML can be "
                     "initiated with original_content_fetch_ non-null";
      headers->SetStatusAndReason(HttpStatus::kForbidden);
    }
    original_content_fetch_->HeadersComplete();
  }

  bool sanitize = cross_domain_;
  if (claims_html_ && !server_context_->ProxiesHtml()) {
    response_headers()->SetStatusAndReason(HttpStatus::kForbidden);
    sanitize = true;
  }

  // Make sure we never serve cookies if the domain we are serving
  // under isn't the domain of the origin.
  if (sanitize) {
    // ... by calling Sanitize to remove them.
    bool changed = response_headers()->Sanitize();
    if (changed) {
      response_headers()->ComputeCaching();
    }
  }

  // We do not call SharedAsyncFetch::HandleHeadersComplete() because
  // we are going to defer propagating headers to the HTTP server
  // infrastructure until we have seen some content.  For example if
  // we may not add a X-PageSpeed header if we don't sniff HTML.
  //
  // Another reason is convert_meta_tags, in which we alter HTTP response
  // headers based on HTML meta-tags.
  //
  // However we want to propagate whether the content size is known to
  // the base fetch.
  PropagateContentLength();
}

void ProxyFetch::AddPagespeedHeader() {
  if (Options()->enabled()) {
    response_headers()->Add(kPageSpeedHeader, Options()->x_header_value());
    response_headers()->ComputeCaching();
  }
}

void ProxyFetch::SetupForHtml() {
  const RewriteOptions* options = Options();

  if (options->enabled() && options->IsAllowed(url_) && !distributed_fetch_) {
    // Note that we guard with distributed_fetch_ to avoid parsing HTML on a
    // distributed task, that's left to the ingress task to do.
    started_parse_ = StartParse();
    if (started_parse_) {
      // TODO(sligocki): Get these in the main flow.
      // Add, remove and update headers as appropriate.
      int64 ttl_ms;
      GoogleString cache_control_suffix;
      if ((options->max_html_cache_time_ms() == 0) ||
          response_headers()->HasValue(
              HttpAttributes::kCacheControl, "no-cache") ||
          response_headers()->HasValue(
              HttpAttributes::kCacheControl, "must-revalidate")) {
        ttl_ms = 0;
        cache_control_suffix = ", no-cache";
        // Preserve values like no-store and no-transform.
        cache_control_suffix +=
            response_headers()->CacheControlValuesToPreserve();
      } else {
        ttl_ms = std::min(options->max_html_cache_time_ms(),
                          response_headers()->cache_ttl_ms());
        // TODO(sligocki): We defensively set Cache-Control: private, but if
        // original HTML was publicly cacheable, we should be able to set
        // the rewritten HTML as publicly cacheable likewise.
        // NOTE: If we do allow "public", we need to deal with other
        // Cache-Control quantifiers, like "proxy-revalidate".
        cache_control_suffix = ", private";
      }

      // When testing, wait a little here for unit tests to make sure
      // we don't race ahead & run filters while we are still cleaning
      // up headers.  When this particular bug is fixed,
      // HeadersComplete will *not* be called on async_fetch_ until
      // after this function returns, so we'd block indefinitely.
      // Instead, block just for 200ms so the test can pass with
      // limited delay.  Note that this is a no-op except in test
      // ProxyInterfaceTest.FiltersRaceSetup which enables thread-sync
      // prefix "HeadersSetupRace:".
      ThreadSynchronizer* sync = server_context_->thread_synchronizer();
      sync->Signal(kHeadersSetupRaceWait);
      sync->TimedWait(kHeadersSetupRaceFlush, kTestSignalTimeoutMs);

      response_headers()->SetDateAndCaching(
          response_headers()->date_ms(), ttl_ms, cache_control_suffix);
      // TODO(sligocki): Support Etags and/or Last-Modified.
      response_headers()->RemoveAll(HttpAttributes::kEtag);
      response_headers()->RemoveAll(HttpAttributes::kLastModified);

      // HTML sizes are likely to be altered by HTML rewriting.
      response_headers()->RemoveAll(HttpAttributes::kContentLength);

      // TODO(sligocki): See mod_instaweb.cc line 528, which strips Expires and
      // Content-MD5.  Perhaps we should do that here as well.
    }
  }
}

void ProxyFetch::StartFetch() {
  factory_->server_context_->rewrite_options_manager()->PrepareRequest(
      Options(),
      &url_,
      request_headers(),
      NewCallback(this, &ProxyFetch::DoFetch));
}

void ProxyFetch::DoFetch(bool prepare_success) {
  if (property_cache_callback_ != NULL) {
    property_cache_callback_->RequestHeadersComplete();
  }

  if (!prepare_success) {
    Done(false);
    return;
  }

  const RewriteOptions* options = driver_->options();
  const bool is_allowed = options->IsAllowed(url_);
  const bool is_enabled = options->enabled();
  {
    ScopedMutex lock(log_record()->mutex());
    if (!is_allowed) {
      log_record()->logging_info()->set_is_url_disallowed(true);
    }
    if (!is_enabled) {
      log_record()->logging_info()->set_is_request_disabled(true);
    }
  }

  if (is_enabled && is_allowed) {
    // Pagespeed enabled on URL.
    if (options->in_place_rewriting_enabled()) {
      // For Ajax rewrites, we go through RewriteDriver to give it
      // a chance to optimize resources. (If they are HTML, it will
      // not touch them, and we will stream them to the parser here).
      driver_->FetchResource(url_, this);
      return;
    }
    // Otherwise we just do a normal fetch from cache, and if it's
    // HTML we will do a streaming rewrite.
  } else {
    // Pagespeed disabled on URL.
    if (options->reject_blacklisted()) {
      // We were asked to error out in this case.
      response_headers()->SetStatusAndReason(
          options->reject_blacklisted_status_code());
      Done(true);
      return;
    } else if (cross_domain_ && !is_allowed) {
      // If we find a cross domain request that is blacklisted, send a 302
      // redirect to the decoded url instead of doing a passthrough.
      response_headers()->Add(HttpAttributes::kLocation, url_);
      response_headers()->SetStatusAndReason(HttpStatus::kFound);
      Done(false);
      return;
    }
    // Else we should do a passthrough. In that case, we still do a normal
    // origin fetch, but we will never rewrite anything, since
    // SetupForHtml() will re-check enabled() and IsAllowed();
  }

  cache_fetcher_.reset(driver_->CreateCacheFetcher());
  // Since we are proxying resources to user, we want to fetch it even if
  // there is a kRecentFetchNotCacheable message in the cache.
  cache_fetcher_->set_ignore_recent_fetch_failed(true);
  cache_fetcher_->Fetch(url_, factory_->handler_, this);
}

void ProxyFetch::ScheduleQueueExecutionIfNeeded() {
  mutex_->DCheckLocked();

  // Already queued -> no need to queue again.
  if (queue_run_job_created_) {
    return;
  }

  // We're waiting for any property-cache lookups and previous flushes to
  // complete, so no need to queue it here.  The queuing will happen when
  // the PropertyCache lookup is complete or from FlushDone.
  if (waiting_for_flush_to_finish_ || (property_cache_callback_ != NULL)) {
    return;
  }

  queue_run_job_created_ = true;
  sequence_->Add(MakeFunction(this, &ProxyFetch::ExecuteQueued));
}

void ProxyFetch::PropertyCacheComplete(
    ProxyFetchPropertyCallbackCollector* callback_collector) {
  driver_->TracePrintf("PropertyCache lookup completed");
  ScopedMutex lock(mutex_.get());

  if (driver_ == NULL) {
    LOG(DFATAL) << "Expected non-null driver.";
  } else {
    // Set the page property and device property objects in the driver.
    driver_->set_fallback_property_page(
        callback_collector->ReleaseFallbackPropertyPage());
    driver_->set_device_type(callback_collector->device_type());
  }
  // We have to set the callback to NULL to let ScheduleQueueExecutionIfNeeded
  // proceed (it waits until it's NULL). And we have to delete it because then
  // we have no reference to it to delete it in Finish.
  if (property_cache_callback_ == NULL) {
    LOG(DFATAL) << "Expected non-null property_cache_callback_.";
  } else {
    delete property_cache_callback_;
    property_cache_callback_ = NULL;
  }
  if (sequence_ != NULL) {
    ScheduleQueueExecutionIfNeeded();
  }
}

bool ProxyFetch::HandleWrite(const StringPiece& str,
                             MessageHandler* message_handler) {
  if (claims_html_ && !server_context_->ProxiesHtml()) {
    return true;
  }

  // TODO(jmarantz): check if the server is being shut down and punt.
  if (original_content_fetch_ != NULL) {
    original_content_fetch_->Write(str, message_handler);
  }

  if (claims_html_ && !html_detector_.already_decided()) {
    if (html_detector_.ConsiderInput(str)) {
      // Figured out whether really HTML or not.
      if (html_detector_.probable_html()) {
        log_record()->SetIsHtml(true);
        if (Options()->max_html_parse_bytes() != 0) {
          SetupForHtml();
        }
      }

      // Now we're done mucking about with headers, add one noting our
      // involvement.
      AddPagespeedHeader();

      if ((property_cache_callback_ != NULL) && started_parse_) {
        // Connect the ProxyFetch in the PropertyCacheCallbackCollector.  This
        // ensures that we will not start executing HTML filters until
        // property cache lookups are complete --- we will keep collecting
        // things into our queue below, but ScheduleQueueExecutionIfNeeded will
        // wait until lookup completed before scheduling the actual parse.
        property_cache_callback_->ConnectProxyFetch(this);
      }

      // If we buffered up any bytes in previous calls, make sure to
      // release them.
      GoogleString buffer;
      html_detector_.ReleaseBuffered(&buffer);
      if (!buffer.empty()) {
        // Recurse on initial buffer of whitespace before processing
        // this call's input below.
        Write(buffer, message_handler);
      }
    } else {
      // Don't know whether HTML or not --- wait for more data.
      return true;
    }
  }

  bool ret = true;
  if (started_parse_) {
    // Buffer up all text & flushes until our worker-thread gets a chance
    // to run. Also split up HTML into manageable chunks if we get a burst,
    // as it will make it easier to insert flushes in between them in
    // ExecuteQueued(), which we want to do in order to limit memory use and
    // latency.
    size_t chunk_size = Options()->flush_buffer_limit_bytes();
    StringStarVector chunks;
    for (size_t pos = 0; pos < str.size(); pos += chunk_size) {
      GoogleString* buffer =
          new GoogleString(str.data() + pos,
                           std::min(chunk_size, str.size() - pos));
      chunks.push_back(buffer);
    }

    {
      ScopedMutex lock(mutex_.get());
      text_queue_.insert(text_queue_.end(), chunks.begin(), chunks.end());
      ScheduleQueueExecutionIfNeeded();
    }
  } else {
    ret = SharedAsyncFetch::HandleWrite(str, message_handler);
  }
  return ret;
}

bool ProxyFetch::HandleFlush(MessageHandler* message_handler) {
  // TODO(jmarantz): check if the server is being shut down and punt.

  if (claims_html_ && !html_detector_.already_decided()) {
    return true;
  }

  bool ret = true;
  if (started_parse_) {
    // Buffer up Flushes for handling in our QueuedWorkerPool::Sequence
    // in ExecuteQueued.  Note that this can re-order Flushes behind
    // pending text, and aggregate together multiple flushes received from
    // the network into one.
    if (Options()->flush_html()) {
      ScopedMutex lock(mutex_.get());
      network_flush_outstanding_ = true;
      ScheduleQueueExecutionIfNeeded();
    }
  } else {
    ret = SharedAsyncFetch::HandleFlush(message_handler);
  }
  return ret;
}

void ProxyFetch::HandleDone(bool success) {
  // TODO(jmarantz): check if the server is being shut down and punt,
  // possibly by calling Finish(false).
  if (original_content_fetch_ != NULL) {
    original_content_fetch_->Done(success);
    // Null the pointer since original_content_fetch_ is not guaranteed to exist
    // beyond this point.
    original_content_fetch_ = NULL;
  }

  bool finish = true;

  if (success) {
    if (claims_html_ && !html_detector_.already_decided()) {
      // This is an all-whitespace document, so we couldn't figure out
      // if it's HTML or not. Handle as pass-through.
      html_detector_.ForceDecision(false /* not html */);
      GoogleString buffered;
      html_detector_.ReleaseBuffered(&buffered);
      AddPagespeedHeader();
      SharedAsyncFetch::HandleHeadersComplete();
      Write(buffered, server_context_->message_handler());
    }
  } else if (!response_headers()->headers_complete()) {
    // This is a fetcher failure, like connection refused, not just an error
    // status code.
    response_headers()->SetStatusAndReason(HttpStatus::kNotFound);
  }

  VLOG(1) << "Fetch result:" << success << " " << url_
          << " : " << response_headers()->status_code();
  if (started_parse_) {
    ScopedMutex lock(mutex_.get());
    done_outstanding_ = true;
    done_result_ = success;
    ScheduleQueueExecutionIfNeeded();
    finish = false;
  }

  if (finish) {
    Finish(success);
  }
}

bool ProxyFetch::IsCachedResultValid(const ResponseHeaders& headers) {
  return OptionsAwareHTTPCacheCallback::IsCacheValid(
      url_, *Options(), request_context(), headers);
}

void ProxyFetch::FlushDone() {
  ScopedMutex lock(mutex_.get());
  DCHECK(waiting_for_flush_to_finish_);
  waiting_for_flush_to_finish_ = false;

  if (!text_queue_.empty() || network_flush_outstanding_ || done_outstanding_) {
    ScheduleQueueExecutionIfNeeded();
  }
}

void ProxyFetch::ExecuteQueued() {
  bool do_flush = false;
  bool do_finish = false;
  bool done_result = false;
  bool force_flush = false;

  size_t buffer_limit = Options()->flush_buffer_limit_bytes();
  StringStarVector v;
  {
    ScopedMutex lock(mutex_.get());
    DCHECK(!waiting_for_flush_to_finish_);

    // See if we should force a flush based on how much stuff has
    // accumulated.
    size_t total = 0;
    size_t force_flush_chunk_count = 0;  // set only if force_flush is true.
    for (size_t c = 0, n = text_queue_.size(); c < n; ++c) {
      total += text_queue_[c]->length();
      if (total >= buffer_limit) {
        force_flush = true;
        force_flush_chunk_count = c + 1;
        break;
      }
    }

    // Are we forcing a flush of some, but not all, of the queued
    // content?
    bool partial_forced_flush =
        force_flush && (force_flush_chunk_count != text_queue_.size());
    if (partial_forced_flush) {
      for (size_t c = 0; c < force_flush_chunk_count; ++c) {
        v.push_back(text_queue_[c]);
      }
      size_t old_len = text_queue_.size();
      text_queue_.erase(text_queue_.begin(),
                        text_queue_.begin() + force_flush_chunk_count);
      DCHECK_EQ(old_len, v.size() + text_queue_.size());

      // Note that in this case, since text_queue_ isn't empty,
      // the call to ScheduleQueueExecutionIfNeeded from FlushDone
      // will make us run again.
    } else {
      v.swap(text_queue_);
    }
    do_flush = network_flush_outstanding_ || force_flush;
    do_finish = done_outstanding_;
    done_result = done_result_;

    network_flush_outstanding_ = false;

    // Note that we don't clear done_outstanding_ here yet, as we
    // can only handle it if we are not also handling a flush.
    queue_run_job_created_ = false;
    if (do_flush) {
      // Stop queuing up invocations of us until the flush we will do
      // below is done.
      waiting_for_flush_to_finish_ = true;
    }
  }

  if (!parse_text_called_) {
    request_context()->mutable_timing_info()->ParsingStarted();
    parse_text_called_ = true;
  }

  // Collect all text received from the fetcher
  for (int i = 0, n = v.size(); i < n; ++i) {
    GoogleString* str = v[i];
    driver_->ParseText(*str);
    delete str;
  }
  if (do_flush) {
    if (force_flush) {
      driver_->RequestFlush();
    }
    if (driver_->flush_requested()) {
      // A flush is about to happen, so we don't want to redundantly
      // flush due to idleness.
      CancelIdleAlarm();
    } else {
      // We will not actually flush, just run through the state-machine, so
      // we want to just advance the idleness timeout.
      QueueIdleAlarm();
    }
    driver_->ExecuteFlushIfRequestedAsync(
        MakeFunction(this, &ProxyFetch::FlushDone));
  } else if (do_finish) {
    CancelIdleAlarm();
    Finish(done_result);
  } else {
    // Advance timeout.
    QueueIdleAlarm();
  }
}

void ProxyFetch::Finish(bool success) {
  ProxyFetchPropertyCallbackCollector* detach_callback = NULL;
  {
    ScopedMutex lock(mutex_.get());
    DCHECK(!waiting_for_flush_to_finish_);
    done_outstanding_ = false;
    finishing_ = true;

    // Avoid holding two locks (this->mutex_ + property_cache_callback_->mutex_)
    // by copying the pointer and detaching after unlocking this->mutex_.
    detach_callback = property_cache_callback_;
    property_cache_callback_ = NULL;
  }
  // The only way detach_callback can be non-NULL here is if the resource isn't
  // being parsed (it's not HTML) and the collector hasn't finished yet, but in
  // that case we never attached the collector to us, so when it's done it won't
  // access us, which is good since we self-delete at the end of this method.
  if (detach_callback != NULL) {
    // Set the status code only for html responses or errors in property cache.
    bool is_response_ok = response_headers()->status_code() == HttpStatus::kOK;
    bool not_html = html_detector_.already_decided() &&
        !html_detector_.probable_html();
    HttpStatus::Code status_code = HttpStatus::kUnknownStatusCode;
    if (!is_response_ok || (claims_html_ && !not_html)) {
      status_code = static_cast<HttpStatus::Code>(
          response_headers()->status_code());
    }
    detach_callback->Detach(status_code);
  }

  if (driver_ != NULL) {
    if (started_parse_) {
      driver_->FinishParseAsync(
          MakeFunction(this, &ProxyFetch::CompleteFinishParse, success));
      return;

    } else {
      // In the unlikely case that StartParse fails (invalid URL?) or the
      // resource is not HTML, we must manually mark the driver for cleanup.
      driver_->Cleanup();
      driver_ = NULL;
    }
  }

  SharedAsyncFetch::HandleDone(success);
  done_called_ = true;
  factory_->RegisterFinishedFetch(this);

  // In ProxyInterfaceTest.HeadersSetupRace, raise a signal that
  // indicates the test functionality is complete.  In other contexts
  // this is a no-op.
  ThreadSynchronizer* sync = server_context_->thread_synchronizer();
  delete this;
  sync->Signal(kHeadersSetupRaceDone);
}

void ProxyFetch::CompleteFinishParse(bool success) {
  driver_ = NULL;
  // Have to call directly -- sequence is gone with driver.
  Finish(success);
}

void ProxyFetch::CancelIdleAlarm() {
  if (idle_alarm_ != NULL) {
    idle_alarm_->CancelAlarm();
    idle_alarm_ = NULL;
  }
}

void ProxyFetch::QueueIdleAlarm() {
  const RewriteOptions* options = Options();
  if (!options->flush_html() || (options->idle_flush_time_ms() <= 0)) {
    return;
  }

  CancelIdleAlarm();
  idle_alarm_ = new QueuedAlarm(
      driver_->scheduler(), sequence_,
      timer_->NowUs() + Options()->idle_flush_time_ms() * Timer::kMsUs,
      MakeFunction(this, &ProxyFetch::HandleIdleAlarm));

  // In ProxyInterfaceTest.HeadersSetupRace, raise a signal that
  // indicates the idle-callback has initiated.  In other contexts
  // this is a no-op.
  ThreadSynchronizer* sync = server_context_->thread_synchronizer();
  sync->Signal(kHeadersSetupRaceAlarmQueued);
}

void ProxyFetch::HandleIdleAlarm() {
  // Clear references to the alarm object as it will be deleted once we exit.
  idle_alarm_ = NULL;

  if (waiting_for_flush_to_finish_ || done_outstanding_ || finishing_) {
    return;
  }

  // Inject an own flush, and queue up its dispatch.
  driver_->ShowProgress("- Flush injected due to input idleness -");
  driver_->RequestFlush();
  Flush(factory_->message_handler());
}

namespace {

PropertyCache::CohortVector GetCohortList(
    bool requires_blink_cohort,
    const ServerContext* server_context) {
  PropertyCache* page_property_cache = server_context->page_property_cache();
  const PropertyCache::CohortVector cohort_list =
      page_property_cache->GetAllCohorts();
  if (requires_blink_cohort) {
    return cohort_list;
  }

  PropertyCache::CohortVector cohort_list_without_blink;
  for (int i = 0, m = cohort_list.size(); i < m; ++i) {
    if (cohort_list[i]->name() == BlinkUtil::kBlinkCohort) {
      continue;
    }
    cohort_list_without_blink.push_back(cohort_list[i]);
  }
  return cohort_list_without_blink;
}

bool UrlMightHavePropertyCacheEntry(const GoogleUrl& url) {
  const ContentType* type = NameExtensionToContentType(url.LeafSansQuery());
  if (type == NULL) {
    // No extension or unknown; could be HTML:
    //   http://www.example.com/
    //   http://www.example.com/index
    //   http://www.example.com/index.php
    return true;
  }

  // Use a complete switch-statement rather than type()->IsHtmlLike()
  // so that every time we add a new content-type we make an explicit
  // decision about whether it should induce a pcache read.
  //
  // TODO(jmarantz): currently this returns false for ".txt".  Thus we will
  // do no optimizations relying on property-cache on HTML files ending with
  // ".txt".  We should determine whether this is the right thing or not.
  switch (type->type()) {
    case ContentType::kHtml:
    case ContentType::kXhtml:
    case ContentType::kCeHtml:
      return true;
    case ContentType::kJavascript:
    case ContentType::kCss:
    case ContentType::kText:
    case ContentType::kXml:
    case ContentType::kPng:
    case ContentType::kGif:
    case ContentType::kJpeg:
    case ContentType::kSwf:
    case ContentType::kWebp:
    case ContentType::kIco:
    case ContentType::kPdf:
    case ContentType::kOther:
    case ContentType::kJson:
    case ContentType::kSourceMap:
    case ContentType::kVideo:
    case ContentType::kAudio:
    case ContentType::kOctetStream:
      return false;
  }
  LOG(DFATAL) << "URL " << url.Spec() << ": unexpected type:" << type->type()
              << "; " << type->mime_type() << "; " << type->file_extension();
  return false;
}

}  // namespace

ProxyFetchPropertyCallbackCollector*
    ProxyFetchFactory::InitiatePropertyCacheLookup(
        const bool is_resource_fetch,
        const GoogleUrl& request_url,
        ServerContext* server_context,
        RewriteOptions* options,
        AsyncFetch* async_fetch,
        const bool requires_blink_cohort,
        bool* added_page_property_callback) {
  RequestContextPtr request_ctx = async_fetch->request_context();
  DCHECK(request_ctx.get() != NULL);
  if (request_ctx->root_trace_context() != NULL) {
    request_ctx->root_trace_context()->TracePrintf(
        "PropertyCache lookup start");
  }
  StringPiece user_agent =
      async_fetch->request_headers()->Lookup1(HttpAttributes::kUserAgent);
  UserAgentMatcher::DeviceType device_type =
      server_context->user_agent_matcher()->GetDeviceTypeForUA(user_agent);

  scoped_ptr<ProxyFetchPropertyCallbackCollector> callback_collector(
      new ProxyFetchPropertyCallbackCollector(
          server_context, request_url.Spec(), request_ctx, options,
          device_type));
  bool added_callback = false;
  PropertyPageStarVector property_callbacks;

  ProxyFetchPropertyCallback* property_callback = NULL;
  ProxyFetchPropertyCallback* fallback_property_callback = NULL;
  PropertyCache* page_property_cache = server_context->page_property_cache();
  if (!is_resource_fetch &&
      server_context->page_property_cache()->enabled() &&
      UrlMightHavePropertyCacheEntry(request_url) &&
      async_fetch->request_headers()->method() == RequestHeaders::kGet) {
    GoogleString options_signature_hash;
    if (options != NULL) {
      server_context->ComputeSignature(options);
      options_signature_hash =
          server_context->GetRewriteOptionsSignatureHash(options);
    }
    AbstractMutex* mutex = server_context->thread_system()->NewMutex();
    property_callback = new ProxyFetchPropertyCallback(
        ProxyFetchPropertyCallback::kPropertyCachePage,
        page_property_cache,
        request_url.Spec(),
        options_signature_hash,
        device_type,
        callback_collector.get(),
        mutex);
    callback_collector->AddCallback(property_callback);
    added_callback = true;
    if (added_page_property_callback != NULL) {
      *added_page_property_callback = true;
    }
    // Trigger property cache lookup for the requests which contains query param
    // as cache key without query params. The result of this lookup will be used
    // if actual property page does not contains property value.
    if (options != NULL &&
        options->use_fallback_property_cache_values()) {
      GoogleString fallback_page_url;
      if (request_url.PathAndLeaf() != "/" &&
          !request_url.PathAndLeaf().empty()) {
        // Don't bother looking up fallback properties for the root, "/", since
        // there is nothing to fall back to.
        fallback_page_url =
            FallbackPropertyPage::GetFallbackPageUrl(request_url);
      }

      if (!fallback_page_url.empty()) {
        fallback_property_callback =
            new ProxyFetchPropertyCallback(
                ProxyFetchPropertyCallback::kPropertyCacheFallbackPage,
                page_property_cache,
                fallback_page_url,
                options_signature_hash,
                device_type,
                callback_collector.get(),
                server_context->thread_system()->NewMutex());
        callback_collector->AddCallback(fallback_property_callback);
      }
    }
  }

  // All callbacks need to be registered before Reads to avoid race.
  PropertyCache::CohortVector cohort_list_without_blink =
      GetCohortList(false /* requires_blink_cohort */, server_context);
  if (property_callback != NULL) {
    page_property_cache->ReadWithCohorts(
        requires_blink_cohort ?
            GetCohortList(
                true /* requires_blink_cohort */, server_context) :
            cohort_list_without_blink,
            property_callback);
  }

  if (fallback_property_callback != NULL) {
    // Always read property page with fallback values without blink as there is
    // no property in BlinkCohort which can used fallback values.
    page_property_cache->ReadWithCohorts(cohort_list_without_blink,
                                         fallback_property_callback);
  }

  if (added_callback) {
    request_ctx->mutable_timing_info()->PropertyCacheLookupStarted();
  } else {
    callback_collector.reset(NULL);
  }
  return callback_collector.release();
}

}  // namespace net_instaweb
