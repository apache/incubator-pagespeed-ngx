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
    NgxUrlAsyncFetcher(const char* proxy, ngx_pool_t* pool,
        int64 timeout_ms, MessageHandler *handler);
    // log used by creating pool_
    NgxUrlAsyncFetcher(const char* proxy, ngx_log_t* log,
        int64 timeout_ms, MessageHandler *handler);
    NgxUrlAsyncFetcher(NgxUrlAsyncFetcher *parent, const char* proxy);

    ~NgxUrlAsyncFetcher();

    // create pool, add the total timeout timer
    int Init(); 
    
    bool SetupProxy(const char* proxy);

    // shut down all the fetchers.
    virtual void ShutDown(); 

    virtual bool SupportsHttps();

    virtual void Fetch(const GoogleString& url,
                       MessageHandler* message_handler,
                       AsyncFetch *callback);

    bool StartFetch(NgxFetch* fetch);
    void FetchComplete(NgxFetch* fetch);

    void PrintActiveFetches(MessageHandler* handler) const;
    
    // change the origin content legnth or not
    bool track_original_content_length(); 
    void set_track_original_content_length(bool);
 
    // vector
    typedef Pool<NgxFetch> NgxFetchPool;

    virtual bool AnyPendingFetches();
    // number of active fetches
    int ApproximateNumActiveFetches();

    void CancelActiveFetches();
    // just remove the error fetches
    void CleanupFetchesWithErrors();

    bool shutdown() const {return shutdown_; }
    void set_shutdown(bool s) {shutdown_ = s; }

  private:
    friend class NgxFetch;

    NgxFetchPool completed_fetches_;
    NgxFetchPool active_fetches_;

    int fetchers_count_;
    bool shutdown_;
    bool track_original_content_length_;
    int64 byte_count_;
    MessageHandler* message_handler_;

    ngx_event_t* timeout_;
    ngx_pool_t* pool_;

    DISALLOW_COPY_AND_ASSIGN(NgxUrlAsyncFetcher);
};
}

#endif
