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

// Author: x.dinic@gmail.com (Junmin Xiong)

extern "C" {
  #include <ngx_http.h>
  #include <ngx_core.h>
}

#include "ngx_url_async_fetcher.h"
#include "ngx_fetch.h"

#include <vector>
#include <algorithm>
#include <string>
#include <list>
#include <map>
#include <set>

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/inflating_fetch.h"
#include "net/instaweb/public/version.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/pool.h"
#include "pagespeed/kernel/base/pool_element.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/writer.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/response_headers_parser.h"

namespace net_instaweb {

  NgxUrlAsyncFetcher::NgxUrlAsyncFetcher(const char* proxy,
                                         ngx_log_t* log,
                                         ngx_msec_t resolver_timeout,
                                         ngx_msec_t fetch_timeout,
                                         ngx_resolver_t* resolver,
                                         int max_keepalive_requests,
                                         ThreadSystem* thread_system,
                                         MessageHandler* handler)
    : fetchers_count_(0),
      shutdown_(false),
      track_original_content_length_(false),
      byte_count_(0),
      thread_system_(thread_system),
      message_handler_(handler),
      mutex_(NULL),
      max_keepalive_requests_(max_keepalive_requests),
      event_connection_(NULL) {
    resolver_timeout_ = resolver_timeout;
    fetch_timeout_ = fetch_timeout;
    ngx_memzero(&proxy_, sizeof(proxy_));
    if (proxy != NULL && *proxy != '\0') {
      proxy_.url.data = reinterpret_cast<u_char*>(const_cast<char*>(proxy));
      proxy_.url.len = ngx_strlen(proxy);
    }
    mutex_ = thread_system_->NewMutex();
    log_ = log;
    pool_ = NULL;
    resolver_ = resolver;
    // If init fails, set shutdown_ so no fetches will be attempted.
    if (!Init(const_cast<ngx_cycle_t*>(ngx_cycle))) {
      shutdown_ = true;
      message_handler_->Message(
          kError, "NgxUrlAsyncFetcher failed to init, fetching disabled.");
    }
  }

  NgxUrlAsyncFetcher::~NgxUrlAsyncFetcher() {
    DCHECK(shutdown_)  << "Shut down before destructing NgxUrlAsyncFetcher.";
    message_handler_->Message(
        kInfo,
        "Destruct NgxUrlAsyncFetcher with [%d] active fetchers",
        ApproximateNumActiveFetches());

    CancelActiveFetches();
    active_fetches_.DeleteAll();
    NgxConnection::Terminate();

    if (pool_ != NULL) {
      ngx_destroy_pool(pool_);
      pool_ = NULL;
    }
    if (mutex_ != NULL) {
      delete mutex_;
      mutex_ = NULL;
    }
  }


  bool NgxUrlAsyncFetcher::ParseUrl(ngx_url_t* url, ngx_pool_t* pool) {
    size_t scheme_offset;
    u_short port;
    if (ngx_strncasecmp(url->url.data, reinterpret_cast<u_char*>(
                                     const_cast<char*>("http://")), 7) == 0) {
      scheme_offset = 7;
      port = 80;
    } else if (ngx_strncasecmp(url->url.data, reinterpret_cast<u_char*>(
                                    const_cast<char*>("https://")), 8) == 0) {
      scheme_offset = 8;
      port = 443;
    } else {
      scheme_offset = 0;
      port = 80;
    }

    url->url.data += scheme_offset;
    url->url.len -= scheme_offset;
    url->default_port = port;
    // See: http://lxr.evanmiller.org/http/source/core/ngx_inet.c#L875
    url->no_resolve = 0;
    url->uri_part = 1;

    if (ngx_parse_url(pool, url) == NGX_OK) {
      return true;
    }
    return false;
  }

  // If there are still active requests, cancel them.
  void NgxUrlAsyncFetcher::CancelActiveFetches() {
    // TODO(oschaaf): this seems tricky, this may end up calling
    // FetchComplete, modifying the active fetches while we are looping
    // it
    for (NgxFetchPool::const_iterator p = active_fetches_.begin(),
        e = active_fetches_.end(); p != e; ++p) {
      NgxFetch* fetch = *p;
      fetch->CallbackDone(false);
    }
  }

  // Create the pool for fetcher, create the pipe, add the read event for main
  // thread. It should be called in the worker process.
  bool NgxUrlAsyncFetcher::Init(ngx_cycle_t* cycle) {
    log_ = cycle->log;
    CHECK(event_connection_ == NULL) << "event connection already set";
    event_connection_ = new NgxEventConnection(ReadCallback);
    if (!event_connection_->Init(cycle)) {
      return false;
    }
    if (pool_ == NULL) {
      pool_ = ngx_create_pool(4096, log_);
      if (pool_ == NULL) {
        ngx_log_error(NGX_LOG_ERR, log_, 0,
            "NgxUrlAsyncFetcher::Init create pool failed");
        return false;
      }
    }

    if (proxy_.url.len == 0) {
      return true;
    }

    // TODO(oschaaf): shouldn't we do this earlier? Do we need to clean
    // up when parsing the url failed?
    if (!ParseUrl(&proxy_, pool_)) {
      ngx_log_error(NGX_LOG_ERR, log_, 0,
          "NgxUrlAsyncFetcher::Init parse proxy[%V] failed", &proxy_.url);
      return false;
    }
    return true;
  }

  void NgxUrlAsyncFetcher::ShutDown() {
    shutdown_ = true;
    if (!pending_fetches_.empty()) {
      for (Pool<NgxFetch>::iterator p = pending_fetches_.begin(),
           e = pending_fetches_.end(); p != e; p++) {
        NgxFetch* fetch = *p;
        fetch->CallbackDone(false);
      }
      pending_fetches_.DeleteAll();
    }

    if (!active_fetches_.empty()) {
      for (Pool<NgxFetch>::iterator p = active_fetches_.begin(),
           e = active_fetches_.end(); p != e; p++) {
        NgxFetch* fetch = *p;
        fetch->CallbackDone(false);
      }
      active_fetches_.Clear();
    }
    if (event_connection_ != NULL) {
      event_connection_->Shutdown();
      delete event_connection_;
      event_connection_ = NULL;
    }
  }

  // It's called in the rewrite thread. All the fetches are started at
  // this function. It will notify the main thread to start the fetch job.
  void NgxUrlAsyncFetcher::Fetch(const GoogleString& url,
                                 MessageHandler* message_handler,
                                 AsyncFetch* async_fetch) {
    // Don't accept new fetches when shut down. This flow is also entered when
    // we did not initialize properly in ::Init().
    if (shutdown_) {
      async_fetch->Done(false);
      return;
    }
    async_fetch = EnableInflation(async_fetch);
    NgxFetch* fetch = new NgxFetch(url, async_fetch,
          message_handler, log_);
    ScopedMutex lock(mutex_);
    pending_fetches_.Add(fetch);

    // TODO(oschaaf): thread safety on written vs shutdown.
    // It is possible that shutdown() is called after writing an event? In that
    // case, this could (rarely) fail when it shouldn't.
    bool written = event_connection_->WriteEvent(this);
    CHECK(written || shutdown_) << "NgxUrlAsyncFetcher: event write failure";
  }

  // This is the read event which is called in the main thread.
  // It will do the real work. Add the work event and start the fetch.
  void NgxUrlAsyncFetcher::ReadCallback(const ps_event_data& data) {
    std::vector<NgxFetch*> to_start;
    NgxUrlAsyncFetcher* fetcher = reinterpret_cast<NgxUrlAsyncFetcher*>(
      data.sender);

    fetcher->mutex_->Lock();
    fetcher->completed_fetches_.DeleteAll();

    for (Pool<NgxFetch>::iterator p = fetcher->pending_fetches_.begin(),
             e = fetcher->pending_fetches_.end(); p != e; p++) {
      NgxFetch* fetch = *p;
      to_start.push_back(fetch);
    }

    fetcher->pending_fetches_.Clear();
    fetcher->mutex_->Unlock();

    for (size_t i = 0; i < to_start.size(); i++) {
      fetcher->StartFetch(to_start[i]);
    }

    return;
  }

  // TODO(oschaaf): return value is ignored.
  bool NgxUrlAsyncFetcher::StartFetch(NgxFetch* fetch) {
    mutex_->Lock();
    active_fetches_.Add(fetch);
    fetchers_count_++;
    mutex_->Unlock();

    // Don't initiate the fetch when we are shutting down
    if (shutdown_) {
      fetch->CallbackDone(false);
      return false;
    }

    bool started = fetch->Start(this);

    if (!started) {
      message_handler_->Message(kWarning, "Fetch failed to start: %s",
                                fetch->str_url());
      fetch->CallbackDone(false);
    }

    return started;
  }

  void NgxUrlAsyncFetcher::FetchComplete(NgxFetch* fetch) {
    ScopedMutex lock(mutex_);
    byte_count_ += fetch->bytes_received();
    fetchers_count_--;
    active_fetches_.Remove(fetch);
    completed_fetches_.Add(fetch);
  }

  void NgxUrlAsyncFetcher::PrintActiveFetches(MessageHandler* handler) const {
    for (NgxFetchPool::const_iterator p = active_fetches_.begin(),
        e = active_fetches_.end(); p != e; ++p) {
      NgxFetch* fetch = *p;
      handler->Message(kInfo, "Active fetch: %s", fetch->str_url());
    }
  }
}  // namespace net_instaweb
