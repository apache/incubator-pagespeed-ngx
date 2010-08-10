// Copyright 2010 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "net/instaweb/apache/serf_url_async_fetcher.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/stl_util-inl.h"
#include "net/instaweb/apache/apr_mutex.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/meta_data.h"
#include "net/instaweb/util/public/writer.h"
#include "third_party/apache/apr/src/include/apr_atomic.h"
#include "third_party/apache/apr/src/include/apr_strings.h"
#include "third_party/apache/apr/src/include/apr_version.h"
#include "third_party/serf/src/serf.h"
#include "third_party/serf/src/serf_bucket_util.h"

namespace {
const int kBufferSize = 2048;
const char kFetchMethod[] = "GET";
}  // namespace

namespace html_rewriter {

std::string GetAprErrorString(apr_status_t status) {
  char error_str[1024];
  apr_strerror(status, error_str, sizeof(error_str));
  return error_str;
}

// TODO(lsong): Move this to a separate file. Necessary?
class SerfFetch {
 public:
  // TODO(lsong): make use of request_headers.
  SerfFetch(SerfUrlAsyncFetcher* fetcher,
            const std::string& url,
            const MetaData& request_headers,
            MetaData* response_headers,
            Writer* fetched_content_writer,
            MessageHandler* message_handler,
            UrlAsyncFetcher::Callback* callback)
      : fetcher_(fetcher),
        str_url_(url),
        response_headers_(response_headers),
        fetched_content_writer_(fetched_content_writer),
        message_handler_(message_handler),
        callback_(callback),
        connection_(NULL) {
    apr_pool_create(&pool_, fetcher->pool());
    bucket_alloc_ = serf_bucket_allocator_create(pool_, NULL, NULL);
  }

  ~SerfFetch() {
    if (connection_ != NULL) {
      serf_connection_close(connection_);
    }
    apr_pool_destroy(pool_);
  }

  // Start the fetch. It returns immediately.
  bool Start();

 private:

  // Static functions used in callbacks.
  static serf_bucket_t* ConnectionSetup(
      apr_socket_t* socket, void* setup_baton, apr_pool_t* pool) {
    SerfFetch* fetch = static_cast<SerfFetch*>(setup_baton);
    return serf_bucket_socket_create(socket, fetch->bucket_alloc_);
  }

  static void ClosedConnection(serf_connection_t* conn,
                               void* closed_baton,
                               apr_status_t why,
                               apr_pool_t* pool) {
    SerfFetch* fetch = static_cast<SerfFetch*>(closed_baton);
    if (why != APR_SUCCESS) {
      fetch->message_handler_->Warning(
          fetch->str_url_.c_str(), 0, "Connection close (code=%d %s).",
          why, GetAprErrorString(why).c_str());
    }
    // Connection is closed.
    fetch->connection_ = NULL;
  }

  static serf_bucket_t* AcceptResponse(serf_request_t* request,
                                       serf_bucket_t* stream,
                                       void* acceptor_baton,
                                       apr_pool_t* pool) {
    // Get the per-request bucket allocator.
    serf_bucket_alloc_t* bucket_alloc = serf_request_get_alloc(request);
    // Create a barrier so the response doesn't eat us!
    // From the comment in Serf:
    // ### the stream does not have a barrier, this callback should generally
    // ### add a barrier around the stream before incorporating it into a
    // ### response bucket stack.
    // ... i.e. the passed bucket becomes owned rather than
    // ### borrowed.
    serf_bucket_t* bucket = serf_bucket_barrier_create(stream, bucket_alloc);
    return serf_bucket_response_create(bucket, bucket_alloc);
  }

  static apr_status_t HandleResponse(serf_request_t* request,
                                     serf_bucket_t* response,
                                     void* handler_baton,
                                     apr_pool_t* pool) {
    SerfFetch* fetch = static_cast<SerfFetch*>(handler_baton);
    return fetch->HandleResponse(request, response);
  }

  // The handler MUST process data from the response bucket until the
  // bucket's read function states it would block (APR_STATUS_IS_EAGAIN).
  // The handler is invoked only when new data arrives. If no further data
  // arrives, and the handler does not process all available data, then the
  // system can result in a deadlock around the unprocessed, but read, data.
  apr_status_t HandleResponse(serf_request_t* request,
                              serf_bucket_t* response) {
    apr_status_t status = APR_EGENERAL;
    serf_status_line status_line;
    if ((response != NULL) &&
        ((status = serf_bucket_response_status(response, &status_line))
         == APR_SUCCESS)) {
      response_headers_->set_status_code(status_line.code);
      response_headers_->set_major_version(status_line.version / 1000);
      response_headers_->set_minor_version(status_line.version % 1000);
      const char* data = NULL;
      apr_size_t len = 0;
      while ((status = serf_bucket_read(response, kBufferSize, &data, &len))
             == APR_SUCCESS || APR_STATUS_IS_EOF(status) ||
             APR_STATUS_IS_EAGAIN(status)) {
        if (len > 0 &&
            !fetched_content_writer_->Write(data, len, message_handler_)) {
          status = APR_EGENERAL;
          break;
        }
        if (status != APR_SUCCESS) {
          break;
        }
      }
      // We could read the headers earlier, but then we have to check if we
      // have received the headers.  At EOF of response, we have the headers
      // already. Read them.
      if (APR_STATUS_IS_EOF(status)) {
        status = ReadHeaders(response);
      }
    }
    if (!APR_STATUS_IS_EAGAIN(status)) {
      bool success = APR_STATUS_IS_EOF(status);
      callback_->Done(success);
      fetcher_->FetchComplete(this);
    }
    return status;
  }

  apr_status_t ReadHeaders(serf_bucket_t* response) {
    apr_status_t status = APR_SUCCESS;
    serf_bucket_t* headers = serf_bucket_response_get_headers(response);
    const char* data = NULL;
    apr_size_t num_bytes = 0;
    while ((status = serf_bucket_read(headers, kBufferSize, &data, &num_bytes))
           == APR_SUCCESS || APR_STATUS_IS_EOF(status) ||
           APR_STATUS_IS_EAGAIN(status)) {
      if (response_headers_->headers_complete()) {
        status = APR_EGENERAL;
        message_handler_->Error(str_url_.c_str(), 0,
                                "headers complete but more data coming");
      }
      net_instaweb::StringPiece str_piece(data, num_bytes);
      apr_size_t parsed_len =
          response_headers_->ParseChunk(str_piece, message_handler_);
      if (parsed_len != num_bytes) {
        status = APR_EGENERAL;
        message_handler_->Error(str_url_.c_str(), 0,
                                "unexpected bytes at end of header");
      }
      if (status != APR_SUCCESS) {
        break;
      }
    }
    if (APR_STATUS_IS_EOF(status)
        && !response_headers_->headers_complete()) {
      message_handler_->Error(str_url_.c_str(), 0,
                              "eof on incomplete headers code=%d %s",
                              status, GetAprErrorString(status).c_str());
      status = APR_EGENERAL;
    }
    return status;
  }

  static apr_status_t SetupRequest(serf_request_t* request,
                                   void* setup_baton,
                                   serf_bucket_t** req_bkt,
                                   serf_response_acceptor_t* acceptor,
                                   void** acceptor_baton,
                                   serf_response_handler_t* handler,
                                   void** handler_baton,
                                   apr_pool_t* pool) {
    SerfFetch* fetch = static_cast<SerfFetch*>(setup_baton);
    const char* url_path = apr_uri_unparse(pool, &fetch->url_,
                                           APR_URI_UNP_OMITSITEPART);
    *req_bkt = serf_request_bucket_request_create(
        request, kFetchMethod,
        url_path, NULL,
        serf_request_get_alloc(request));
    serf_bucket_t* hdrs_bkt = serf_bucket_request_get_headers(*req_bkt);
    // TODO(lsong): Use user-agent from the request headers.
    serf_bucket_headers_setn(hdrs_bkt, "User-Agent",
                             "Serf/" SERF_VERSION_STRING);
    serf_bucket_headers_setn(hdrs_bkt, "Accept-Encoding", "gzip");
    *acceptor = SerfFetch::AcceptResponse;
    *acceptor_baton = fetch;
    *handler = SerfFetch::HandleResponse;
    *handler_baton = fetch;
    return APR_SUCCESS;
  }

  bool ParseUrl() {
    apr_status_t status = 0;
    status = apr_uri_parse(pool_, str_url_.c_str(), &url_);
    if (status != APR_SUCCESS) {
      return false;  // Failed to parse URL.
    }

    // TODO(lsong): We do not handle HTTPS for now. HTTPS needs authentication
    // verifying certificates, etc.
    if (strcasecmp(url_.scheme, "https") == 0) {
      return false;
    }
    if (!url_.port) {
      url_.port = apr_uri_port_of_scheme(url_.scheme);
    }
    if (!url_.path) {
      url_.path = apr_pstrdup(pool_, "/");
    }
    return true;
  }

  SerfUrlAsyncFetcher* fetcher_;
  const std::string str_url_;
  MetaData* response_headers_;
  Writer* fetched_content_writer_;
  MessageHandler* message_handler_;
  UrlAsyncFetcher::Callback* callback_;

  apr_pool_t* pool_;
  serf_bucket_alloc_t* bucket_alloc_;
  apr_uri_t url_;
  serf_connection_t* connection_;
};

bool SerfFetch::Start() {
  // Parse and validate the URL.
  if (!ParseUrl()) {
    return false;
  }

  apr_status_t status = serf_connection_create2(&connection_,
                                                fetcher_->serf_context(),
                                                url_,
                                                ConnectionSetup, this,
                                                ClosedConnection, this,
                                                pool_);
  if (status != APR_SUCCESS) {
    message_handler_->Error(__FILE__, __LINE__, "Creating connection");
    return false;
  }
  serf_connection_request_create(connection_, SetupRequest, this);

  // Start the fetch. It will connect to the remote host, send the request,
  // and accept the response, without blocking.
  status = serf_context_run(fetcher_->serf_context(), 0, fetcher_->pool());

  if (status == APR_SUCCESS || APR_STATUS_IS_TIMEUP(status)) {
    return true;
  } else {
    message_handler_->Error(str_url_.c_str(), 0,
                            "serf_context_run error status=%d (%s)",
                            status, GetAprErrorString(status).c_str());
    return false;
  }
}


// Set up the proxy for all the connections in the context. The proxy is in the
// format of hostname:port.
bool SerfUrlAsyncFetcher::SetupProxy(const char* proxy) {
  apr_status_t status = 0;
  if (proxy == NULL || *proxy == '\0') {
    return true;  // No proxy to be set.
  }

  apr_sockaddr_t* proxy_address = NULL;
  apr_port_t proxy_port;
  char* proxy_host;
  char* proxy_scope;
  status = apr_parse_addr_port(&proxy_host, &proxy_scope, &proxy_port, proxy,
                               pool_);
  if (status != APR_SUCCESS || proxy_host == NULL || proxy_port == 0 ||
      (status = apr_sockaddr_info_get(&proxy_address, proxy_host, APR_UNSPEC,
                                      proxy_port, 0, pool_)) != APR_SUCCESS) {
    return false;
  }
  serf_config_proxy(serf_context_, proxy_address);
  return true;
}

SerfUrlAsyncFetcher::SerfUrlAsyncFetcher(const char* proxy, apr_pool_t* pool)
    : pool_(pool),
      mutex_(NULL),
      serf_context_(NULL) {
  mutex_ = new AprMutex(pool_);
  net_instaweb::ScopedMutex mutex(mutex_);
  serf_context_ = serf_context_create(pool_);
  if (SetupProxy(proxy)) {
    // TODO(lsong): What to do if the proxy setup fails?
  }
}

SerfUrlAsyncFetcher::~SerfUrlAsyncFetcher() {
  delete mutex_;
  STLDeleteElements(&active_fetches_);
}

void SerfUrlAsyncFetcher::StreamingFetch(const std::string& url,
                                         const MetaData& request_headers,
                                         MetaData* response_headers,
                                         Writer* fetched_content_writer,
                                         MessageHandler* message_handler,
                                         UrlAsyncFetcher::Callback* callback) {
  SerfFetch* fetch = new SerfFetch(this,
                                   url, request_headers,
                                   response_headers,
                                   fetched_content_writer,
                                   message_handler, callback);
  if (fetch->Start()) {
    net_instaweb::ScopedMutex mutex(mutex_);
    active_fetches_.insert(fetch);
  } else {
    delete fetch;
  }
  return;
}

bool SerfUrlAsyncFetcher::Poll(int microseconds, MessageHandler* handler) {
  // Run serf polling up to microseconds.
  apr_status_t status = serf_context_run(serf_context_, microseconds, pool_);
  if (status != APR_SUCCESS && !APR_STATUS_IS_TIMEUP(status)) {
    // TODO(lsong): log error?
    return false;
  }
  net_instaweb::ScopedMutex mutex(mutex_);
  // Clear the completed fetches.
  STLDeleteElements(&completed_fetches_);
  return true;
}

void SerfUrlAsyncFetcher::FetchComplete(SerfFetch* fetch) {
  net_instaweb::ScopedMutex mutex(mutex_);
  int erased = active_fetches_.erase(fetch);
  if (erased != 1) {
    // TODO(lsong): log error? Add message_handler.
  }
  completed_fetches_.push_back(fetch);
}

bool SerfUrlAsyncFetcher::WaitForInProgressFetches(
    int64 max_milliseconds, MessageHandler* message_handler) {
  AprTimer timer;
  for (int64 start_ms = timer.NowMs(), now_ms = start_ms;
       !active_fetches_.empty() && now_ms - start_ms < max_milliseconds;
       now_ms = timer.NowMs()) {
    int64 remaining_us = std::max(static_cast<int64>(0),
                                  1000 * (max_milliseconds - now_ms));
    Poll(remaining_us, message_handler);
  }
  if (!active_fetches_.empty()) {
    return false;
  }
  return true;
}

}  // namespace html_rewriter
