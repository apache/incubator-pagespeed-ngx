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

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/inflating_fetch.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/response_headers_parser.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/pool.h"
#include "net/instaweb/util/public/pool_element.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {
  NgxUrlAsyncFetcher::NgxUrlAsyncFetcher(const char* proxy,
                                         ngx_pool_t* pool,
                                         int64 resolver_timeout, // timer for resolver
                                         int64 fetch_timeout, // timer for fetch
                                         ngx_resolver_t* resolver,
                                         MessageHandler* handler)
    : fetchers_count_(0),
      shutdown_(false),
      track_original_content_length_(false),
      byte_count_(0),
      message_handler_(handler),
      resolver_timeout_(resolver_timeout),
      fetch_timeout_(fetch_timeout) {
    ngx_memzero(&url_, sizeof(ngx_url_t));
    url_.url.data = (u_char*)(proxy);
    url_.url.len = ngx_strlen(proxy);
    pool_ = pool;
    log_ = pool->log;
    command_connection_ = NULL;
    pipe_fd_ = 0;
    resolver_ = resolver;
  }

  NgxUrlAsyncFetcher::NgxUrlAsyncFetcher(const char* proxy,
                                         ngx_log_t* log,
                                         int64 resolver_timeout,
                                         int64 fetch_timeout,
                                         ngx_resolver_t* resolver,
                                         MessageHandler* handler)
    : fetchers_count_(0),
      shutdown_(false),
      track_original_content_length_(false),
      byte_count_(0),
      message_handler_(handler),
      resolver_timeout_(resolver_timeout),
      fetch_timeout_(fetch_timeout) {
    ngx_memzero(&url_, sizeof(url_));
    if (proxy != NULL && *proxy != '\0') {
      url_.url.data = (u_char *)(proxy);
      url_.url.len = ngx_strlen(proxy);
    }
    log_ = log;
    pool_ = NULL;
    command_connection_ = NULL;
    pipe_fd_ = 0;
    resolver_ = resolver;
  }

  NgxUrlAsyncFetcher::NgxUrlAsyncFetcher(NgxUrlAsyncFetcher* parent,
                                         char* proxy)
    : fetchers_count_(parent->fetchers_count_),
      shutdown_(false),
      track_original_content_length_(parent->track_original_content_length_),
      byte_count_(parent->byte_count_),
      message_handler_(parent->message_handler_),
      resolver_timeout_(parent->resolver_timeout_),
      fetch_timeout_(parent->fetch_timeout_) {
    ngx_memzero(&url_, sizeof(ngx_url_t));
    if (proxy != NULL && *proxy != '\0') {
      url_.url.data = reinterpret_cast<u_char*>(proxy);
      url_.url.len = ngx_strlen(proxy);
    }
    pool_ = parent->pool_;
    log_ = parent->log_;
    resolver_ = parent->resolver_;
    command_connection_ = NULL;
    pipe_fd_ = 0;
  }

  NgxUrlAsyncFetcher::~NgxUrlAsyncFetcher() {
    CancelActiveFetches();
    completed_fetches_.DeleteAll();
    active_fetches_.DeleteAll();
    ngx_destroy_pool(pool_);
  }

  // If there are still active requests, cancel them.
  void NgxUrlAsyncFetcher::CancelActiveFetches() {
    for (NgxFetchPool::const_iterator p = active_fetches_.begin(),
        e = active_fetches_.end(); p != e; ++p) {
      NgxFetch* fetch = *p;
      fetch->CallbackDone(false);
    }
  }

  // Create the pool for fetcher, create the pipe, add the read event for main
  // thread. It should be called in the worker process.
  bool NgxUrlAsyncFetcher::Init() {
    if (pool_ == NULL) {
      pool_ = ngx_create_pool(4096, log_);
      if (pool_ == NULL) {
        ngx_log_error(NGX_LOG_ERR, log_, 0,
            "NgxUrlAsyncFetcher::Init create pool failed");
        return false;
      }
    }

    int pipe_fds[2];
    int rc = pipe(pipe_fds);
    if (rc != 0) {
      ngx_log_error(NGX_LOG_ERR, log_, 0, "pipe() failed");
      return false;
    }
    if (ngx_nonblocking(pipe_fds[0]) == -1) {
      ngx_log_error(NGX_LOG_ERR, log_, 0, "nonblocking pipe[0] failed");
      return false;
    }

    if (ngx_nonblocking(pipe_fds[1]) == -1) {
      ngx_log_error(NGX_LOG_ERR, log_, 0, "nonblocking pipe[1] failed");
      return false;
    }

    pipe_fd_ = pipe_fds[0];
    command_connection_ = ngx_get_connection(pipe_fds[1], log_);
    if (command_connection_ == NULL) {
      close(pipe_fds[1]);
      close(pipe_fds[0]);
      return false;
    }

    command_connection_->recv = ngx_recv;
    command_connection_->send = ngx_send;
    command_connection_->recv_chain = ngx_recv_chain;
    command_connection_->send_chain = ngx_send_chain;
    command_connection_->log = log_;
    command_connection_->read->log = log_;
    command_connection_->write->log = log_;
    command_connection_->data = this;
    command_connection_->read->handler = CommandHandler;
    ngx_add_event(command_connection_->read, NGX_READ_EVENT, 0);

    if (url_.url.len == 0) {
      return true;
    }
    if (ngx_parse_url(pool_, &url_) != NGX_OK) {
      ngx_log_error(NGX_LOG_ERR, log_, 0,
          "NgxUrlAsyncFetcher::Init parse proxy[%V] failed", &url_.url);
      return false;
    }
    return true;
  }

  void NgxUrlAsyncFetcher::ShutDown() {
      shutdown_ = true;
      SendCmd('S');
  }

  // It's called in the Fetcher thread. All the fetches are started at
  // this function. It will notify the main thread to start the fetch job.
  void NgxUrlAsyncFetcher::Fetch(const GoogleString& url,
                                 MessageHandler* message_handler,
                                 AsyncFetch* async_fetch) {
    async_fetch = EnableInflation(async_fetch);
    NgxFetch* fetch = new NgxFetch(url, async_fetch,
          message_handler, fetch_timeout_);
    pending_fetches_.Add(fetch);
    SendCmd('F');
  }

  // send command to nginx main thread
  // 'F' : start a fetch
  // 'S' : shutdown the fetcher
  bool NgxUrlAsyncFetcher::SendCmd(const char command) {
    int rc;
    while (true) {
      rc = write(pipe_fd_, &command, 1);
      if (rc == 1) {
        return true;
      } else if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        // TODO(junmin): It's rare. But it need be fixed.
      } else {
        return false;
      }
    }
    return true;
  }

  // This is the read event which is called in the main thread.
  // It will do the real work. Add the work event and start the fetch.
  void NgxUrlAsyncFetcher::CommandHandler(ngx_event_t *cmdev) {
    char command;
    int rc;
    ngx_connection_t* c = static_cast<ngx_connection_t*>(cmdev->data);
    NgxUrlAsyncFetcher* fetcher = static_cast<NgxUrlAsyncFetcher*>(c->data);
    do {
      rc = read(fetcher->pipe_fd_, &command, 1);
    } while (rc == -1 && errno == EINTR);

    CHECK(rc == -1 || rc == 0 || rc == 1);

    if (rc == -1 || rc == 0) {
      // EAGAIN
      return;
    }

    NgxFetchPool::const_iterator p, e;
    NgxFetch* fetch;
    switch (command) {
      // All the new fetches are appended in the pending_fetches.
      // Start all these fetches.
      case 'F':
        if (!fetcher->pending_fetches_.empty()) {
          for (p = fetcher->active_fetches_.begin(),
              e = fetcher->pending_fetches_.end(); p != e; p++) {
            fetch = *p;
            fetcher->StartFetch(fetch);
          }
          fetcher->pending_fetches_.DeleteAll();
        }
        CHECK(ngx_handle_read_event(cmdev, 0) == NGX_OK);
        break;

      // Shutdown all the fetches.
      case 'S':
        if (!fetcher->pending_fetches_.empty()) {
          fetcher->pending_fetches_.DeleteAll();
        }

        if (!fetcher->completed_fetches_.empty()) {
          fetcher->completed_fetches_.DeleteAll();
        }

        if (!fetcher->active_fetches_.empty()) {
          for (p = fetcher->active_fetches_.begin(),
               e = fetcher->active_fetches_.end(); p != e; p++) {
            fetch = *p;
            fetch->CallbackDone(false);
          }
          fetcher->active_fetches_.DeleteAll();
        }
        CHECK(ngx_del_event(cmdev, NGX_READ_EVENT, 0) == NGX_OK);
        break;

      default:
        break;
    }

    return;
  }

  bool NgxUrlAsyncFetcher::StartFetch(NgxFetch* fetch) {
    bool started = !shutdown_ && fetch->Start(this);
    if (started) {
      active_fetches_.Add(fetch);
      fetchers_count_++;
    } else {
      LOG(WARNING) << "Fetch failed to start: " << fetch->str_url();
      fetch->CallbackDone(false);
      delete fetch;
    }
    return started;
  }

  void NgxUrlAsyncFetcher::FetchComplete(NgxFetch* fetch) {
    active_fetches_.Remove(fetch);
    completed_fetches_.Add(fetch);
    fetch->message_handler()->Message(kInfo, "Fetch complete:%s",
                    fetch->str_url());
    // TODO(junmin): count the time_duration_ms_
    byte_count_ += fetch->bytes_received();
    fetchers_count_--;
  }

  void NgxUrlAsyncFetcher::PrintActiveFetches(MessageHandler* handler) const {
    for (NgxFetchPool::const_iterator p = active_fetches_.begin(),
        e = active_fetches_.end(); p != e; ++p) {
      NgxFetch* fetch = *p;
      handler->Message(kInfo, "Active fetch: %s", fetch->str_url());
    }
  }
} // namespace net_instaweb
