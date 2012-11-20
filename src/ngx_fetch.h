#ifndef NET_INSTAWEB_NGX_FETCHER_H_
#define NET_INSTAWEB_NGX_FETCHER_H_

extern "C" {
  #include <ngx_config.h>
  #include <ngx_core.h>
  #include <ngx_http.h>

  typedef bool (*response_handler_pt)(ngx_connection_t* c);
}

#include "ngx_url_async_fetcher.h"
#include <vector>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/pool.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/http/public/url_pollable_async_fetcher.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/response_headers_parser.h"

namespace net_instaweb {
  class NgxStatusLine;
  class NgxUrlAsyncFetcher;
  class NgxFetch : public PoolElement<NgxFetch> {
    public:
      NgxFetch(const GoogleString& url,
               AsyncFetch* async_fetch,
               MessageHandler* message_handler,
               int64 timeout_ms);
      ~NgxFetch();

      // add the connnection, parse url, connect, add write event, 
      bool Init();
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
      void bytes_received_add(int64 x);
      int64 fetch_start_ms();
      void set_fetch_start_ms(int64 start_ms);
      int64 fetch_end_ms();
      void set_fetch_end_ms(int64 end_ms);
       // show the bytes received
      MessageHandler* message_handler();
      NgxUrlAsyncFetcher* get_fetcher() {
        return fetcher_;
      }

      AsyncFetch* get_async_fetch() {
        return async_fetch_;
      }

      int InitRquest();
      int Connect();
      void set_response_handler(response_handler_pt handler) {
        response_handler = handler;
      }

    private:

      response_handler_pt response_handler;
      // functions used in event callback

      bool ParseUrl();
      
      static void NgxFetchResolveDone(ngx_resolver_ctx_t* ctx);
      
      // create request, connect

      // handler of write event
      static void NgxFetchWrite(ngx_event_t* wev);

      // handler of read event
      static void NgxFetchRead(ngx_event_t* rev);
      static bool NgxFetchHandleStatusLine(ngx_connection_t* c);
      // handle header 
      static bool NgxFetchHandleHeader(ngx_connection_t* c);
      // handle body
      static bool NgxFetchHandleBody(ngx_connection_t* c);

      // cancel the fetch when timeout;
      static void NgxFetchTimeout(ngx_event_t* tev);

      // add pagespeed user-agent
      void FixUserAgent();
      void FixHost();

      const GoogleString str_url_;
      ngx_url_t url_;
      NgxUrlAsyncFetcher* fetcher_;
      AsyncFetch* async_fetch_;
      ResponseHeadersParser parser_;
      NgxStatusLine* status_line_;
      MessageHandler* message_handler_;
      size_t bytes_received_;
      int64 fetch_start_ms_;
      int64 fetch_end_ms_;
      int64 timeout_ms_;

      ngx_log_t* log_;
      ngx_buf_t* out_;
      ngx_buf_t* in_;
      ngx_pool_t* pool_;
      ngx_event_t* timeout_event_;
      ngx_connection_t* connection_;
      ngx_resolver_ctx_t* resolver_ctx_;
      
      DISALLOW_COPY_AND_ASSIGN(NgxFetch);
  };
} // namespace net_instaweb
#endif // 
