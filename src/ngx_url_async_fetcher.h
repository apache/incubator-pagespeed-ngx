#ifndef NET_INSTAWEB_NGX_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_NGX_URL_ASYNC_FETCHER_H_

extern "C" {
  #include <ngx_config.h>
  #include <ngx_core.h>
}

#include <vector>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/pool.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/http/public/url_pollable_async_fetcher.h"


namespace net_instaweb {

class AsyncFetch;
class MessageHandler;
class Statistics;
class NgxFetch;
class Variable;

class NgxUrlAsyncFetcher : public UrlPollableAsyncFetcher {
  public:
    // add timeout event
    NgxUrlAsyncFetcher(const char* proxy, ngx_pool_t* pool, int64 timeout,
        int64 resolver_timeout, int64 fetch_timeout, ngx_resolver_t* resolver,
        MessageHandler* handler);
    NgxUrlAsyncFetcher(const char* proxy, ngx_log_t* log, int64 timeout,
        int64 resolver_timeout, int64 fetch_timeout, ngx_resolver_t* resolver,
        MessageHandler* handler);
    NgxUrlAsyncFetcher(NgxUrlAsyncFetcher *parent, const char* proxy);

    ~NgxUrlAsyncFetcher();

    // create pool, add the total timeout timer
    bool Init(); 
    
    // shut down all the fetchers.
    virtual void ShutDown(); 

    virtual bool SupportsHttps();

    virtual bool Fetch(const GoogleString& url,
                       MessageHandler* message_handler,
                       AsyncFetch* callback);

    bool StartFetch(NgxFetch* fetch);
    void FetchComplete(NgxFetch* fetch);

    void PrintActiveFetches(MessageHandler* handler) const;
    
    // change the original content legnth or not
    bool track_original_content_length() {
      return track_original_content_length_;
    }
    void set_track_original_content_length(bool x) {
      track_original_content_length_ = x;
    }
 
    // vector
    typedef Pool<NgxFetch> NgxFetchPool;

    virtual bool AnyPendingFetches() {
      return !active_fetches_.empty();
    }
    // number of active fetches
    int ApproximateNumActiveFetches() {
      return active_fetches_.size();
    }

    void CancelActiveFetches();

    // remove the error fetches
    //void CleanupFetchesWithErrors();

    bool shutdown() const {return shutdown_; }
    void set_shutdown(bool s) {shutdown_ = s; }

  private:
    static void TimeoutHandler(ngx_event_t* tev);
    friend class NgxFetch;

    NgxFetchPool completed_fetches_;
    NgxFetchPool active_fetches_;
    ngx_url_t url_;

    int fetchers_count_;
    bool shutdown_;
    bool track_original_content_length_;
    int64 byte_count_;
    int64 timeout_;
    MessageHandler* message_handler_;

    ngx_event_t* timeout_event_;
    ngx_pool_t* pool_;
    ngx_log_t* log_;
    ngx_resolver_t* resolver_;
    int64 resolver_timeout_;
    int64 fetch_timeout_;

    DISALLOW_COPY_AND_ASSIGN(NgxUrlAsyncFetcher);
};
} // net_instaweb

#endif
