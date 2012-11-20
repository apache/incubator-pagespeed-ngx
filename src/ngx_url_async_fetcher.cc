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
                                         int64 timeout, // timer for fetcher
                                         int64 resolver_timeout, // timer for resolver
                                         int64 fetch_timeout, // timer for fetch
                                         ngx_resolver_t* resolver,
                                         MessageHandler* handler)
    : fetchers_count_(0),
      shutdown_(false),
      track_original_content_length_(false),
      byte_count_(0),
      timeout_(timeout),
      message_handler_(handler),
      resolver_timeout_(resolver_timeout),
      fetch_timeout_(fetch_timeout) {
    ngx_memzero(&url_, sizeof(ngx_url_t));
    url_.url.data = (u_char*)proxy;
    url_.url.len = ngx_strlen(proxy);
    pool_ = pool;
    log_ = pool->log;
    resolver_ = resolver;
  }

  NgxUrlAsyncFetcher::NgxUrlAsyncFetcher(const char* proxy,
                                         ngx_log_t* log,
                                         int64 timeout,
                                         int64 resolver_timeout,
                                         int64 fetch_timeout,
                                         ngx_resolver_t* resolver,
                                         MessageHandler* handler)
    : fetchers_count_(0),
      shutdown_(false),
      track_original_content_length_(false),
      byte_count_(0),
      timeout_(timeout),
      message_handler_(handler),
      resolver_timeout_(resolver_timeout),
      fetch_timeout_(fetch_timeout) {
    ngx_memzero(&url_, sizeof(ngx_url_t));
    if (proxy != NULL && *proxy != '\0') {
      url_.url.data = (u_char*)proxy;
      url_.url.len = ngx_strlen(proxy);
    }
    log_ = log;
    resolver_ = resolver;
  }

  NgxUrlAsyncFetcher::NgxUrlAsyncFetcher(NgxUrlAsyncFetcher* parent,
                                         const char* proxy)
    : fetchers_count_(parent->fetchers_count_),
      shutdown_(false),
      track_original_content_length_(parent->track_original_content_length_),
      byte_count_(parent->byte_count_),
      timeout_(parent->timeout_),
      message_handler_(parent->message_handler_),
      resolver_timeout_(parent->resolver_timeout_),
      fetch_timeout_(parent->fetch_timeout_) {
    ngx_memzero(&url_, sizeof(ngx_url_t));
    if (proxy != NULL && *proxy != '\0') {
      url_.url.data = (u_char*)proxy;
      url_.url.len = ngx_strlen(proxy);
    }
    pool_ = parent->pool_;
    log_ = parent->log_;
    resolver_ = parent->resolver_;
  }

  NgxUrlAsyncFetcher::~NgxUrlAsyncFetcher() {
    CancelActiveFetches();
    completed_fetches_.DeleteAll();
    active_fetches_.DeleteAll();
    ngx_destroy_pool(pool_);
  }

  void NgxUrlAsyncFetcher::CancelActiveFetches() {
    for (NgxFetchPool::const_iterator p = active_fetches_.begin(),
        e = active_fetches_.end(); p != e; ++p) {
      NgxFetch* fetch = *p;
      fetch->CallbackDone(false);
    }
  }

  bool NgxUrlAsyncFetcher::Init() {
    if (pool_ == NULL) {
      pool_ = ngx_create_pool(4096, log_);
      if (pool_ == NULL) {
        ngx_log_error(NGX_LOG_ERR, log_, 0,
            "NgxUrlAsyncFetcher::Init create pool failed");
        return false;
      }
    }

    timeout_event_ = static_cast<ngx_event_t*>(
        ngx_pcalloc(pool_, sizeof(ngx_event_t)));
    if (timeout_event_ == NULL) {
      ngx_log_error(NGX_LOG_ERR, log_, 0,
          "NgxUrlAsyncFetcher::Init calloc timeout_event_ failed");
      return false;
    }

    ngx_add_timer(timeout_event_, static_cast<ngx_msec_t>(timeout_));
    timeout_event_->handler = TimeoutHandler;
    timeout_event_->data = this;
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

  void NgxUrlAsyncFetcher::TimeoutHandler(ngx_event_t* tev) {
    NgxUrlAsyncFetcher* fetcher = static_cast<NgxUrlAsyncFetcher*>(tev->data);
    fetcher->ShutDown();
  }

  void NgxUrlAsyncFetcher::ShutDown() {
    for (NgxFetchPool::const_iterator p = active_fetches_.begin(),
            e = active_fetches_.end(); p != e; ++p) {
      NgxFetch* fetch = *p;
      fetch->CallbackDone(false);
    }
    if (timeout_event_->timer_set) {
      ngx_del_timer(timeout_event_);
    }
    shutdown_ = true;
  }

  bool NgxUrlAsyncFetcher::Fetch(const GoogleString& url,
                                 MessageHandler* message_handler,
                                 AsyncFetch* async_fetch) {
    async_fetch = EnableInflation(async_fetch);
    NgxFetch* fetch = new NgxFetch(url, async_fetch,
          message_handler, fetch_timeout_);
    return StartFetch(fetch);
  }

  bool NgxUrlAsyncFetcher::StartFetch(NgxFetch* fetch) {
    bool started = !shutdown_ && fetch->Start(this);
    if (started) {
      active_fetches_.Add(fetch);
      fetchers_count_++;
    } else {
      //Start fetch failed
      fetch->CallbackDone(false);
    }
    return started;
  }

  void NgxUrlAsyncFetcher::FetchComplete(NgxFetch* fetch) {
    active_fetches_.Remove(fetch);
    completed_fetches_.Add(fetch);
    fetch->message_handler()->Message(kInfo, "Fetch complete:%s",
                    fetch->str_url());
    //count time_duration_ms_
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
