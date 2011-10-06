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

#include "net/instaweb/http/public/cache_url_async_fetcher.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/queued_worker.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

ProxyFetchFactory::ProxyFetchFactory(ResourceManager* manager)
    : manager_(manager),
      timer_(manager->timer()),
      handler_(manager->message_handler()),
      cache_fetcher_(new CacheUrlAsyncFetcher(manager->http_cache(),
                                              manager->url_async_fetcher())),
      outstanding_proxy_fetches_mutex_(manager->thread_system()->NewMutex()) {
  cache_fetcher_->set_ignore_recent_fetch_failed(true);
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
    const GoogleString& url, const RequestHeaders& request_headers,
    RewriteOptions* custom_options, ResponseHeaders* response_headers,
    Writer* base_writer, UrlAsyncFetcher::Callback* callback) {
  ProxyFetch* fetch = new ProxyFetch(url, request_headers, custom_options,
                                     response_headers, base_writer,
                                     manager_, timer_, callback, this);
  Start(fetch);
  cache_fetcher_->Fetch(url, request_headers, fetch->response_headers_,
                        handler_, fetch);
}

void ProxyFetchFactory::Start(ProxyFetch* fetch) {
  ScopedMutex lock(outstanding_proxy_fetches_mutex_.get());
  outstanding_proxy_fetches_.insert(fetch);
}

void ProxyFetchFactory::Finish(ProxyFetch* fetch) {
  ScopedMutex lock(outstanding_proxy_fetches_mutex_.get());
  outstanding_proxy_fetches_.erase(fetch);
}

ProxyFetch::ProxyFetch(const GoogleString& url,
                       const RequestHeaders& request_headers,
                       RewriteOptions* custom_options,
                       ResponseHeaders* response_headers,
                       Writer* base_writer,
                       ResourceManager* manager,
                       Timer* timer, UrlAsyncFetcher::Callback* callback,
                       ProxyFetchFactory* factory)
    : url_(url),
      response_headers_(response_headers),
      base_writer_(base_writer),
      resource_manager_(manager),
      timer_(timer),
      callback_(callback),
      pass_through_(true),
      started_parse_(false),
      start_time_us_(0),
      custom_options_(custom_options),
      driver_(NULL),  // Needs to be set in StartParse.
      queue_run_job_created_(false),
      mutex_(manager->thread_system()->NewMutex()),
      network_flush_outstanding_(false),
      sequence_(NULL),
      done_outstanding_(false),
      done_result_(false),
      waiting_for_flush_to_finish_(false),
      factory_(factory) {
  if (const char* ua = request_headers.Lookup1(HttpAttributes::kUserAgent)) {
    request_user_agent_.reset(new GoogleString(ua));
  }
}

ProxyFetch::~ProxyFetch() {
  DCHECK(callback_ == NULL) << "Callback should be called before destruction";
  DCHECK(!queue_run_job_created_);
  DCHECK(!network_flush_outstanding_);
  DCHECK(!done_outstanding_);
  DCHECK(!waiting_for_flush_to_finish_);
  DCHECK(text_queue_.empty());
}

bool ProxyFetch::StartParse() {
  DCHECK(driver_ == NULL);

  // Set RewriteDriver.
  if (custom_options_ == NULL) {
    driver_ = resource_manager_->NewRewriteDriver();
  } else {
    // NewCustomRewriteDriver takes ownership of custom_options_.
    driver_ =
        resource_manager_->NewCustomRewriteDriver(custom_options_.release());
  }
  LOG(INFO) << "Attaching RewriteDriver " << driver_
            << " to HtmlRewriter " << this;

  driver_->SetWriter(base_writer_);
  sequence_ = driver_->html_worker();
  driver_->set_response_headers_ptr(response_headers_);

  // Start parsing.
  // TODO(sligocki): Allow calling StartParse with GoogleUrl.
  if (!driver_->StartParse(url_)) {
    // We don't expect this to ever fail.
    LOG(ERROR) << "StartParse failed for URL: " << url_;
    return false;
  } else {
    if (request_user_agent_.get() != NULL) {
      LOG(INFO) << "Setting user-agent to " << *request_user_agent_;
      driver_->set_user_agent(*request_user_agent_);
    } else {
      LOG(INFO) << "User-agent empty";
    }
    LOG(INFO) << "Parse successfully started.";
    return true;
  }
}

bool ProxyFetch::IsHtml() {
  CHECK(response_headers_ != NULL);
  ConstStringStarVector v;
  bool html = (response_headers_->Lookup(HttpAttributes::kContentType, &v) &&
               (v.size() == 1) && (v[0] != NULL) &&
               (strstr(v[0]->data(), "text/html") != NULL));
  if (html) {
    LOG(INFO) << "Response appears to be HTML. Rewriting content.";
  } else {
    LOG(INFO) << "Response appears not to be HTML. Passing content through.";
  }
  return html;
}

const RewriteOptions* ProxyFetch::Options() {
  // If driver_ is not yet constructed, we need to use the ResoruceManager's
  // default options or custom options supplied to us.
  // However, if driver_ has been constructed, then custom_options gets
  // reset to NULL, so the logic here is a bit complicated.
  if (driver_ != NULL) {
    return driver_->options();
  } else if (custom_options_.get() != NULL) {
    return custom_options_.get();
  } else {
    return resource_manager_->global_options();
  }
}

void ProxyFetch::HeadersComplete() {
  // Figure out semantic info from response_headers_.
  DCHECK(pass_through_);  // default until HTML detected.

  // TODO(sligocki): Get these in the main flow.
  // Add, remove and update headers as appropriate.
  const RewriteOptions* options = Options();
  if (IsHtml() && options->enabled()) {
    started_parse_ = StartParse();
    if (started_parse_) {
      pass_through_ = false;
      int64 ttl_ms;
      GoogleString cache_control_suffix;
      if ((options->max_html_cache_time_ms() == 0) ||
          response_headers_->HasValue(
              HttpAttributes::kCacheControl, "no-cache") ||
          response_headers_->HasValue(
              HttpAttributes::kCacheControl, "must-revalidate")) {
        ttl_ms = 0;
        cache_control_suffix = ", no-cache, no-store";
      } else {
        ttl_ms = std::min(options->max_html_cache_time_ms(),
                          response_headers_->cache_ttl_ms());
        // TODO(sligocki): We defensively set Cache-Control: private, but if
        // original HTML was publicly cacheable, we should be able to set
        // the rewritten HTML as publicly cacheable likewise.
        // NOTE: If we do allow "public", we need to deal with other
        // Cache-Control quantifiers, like "proxy-revalidate".
        cache_control_suffix = ", private";
      }
      response_headers_->SetDateAndCaching(
          response_headers_->fetch_time_ms(), ttl_ms, cache_control_suffix);
      // TODO(sligocki): Support Etags.
      response_headers_->RemoveAll(HttpAttributes::kEtag);
      start_time_us_ = resource_manager_->timer()->NowUs();

      // HTML sizes are likely to be altered by HTML rewriting.
      response_headers_->RemoveAll(HttpAttributes::kContentLength);

      // TODO(sligocki): see mod_instaweb.cc line 528, which strips
      // Expires, Last-Modified and Content-MD5.  Perhaps we should
      // do that here as well.

      response_headers_->Add(kPageSpeedHeader, factory_->server_version());
    }
  }
}

void ProxyFetch::ScheduleQueueExecutionIfNeeded() {
  mutex_->DCheckLocked();

  // Already queued -> no need to queue again.
  if (queue_run_job_created_) {
    return;
  }

  // We're waiting for previous flushes -> no need to queue it here,
  // will happen from FlushDone.
  if (waiting_for_flush_to_finish_) {
    return;
  }

  queue_run_job_created_ = true;
  sequence_->Add(MakeFunction(this, &ProxyFetch::ExecuteQueued));
}

bool ProxyFetch::Write(const StringPiece& str,
                       MessageHandler* message_handler) {
  // TODO(jmarantz): check if the server is being shut down and punt.
  bool ret = true;
  if (!pass_through_) {
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
    ret = base_writer_->Write(str, message_handler);
  }
  return ret;
}

bool ProxyFetch::Flush(MessageHandler* message_handler) {
  // TODO(jmarantz): check if the server is being shut down and punt.

  bool ret = true;
  if (!pass_through_) {
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
    ret = base_writer_->Flush(message_handler);
  }
  return ret;
}

void ProxyFetch::Done(bool success) {
  // TODO(jmarantz): check if the server is being shut down and punt,
  // possibly by calling Finish(false).

  bool finish = true;
  if (success) {
    LOG(INFO) << "Fetch succeeded " << url_
              << " : " << response_headers_->status_code();
    if (!pass_through_) {
      ScopedMutex lock(mutex_.get());
      done_outstanding_ = true;
      done_result_ = success;
      ScheduleQueueExecutionIfNeeded();
      finish = false;
    }
  } else {
    // This is a fetcher failure, like connection refused, not just an error
    // status code.
    response_headers_->SetStatusAndReason(HttpStatus::kNotFound);
  }

  if (finish) {
    Finish(success);
  }
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
    driver_->ExecuteFlushIfRequestedAsync(
        MakeFunction(this, &ProxyFetch::FlushDone));
  } else if (do_finish) {
    Finish(done_result);
  }
}


void ProxyFetch::Finish(bool success) {
  {
    ScopedMutex lock(mutex_.get());
    DCHECK(!waiting_for_flush_to_finish_);
    done_outstanding_ = false;
  }

  if (driver_ != NULL) {
    if (started_parse_) {
      driver_->FinishParseAsync(
        MakeFunction(this, &ProxyFetch::CompleteFinishParse, success));
      return;

    } else {
      // In the unlikely case that StartParse fails (invalid URL?)
      // we must manually release driver_ (FinishParse usually does this).
      resource_manager_->ReleaseRewriteDriver(driver_);
      driver_ = NULL;
    }
  }

  if (!pass_through_ && success) {
    RewriteStats* stats = resource_manager_->rewrite_stats();
    stats->rewrite_latency_histogram()->Add(
        (timer_->NowUs() - start_time_us_) / 1000.0);
    stats->total_rewrite_count()->IncBy(1);
  }

  callback_->Done(success);
  callback_ = NULL;
  factory_->Finish(this);

  delete this;
}

void ProxyFetch::CompleteFinishParse(bool success) {
  driver_ = NULL;
  // Have to call directly -- sequence is gone with driver.
  Finish(success);
}

}  // namespace net_instaweb
