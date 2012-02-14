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

#include "base/logging.h"
#include "net/instaweb/http/public/cache_url_async_fetcher.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/furious_util.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/queued_alarm.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

ProxyFetchFactory::ProxyFetchFactory(ResourceManager* manager)
    : manager_(manager),
      timer_(manager->timer()),
      handler_(manager->message_handler()),
      outstanding_proxy_fetches_mutex_(manager->thread_system()->NewMutex()) {
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

void ProxyFetchFactory::StartNewProxyFetch(
    const GoogleString& url_in, AsyncFetch* async_fetch,
    RewriteOptions* custom_options,
    ProxyFetchPropertyCallback* property_callback) {
  const GoogleString* url_to_fetch = &url_in;

  // Check whether this an encoding of a non-rewritten resource served
  // from a non-transparently proxied domain.
  UrlNamer* namer = manager_->url_namer();
  GoogleString decoded_resource;
  GoogleUrl gurl(url_in), request_origin;
  DCHECK(!manager_->IsPagespeedResource(gurl))
      << "expect ResourceFetch called for pagespeed resources, not ProxyFetch";

  bool cross_domain = false;
  if (gurl.is_valid()) {
    if (namer->Decode(gurl, &request_origin, &decoded_resource)) {
      const RewriteOptions* options = (custom_options == NULL)
          ? manager_->global_options()
          : custom_options;
      if (namer->IsAuthorized(gurl, *options)) {
        // The URL is proxied, but is not rewritten as a pagespeed resource,
        // so don't try to do the cache-lookup or URL fetch without stripping
        // the proxied portion.
        url_to_fetch = &decoded_resource;
        cross_domain = true;
      } else {
        async_fetch->response_headers()->SetStatusAndReason(
            HttpStatus::kForbidden);
        if (custom_options != NULL) {
          delete custom_options;
        }
        async_fetch->Done(false);
        if (property_callback != NULL) {
          property_callback->Detach();
        }
        return;
      }
    }
  }

  ProxyFetch* fetch = new ProxyFetch(
      *url_to_fetch, cross_domain, property_callback, async_fetch,
      custom_options, manager_, timer_, this);
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
  } else {
    // If we didn't already remove all the cookies, remove the furious
    // ones so we don't confuse the origin.
    furious::RemoveFuriousCookie(fetch->request_headers());
  }
  Start(fetch);
  fetch->StartFetch();
}

void ProxyFetchFactory::Start(ProxyFetch* fetch) {
  ScopedMutex lock(outstanding_proxy_fetches_mutex_.get());
  outstanding_proxy_fetches_.insert(fetch);
}

void ProxyFetchFactory::Finish(ProxyFetch* fetch) {
  ScopedMutex lock(outstanding_proxy_fetches_mutex_.get());
  outstanding_proxy_fetches_.erase(fetch);
}

ProxyFetchPropertyCallback::ProxyFetchPropertyCallback(AbstractMutex* mutex)
    : PropertyPage(mutex),
      mutex_(mutex),
      detached_(false),
      done_(false),
      success_(false),
      proxy_fetch_(NULL) {
}

// Calls to Done(), SetProxyFetch(), and Detach() may occur on
// different threads.  Exactly one of SetProxyFetch and Detach will
// never race with each other, as they correspond to the construction
// or destruction of ProxyFetch, but either can race with Done().  Note
// that SetProxyFetch can be followed by Detach if it turns out that
// a URL without a known extension is *not* HTML.  See
// ProxyInterfaceTest.PropCacheNoWritesIfNonHtmlDelayedCache.
//
// TODO(jmarantz): Add unit-tests to capture the 4 orderings, if
// not the potential races.

void ProxyFetchPropertyCallback::Done(bool success) {
  // Note that the proxy_fetch is created while the property-cache
  // lookup is in progress, so we must lock access to the proxy_fetch_.
  ProxyFetch* fetch = NULL;
  bool do_delete = false;
  {
    ScopedMutex lock(mutex_);
    success_ = success;
    done_ = true;
    fetch = proxy_fetch_;
    do_delete = detached_;
  }
  if (fetch != NULL) {
    fetch->PropertyCacheComplete(this, success);  // Transfers ownership.
  } else if (do_delete) {
    delete this;
  }
}

void ProxyFetchPropertyCallback::SetProxyFetch(ProxyFetch* proxy_fetch) {
  bool ready = false;
  {
    ScopedMutex lock(mutex_);
    DCHECK(proxy_fetch_ == NULL);
    DCHECK(!detached_);
    proxy_fetch_ = proxy_fetch;
    ready = done_;
  }
  if (ready) {
    proxy_fetch->PropertyCacheComplete(this, success_);  // Transfers ownership.
  }
}

void ProxyFetchPropertyCallback::Detach() {
  bool do_delete = false;
  {
    ScopedMutex lock(mutex_);
    proxy_fetch_ = NULL;
    DCHECK(!detached_);
    detached_ = true;
    do_delete = done_;
  }
  if (do_delete) {
    delete this;
  }
}

ProxyFetch::ProxyFetch(const GoogleString& url,
                       bool cross_domain,
                       ProxyFetchPropertyCallback* property_cache_callback,
                       AsyncFetch* async_fetch,
                       RewriteOptions* custom_options,
                       ResourceManager* manager,
                       Timer* timer,
                       ProxyFetchFactory* factory)
    : SharedAsyncFetch(async_fetch),
      url_(url),
      resource_manager_(manager),
      timer_(timer),
      cross_domain_(cross_domain),
      claims_html_(false),
      started_parse_(false),
      done_called_(false),
      start_time_us_(0),
      property_cache_callback_(property_cache_callback),
      queue_run_job_created_(false),
      mutex_(manager->thread_system()->NewMutex()),
      network_flush_outstanding_(false),
      sequence_(NULL),
      done_outstanding_(false),
      finishing_(false),
      done_result_(false),
      waiting_for_flush_to_finish_(false),
      idle_alarm_(NULL),
      factory_(factory),
      prepare_success_(false) {
  set_request_headers(async_fetch->request_headers());
  set_response_headers(async_fetch->response_headers());

  // TODO(nforman): If we are not running an experiment, remove the
  // furious cookie.
  // If we don't already have custom options, and the global options
  // say we're running furious, then clone them into custom_options so we
  // can manipulate custom options without affecting the global options.
  const RewriteOptions* global_options = resource_manager_->global_options();
  if (custom_options == NULL && global_options->running_furious()) {
    custom_options = global_options->Clone();
  }

  // Set RewriteDriver.
  if (custom_options == NULL) {
    driver_ = resource_manager_->NewRewriteDriver();
  } else {
    // NewCustomRewriteDriver takes ownership of custom_options_.
    if (custom_options->running_furious()) {
      furious::FuriousState furious_value;
      if (!furious::GetFuriousCookieState(async_fetch->request_headers(),
                                          &furious_value)) {
        furious_value = furious::DetermineFuriousState(custom_options);
      }
      custom_options->set_furious_state(furious_value);
      // If this request is on the 'B' side of the experiment, turn off
      // all the rewriters except the ones we need to do the experiment.
      // TODO(nforman): Allow the configuration to specify what the 'B'
      // set of filters should be.
      if (custom_options->furious_state() == furious::kFuriousB) {
        furious::FuriousNoFilterDefault(custom_options);
      }
    }
    resource_manager_->ComputeSignature(custom_options);
    driver_ = resource_manager_->NewCustomRewriteDriver(custom_options);
  }

  // Now that we've created the RewriteDriver, include the client_id generated
  // from the original request headers, if any.
  const char* client_id = async_fetch->request_headers()->Lookup1(
      HttpAttributes::kXGooglePagespeedClientId);
  if (client_id != NULL) {
    driver_->set_client_id(client_id);
  }

  // TODO(sligocki): We should pass the options in with the AsyncFetch rather
  // than creating a new fetcher for each fetch.
  // Note: CacheUrlAsyncFetcher is actually a pretty light class, so this isn't
  // terrible for performance, just seems like bad programming practice.
  cache_fetcher_.reset(new CacheUrlAsyncFetcher(
      resource_manager_->http_cache(), resource_manager_->url_async_fetcher()));
  cache_fetcher_->set_respect_vary(Options()->respect_vary());
  cache_fetcher_->set_ignore_recent_fetch_failed(true);
  cache_fetcher_->set_default_cache_html(Options()->default_cache_html());
  cache_fetcher_->set_backend_first_byte_latency_histogram(
      resource_manager_->rewrite_stats()->backend_latency_histogram());
  cache_fetcher_->set_fallback_responses_served(
      resource_manager_->rewrite_stats()->fallback_responses_served());
  cache_fetcher_->set_num_conditional_refreshes(
      resource_manager_->rewrite_stats()->num_conditional_refreshes());
  cache_fetcher_->set_serve_stale_if_fetch_error(
      Options()->serve_stale_if_fetch_error());

  // TODO(sligocki): Make complete request header available to filters.
  const char* cookies = request_headers()->Lookup1(HttpAttributes::kCookie);
  if (cookies != NULL) {
    driver_->set_cookies(cookies);
  }

  const char* user_agent = request_headers()->Lookup1(
      HttpAttributes::kUserAgent);
  if (user_agent != NULL) {
    VLOG(1) << "Setting user-agent to " << user_agent;
    driver_->set_user_agent(user_agent);
  } else {
    VLOG(1) << "User-agent empty";
  }

  // Set the implicit cache ttl for the response headers based on the value
  // specified in the options.
  response_headers()->set_implicit_cache_ttl_ms(
      Options()->implicit_cache_ttl_ms());

  VLOG(1) << "Attaching RewriteDriver " << driver_
          << " to HtmlRewriter " << this;

  if (property_cache_callback != NULL) {
    property_cache_callback->SetProxyFetch(this);
  }
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
  driver_->SetWriter(base_fetch());
  if (Options()->running_furious()) {
    furious::FuriousState state = Options()->furious_state();
    // The "0" string is for an experiment id.
    // TODO(nforman): Replace "0" with a configurable id.
    furious::SetFuriousCookie(response_headers(), "0", state, url_.c_str(),
                              resource_manager_->timer()->NowUs());
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
  // Figure out semantic info from response_headers_
  claims_html_ = response_headers()->IsHtmlLike();

  // Make sure we never serve cookies if the domain we are serving
  // under isn't the domain of the origin.
  if (cross_domain_) {
    // ... by calling Sanitize to remove them.
    bool changed = response_headers()->Sanitize();
    if (changed) {
      response_headers()->ComputeCaching();
    }
  }
}

void ProxyFetch::AddPagespeedHeader() {
  if (Options()->enabled()) {
    response_headers()->Add(kPageSpeedHeader, factory_->server_version());
  }
}

void ProxyFetch::SetupForHtml() {
  const RewriteOptions* options = Options();
  if (options->enabled() && options->IsAllowed(url_)) {
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
        // We don't want to add no-store unless we have to.
        // TODO(sligocki): Stop special-casing no-store, just preserve all
        // Cache-Control identifiers except for restricting max-age and
        // private/no-cache level.
        if (response_headers()->HasValue(
                HttpAttributes::kCacheControl, "no-store")) {
          cache_control_suffix += ", no-store";
        }
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
      response_headers()->SetDateAndCaching(
          response_headers()->date_ms(), ttl_ms, cache_control_suffix);
      // TODO(sligocki): Support Etags and/or Last-Modified.
      response_headers()->RemoveAll(HttpAttributes::kEtag);
      response_headers()->RemoveAll(HttpAttributes::kLastModified);
      start_time_us_ = resource_manager_->timer()->NowUs();

      // HTML sizes are likely to be altered by HTML rewriting.
      response_headers()->RemoveAll(HttpAttributes::kContentLength);

      // TODO(sligocki): See mod_instaweb.cc line 528, which strips Expires and
      // Content-MD5.  Perhaps we should do that here as well.
    }
  }
}

void ProxyFetch::StartFetch() {
  factory_->manager_->url_namer()->PrepareRequest(
      Options(), &url_, request_headers(), &prepare_success_,
      MakeFunction(this, &ProxyFetch::DoFetch),
      factory_->handler_);
}

void ProxyFetch::DoFetch() {
  if (prepare_success_) {
    if (driver_->options()->enabled() &&
        driver_->options()->ajax_rewriting_enabled() &&
        driver_->options()->IsAllowed(url_)) {
      driver_->set_async_fetcher(cache_fetcher_.get());
      driver_->FetchResource(url_, this);
    } else {
      cache_fetcher_->Fetch(url_, factory_->handler_, this);
    }
  } else {
    Done(false);
  }
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

void ProxyFetch::PropertyCacheComplete(PropertyPage* property_page,
                                       bool success) {
  ScopedMutex lock(mutex_.get());
  if (property_cache_callback_ == NULL) {
    // Already called Finish and abandoned callback.
    delete property_page;
  } else {
    property_cache_callback_ = NULL;
    if (driver_ == NULL) {
      LOG(DFATAL) << "Expected non-null driver.";
      delete property_page;
    } else {
      driver_->set_property_page(property_page);
    }
    if (sequence_ != NULL) {
      ScheduleQueueExecutionIfNeeded();
    }
  }
}

bool ProxyFetch::HandleWrite(const StringPiece& str,
                             MessageHandler* message_handler) {
  // TODO(jmarantz): check if the server is being shut down and punt.

  if (claims_html_ && !html_detector_.already_decided()) {
    if (html_detector_.ConsiderInput(str)) {
      // Figured out whether really HTML or not.
      if (html_detector_.probable_html()) {
        SetupForHtml();
      }

      // Now we're done mucking about with headers, add one noting our
      // involvement.
      AddPagespeedHeader();

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
    // to run.  This will re-order pending flushes after already-received
    // html, so that if the html is coming in faster than we can process it,
    // then we'll perform fewer flushes.

    GoogleString* buffer = new GoogleString(str.data(), str.size());
    {
      ScopedMutex lock(mutex_.get());
      text_queue_.push_back(buffer);
      ScheduleQueueExecutionIfNeeded();
    }
  } else {
    // Pass other data (css, js, images) directly to http writer.
    ret = base_fetch()->Write(str, message_handler);
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
    ret = base_fetch()->Flush(message_handler);
  }
  return ret;
}

void ProxyFetch::HandleDone(bool success) {
  // TODO(jmarantz): check if the server is being shut down and punt,
  // possibly by calling Finish(false).

  bool finish = true;

  if (success) {
    if (claims_html_ && !html_detector_.already_decided()) {
      // This is an all-whitespace document, so we couldn't figure out
      // if it's HTML or not. Handle as pass-through.
      html_detector_.ForceDecision(false /* not html */);
      GoogleString buffered;
      html_detector_.ReleaseBuffered(&buffered);
      AddPagespeedHeader();
      base_fetch()->HeadersComplete();
      Write(buffered, resource_manager_->message_handler());
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
  return headers.IsDateLaterThan(Options()->cache_invalidation_timestamp());
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
  StringStarVector v;
  {
    ScopedMutex lock(mutex_.get());
    DCHECK(!waiting_for_flush_to_finish_);
    v.swap(text_queue_);
    do_flush = network_flush_outstanding_;
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

  // Collect all text received from the fetcher
  for (int i = 0, n = v.size(); i < n; ++i) {
    GoogleString* str = v[i];
    driver_->ParseText(*str);
    delete str;
  }
  if (do_flush) {
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
  ProxyFetchPropertyCallback* detach_callback = NULL;
  {
    ScopedMutex lock(mutex_.get());
    DCHECK(!waiting_for_flush_to_finish_);
    done_outstanding_ = false;
    finishing_ = true;

    if (property_cache_callback_ != NULL) {
      // Avoid holding two locks (this->mutex_ and
      // property_cache_callback_->mutex_) by copying the
      // pointer and detaching after unlocking the this->mutex_.
      detach_callback = property_cache_callback_;
      property_cache_callback_ = NULL;
    }
  }
  if (detach_callback != NULL) {
    detach_callback->Detach();
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

  if (started_parse_ && success) {
    RewriteStats* stats = resource_manager_->rewrite_stats();
    stats->rewrite_latency_histogram()->Add(
        (timer_->NowUs() - start_time_us_) / 1000.0);
    stats->total_rewrite_count()->IncBy(1);
  }

  base_fetch()->Done(success);
  done_called_ = true;
  factory_->Finish(this);

  delete this;
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

}  // namespace net_instaweb
