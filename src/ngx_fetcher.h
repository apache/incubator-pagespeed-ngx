#ifndef NET_INSTAWEB_NGX_FETCHER_H_
#define NET_INSTAWEB_NGX_FETCHER_H_

extern "C" {
  #include <ngx_config.h>
  #include <ngx_core.h>
  #include <ngx_http.h>
}

#include <vector>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/pool.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/http/public/url_pollable_async_fetcher.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/response_headers_parser.h"

namespace net_instaweb {
  class NgxUrlAsyncFetcher;
  class NgxFetch {
    public:
      NgxFetch(const GoogleString& url,
               AsyncFetch* async_fetch,
               MessageHandler* message_handler,
               int64 timeout_ms);
      ~NgxFetch();

      // add the connnection, parse url, connect, add write event, 
      // add read event, init fetcher_
      bool Start(NgxUrlAsyncFetcher* fetcher);
      // completed url, for logging
      const char* str_url();
      // timeout or cancel by force
      void Cancel();
      // finish this task
      void CallbackDone(bool success);

      // show the bytes received
      size_t bytes_received();
      int64 fetch_start_ms();
      void set_fetch_start_ms(int64 start_ms);
      int64 fetch_end_ms();
      void set_fetch_end_ms(int64 end_ms);
       // show the bytes received
      MessageHandler* message_handler();

    private:

      // functions used in event callback

      bool ParseUrl();
      
      static void NgxFecthResolveDone(ngx_resolver_ctx_t *ctx);
      
      // create request, connect
      int InitRequest();

      // handler of write event
      static void NgxFecthWrite(ngx_event_t *wev);

      // handler of read event
      static void NgxFecthRead(ngx_event_t *rev);

      // cancel the fetch;
      static void NgxFetchTimeout(ngx_event_t *tev);

      // add pagespeed user-agent
      void FixUserAgent(); 

      const GoogleString str_url_;
      NgxUrlAsyncFetcher* fetcher_;
      ResponseHeadersParser parser_;
      MessageHandler* message_handler_;
      size_t bytes_received_;
      int64 fetch_start_ms_;
      int64 fetch_end_ms_;

      ngx_event_t read_event_;
      ngx_event_t write_event_;
      ngx_event_t timeout_event_;
      ngx_connection_t* connection_;
      
      DISALLOW_COPY_AND_ASSIGN(NgxFetch);
  };
} // namespace net_instaweb
#endif
