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
//
// Author: jmarantz@google.com (Joshua Marantz)
//         lsong@google.com (Libo Song)

// TODO(jmarantz): Avoid initiating fetches for resources already in flight.
// The challenge is that we would want to call all the callbacks that indicated
// interest in a particular URL once the callback completed.  Alternatively,
// this could be done in a level above the URL fetcher.

#include "net/instaweb/system/public/serf_url_async_fetcher.h"

#include <cstddef>
#include <list>
#include <vector>

#include "apr.h"
#include "apr_strings.h"
#include "apr_pools.h"
#include "apr_thread_proc.h"
#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/response_headers_parser.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/system/public/apr_thread_compatible_pool.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/pool.h"
#include "net/instaweb/util/public/pool_element.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "third_party/serf/src/serf.h"

// This is an easy way to turn on lots of debug messages. Note that this
// is somewhat verbose.
#define SERF_DEBUG(x)

namespace {

enum HttpsOptions {
  kEnableHttps                          = 1 << 0,
  kAllowSelfSigned                      = 1 << 1,
  kAllowUnknownCertificateAuthority     = 1 << 2,
  kAllowCertificateNotYetValid          = 1 << 3,
};

}  // namespace

extern "C" {
  // Declares new functions added to
  // src/third_party/serf/instaweb_context.c
serf_bucket_t* serf_request_bucket_request_create_for_host(
    serf_request_t *request,
    const char *method,
    const char *uri,
    serf_bucket_t *body,
    serf_bucket_alloc_t *allocator, const char* host);

int serf_connection_is_in_error_state(serf_connection_t* connection);

  // Declares new functions added in instaweb_ssl_buckets.c
apr_status_t serf_ssl_set_certificates_directory(serf_ssl_context_t *ssl_ctx,
                                                 const char* path);
apr_status_t serf_ssl_set_certificates_file(serf_ssl_context_t *ssl_ctx,
                                            const char* file);
}  // extern "C"

namespace net_instaweb {

const char SerfStats::kSerfFetchRequestCount[] = "serf_fetch_request_count";
const char SerfStats::kSerfFetchByteCount[] = "serf_fetch_bytes_count";
const char SerfStats::kSerfFetchTimeDurationMs[] =
    "serf_fetch_time_duration_ms";
const char SerfStats::kSerfFetchCancelCount[] = "serf_fetch_cancel_count";
const char SerfStats::kSerfFetchActiveCount[] =
    "serf_fetch_active_count";
const char SerfStats::kSerfFetchTimeoutCount[] = "serf_fetch_timeout_count";
const char SerfStats::kSerfFetchFailureCount[] = "serf_fetch_failure_count";
const char SerfStats::kSerfFetchCertErrors[] = "serf_fetch_cert_errors";

GoogleString GetAprErrorString(apr_status_t status) {
  char error_str[1024];
  apr_strerror(status, error_str, sizeof(error_str));
  return error_str;
}

// TODO(lsong): Move this to a separate file. Necessary?
class SerfFetch : public PoolElement<SerfFetch> {
 public:
  // TODO(lsong): make use of request_headers.
  SerfFetch(const GoogleString& url,
            AsyncFetch* async_fetch,
            MessageHandler* message_handler,
            Timer* timer)
      : fetcher_(NULL),
        timer_(timer),
        str_url_(url),
        async_fetch_(async_fetch),
        parser_(async_fetch->response_headers()),
        status_line_read_(false),
        one_byte_read_(false),
        has_saved_byte_(false),
        saved_byte_('\0'),
        message_handler_(message_handler),
        pool_(NULL),  // filled in once assigned to a thread, to use its pool.
        bucket_alloc_(NULL),
        host_header_(NULL),
        sni_host_(NULL),
        connection_(NULL),
        bytes_received_(0),
        fetch_start_ms_(0),
        fetch_end_ms_(0),
        using_https_(false),
        ssl_context_(NULL),
        ssl_error_message_(NULL) {
  }

  ~SerfFetch() {
    DCHECK(async_fetch_ == NULL);
    if (connection_ != NULL) {
      serf_connection_close(connection_);
    }
    if (pool_ != NULL) {
      apr_pool_destroy(pool_);
    }
  }

  // Start the fetch. It returns immediately.  This can only be run when
  // locked with fetcher->mutex_.
  bool Start(SerfUrlAsyncFetcher* fetcher);

  const char* str_url() { return str_url_.c_str(); }

  // This must be called while holding SerfUrlAsyncFetcher's mutex_.
  void Cancel() {
    if (connection_ != NULL) {
      // We can get here either because we're canceling the connection ourselves
      // or because Serf detected an error.
      //
      // If we canceled/timed out, we want to close the serf connection so it
      // doesn't call us back, as we will detach from the async_fetch_ shortly.
      //
      // If Serf detected an error we also want to clean up as otherwise it will
      // keep re-detecting it, which will interfere with other jobs getting
      // handled (until we finally cleanup the old fetch and close things in
      // ~SerfFetch).
      serf_connection_close(connection_);
      connection_ = NULL;
    }

    CallCallback(false);
  }

  // Calls the callback supplied by the user.  This needs to happen
  // exactly once.  In some error cases it appears that Serf calls
  // HandleResponse multiple times on the same object.
  //
  // This must be called while holding SerfUrlAsyncFetcher's mutex_.
  //
  // Note that when there are SSL error messages, we immediately call
  // CallCallback, which is robust against duplicate calls in that case.
  void CallCallback(bool success) {
    if (ssl_error_message_ != NULL) {
      success = false;
    }

    if (async_fetch_ != NULL) {
      fetch_end_ms_ = timer_->NowMs();
      fetcher_->ReportCompletedFetchStats(this);
      CallbackDone(success);
      fetcher_->FetchComplete(this);
    } else if (ssl_error_message_ == NULL) {
      LOG(FATAL) << "BUG: Serf callback called more than once on same fetch "
                 << str_url() << " (" << this << ").  Please report this "
                 << "at http://code.google.com/p/modpagespeed/issues/";
    }
  }

  void CallbackDone(bool success) {
    // fetcher_==NULL if Start is called during shutdown.
    if (fetcher_ != NULL) {
      if (!success) {
        fetcher_->failure_count_->Add(1);
      }
      if (fetcher_->track_original_content_length() &&
          !async_fetch_->response_headers()->Has(
              HttpAttributes::kXOriginalContentLength)) {
        async_fetch_->extra_response_headers()->SetOriginalContentLength(
            bytes_received_);
      }
    }
    async_fetch_->Done(success);
    // We should always NULL the async_fetch_ out after calling otherwise we
    // could get weird double calling errors.
    async_fetch_ = NULL;
  }

  // If last poll of this fetch's connection resulted in an error, clean it up.
  // Must be called after serf_context_run, with fetcher's mutex_ held.
  void CleanupIfError() {
    if ((connection_ != NULL) &&
        serf_connection_is_in_error_state(connection_)) {
      message_handler_->Message(
          kInfo, "Serf cleanup for error'd fetch of: %s", str_url());
      Cancel();
    }
  }

  int64 TimeDuration() const {
    if ((fetch_start_ms_ != 0) && (fetch_end_ms_ != 0)) {
      return fetch_end_ms_ - fetch_start_ms_;
    } else {
      return 0;
    }
  }
  int64 fetch_start_ms() const { return fetch_start_ms_; }

  size_t bytes_received() const { return bytes_received_; }
  MessageHandler* message_handler() { return message_handler_; }

 private:
  // Static functions used in callbacks.

  // The code under SERF_HTTPS_FETCHING was contributed by Devin Anderson
  // (surfacepatterns@gmail.com).
  //
  // Note this must be ifdef'd because calling serf_bucket_ssl_decrypt_create
  // requires ssl_buckets.c in the link.  ssl_buckets.c requires openssl.
#if SERF_HTTPS_FETCHING
  static apr_status_t SSLCertError(void *data, int failures,
                                   const serf_ssl_certificate_t *cert) {
    return static_cast<SerfFetch*>(data)->HandleSSLCertErrors(failures, 0);
  }

  static apr_status_t SSLCertChainError(
      void *data, int failures, int error_depth,
      const serf_ssl_certificate_t * const *certs,
      apr_size_t certs_count) {
    return static_cast<SerfFetch*>(data)->HandleSSLCertErrors(failures,
                                                              error_depth);
  }
#endif

  static apr_status_t ConnectionSetup(
      apr_socket_t* socket, serf_bucket_t **read_bkt, serf_bucket_t **write_bkt,
      void* setup_baton, apr_pool_t* pool) {
    SerfFetch* fetch = static_cast<SerfFetch*>(setup_baton);
    *read_bkt = serf_bucket_socket_create(socket, fetch->bucket_alloc_);
#if SERF_HTTPS_FETCHING
    apr_status_t status = APR_SUCCESS;
    if (fetch->using_https_) {
      *read_bkt = serf_bucket_ssl_decrypt_create(*read_bkt,
                                                 fetch->ssl_context_,
                                                 fetch->bucket_alloc_);
      if (fetch->ssl_context_ == NULL) {
        fetch->ssl_context_ = serf_bucket_ssl_decrypt_context_get(*read_bkt);
        if (fetch->ssl_context_ == NULL) {
          status = APR_EGENERAL;
        } else {
          SerfUrlAsyncFetcher* fetcher = fetch->fetcher_;
          const GoogleString& certs_dir = fetcher->ssl_certificates_dir();
          const GoogleString& certs_file = fetcher->ssl_certificates_file();

          if (!certs_file.empty()) {
            status = serf_ssl_set_certificates_file(
                fetch->ssl_context_, certs_file.c_str());
          }
          if ((status == APR_SUCCESS) && !certs_dir.empty()) {
            status = serf_ssl_set_certificates_directory(fetch->ssl_context_,
                                                         certs_dir.c_str());
          }

          // If no explicit file or directory is specified, then use the
          // compiled-in default.
          if (certs_dir.empty() && certs_file.empty()) {
            status = serf_ssl_use_default_certificates(fetch->ssl_context_);
          }
        }
        if (status != APR_SUCCESS) {
          return status;
        }
      }

      serf_ssl_server_cert_callback_set(fetch->ssl_context_, SSLCertError,
                                        fetch);

      serf_ssl_server_cert_chain_callback_set(fetch->ssl_context_,
                                              SSLCertError, SSLCertChainError,
                                              fetch);

      serf_ssl_set_hostname(fetch->ssl_context_, fetch->sni_host_);
      *write_bkt = serf_bucket_ssl_encrypt_create(*write_bkt,
                                                  fetch->ssl_context_,
                                                  fetch->bucket_alloc_);
    }
#endif
    return APR_SUCCESS;
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
    return fetch->HandleResponse(response);
  }

  static bool MoreDataAvailable(apr_status_t status) {
    // This OR is structured like this to make debugging easier, as it's
    // not obvious when looking at the status mask which of these conditions
    // is hit.
    if (APR_STATUS_IS_EAGAIN(status)) {
      return true;
    }
    return APR_STATUS_IS_EINTR(status);
  }

  static bool IsStatusOk(apr_status_t status) {
    return ((status == APR_SUCCESS) ||
            APR_STATUS_IS_EOF(status) ||
            MoreDataAvailable(status));
  }

#if SERF_HTTPS_FETCHING
  // Called indicating whether SSL certificate errors have occurred detected.
  // The function returns SUCCESS in all cases, but sets ssl_error_message_
  // non-null for errors as a signal to ReadHeaders that we should not let
  // any output thorugh.
  //
  // Interpretation of two of the error conditions is configuraable:
  // 'allow_unknown_certificate_authority' and 'allow_self_signed'.
  apr_status_t HandleSSLCertErrors(int errors, int failure_depth) {
    // TODO(jmarantz): is there value in logging the errors and failure_depth
    // formals here?

    // Note that HandleSSLCertErrors can be called multiple times for
    // a single request.  As far as I can tell, there is value in
    // recording only one of these.  For now, I have set up the logic
    // so only the last error will be printed lazilly, in ReadHeaders.
    if (((errors & SERF_SSL_CERT_SELF_SIGNED) != 0) &&
        !fetcher_->allow_self_signed()) {
      ssl_error_message_ = "SSL certificate is self-signed";
    } else if (((errors & SERF_SSL_CERT_UNKNOWNCA) != 0) &&
               !fetcher_->allow_unknown_certificate_authority()) {
      ssl_error_message_ =
          "SSL certificate has an unknown certificate authority";
    } else if (((errors & SERF_SSL_CERT_NOTYETVALID) != 0) &&
               !fetcher_->allow_certificate_not_yet_valid()) {
      ssl_error_message_ = "SSL certificate is not yet valid";
    } else if (errors & SERF_SSL_CERT_EXPIRED) {
      ssl_error_message_ = "SSL certificate is expired";
    } else if (errors & SERF_SSL_CERT_UNKNOWN_FAILURE) {
      ssl_error_message_ = "SSL certificate has an unknown error";
    }

    // Immediately call the fetch callback on a cert error.  Note that
    // HandleSSLCertErrors is called multiple times when there is an error,
    // so check async_fetch before CallCallback.
    if ((ssl_error_message_ != NULL) && (async_fetch_ != NULL)) {
      fetcher_->cert_errors_->Add(1);
      CallCallback(false);  // sets async_fetch_ to null.
    }

    // TODO(jmarantz): I think the design of this system indicates
    // that we should be returning APR_EGENERAL on failure.  However I
    // have found that doesn't work properly, at least for
    // SERF_SSL_CERT_SELF_SIGNED.  The request does not terminate
    // quickly but instead times out.  Thus we return APR_SUCCESS
    // but change the status_code to 404, report an error, and suppress
    // the output.
    //
    // TODO(jmarantz): consider aiding diagnosibility with by changing the
    // 404 to a 401 (Unauthorized) or 418 (I'm a teapot), or 459 (nginx
    // internal cert error code).

    return APR_SUCCESS;
  }
#endif

  // The handler MUST process data from the response bucket until the
  // bucket's read function states it would block (APR_STATUS_IS_EAGAIN).
  // The handler is invoked only when new data arrives. If no further data
  // arrives, and the handler does not process all available data, then the
  // system can result in a deadlock around the unprocessed, but read, data.
  apr_status_t HandleResponse(serf_bucket_t* response) {
    if (response == NULL) {
      message_handler_->Message(
          kInfo, "serf HandlerReponse called with NULL response for %s",
          str_url());
      CallCallback(false);
      return APR_EGENERAL;
    }

    // The response-handling code must be robust to packets coming in all at
    // once, one byte at a time, or anything in between.  EAGAIN indicates
    // that more data is available in the socket so another read should
    // be issued before returning.
    apr_status_t status = APR_EAGAIN;
    while (MoreDataAvailable(status) && (async_fetch_ != NULL) &&
            !parser_.headers_complete()) {
      if (!status_line_read_) {
        status = ReadStatusLine(response);
      }

      if (status_line_read_ && !one_byte_read_) {
        status = ReadOneByteFromBody(response);
      }

      if (one_byte_read_ && !parser_.headers_complete()) {
        status = ReadHeaders(response);
      }
    }

    if (parser_.headers_complete()) {
      status = ReadBody(response);
    }

    if ((async_fetch_ != NULL) &&
        ((APR_STATUS_IS_EOF(status) && parser_.headers_complete()) ||
         (status == APR_EGENERAL))) {
      bool success = (IsStatusOk(status) && parser_.headers_complete());
      if (!parser_.headers_complete() && (async_fetch_ != NULL)) {
        // Be careful not to leave headers in inconsistent state in some error
        // conditions.
        async_fetch_->response_headers()->Clear();
      }
      CallCallback(success);
    }
    return status;
  }

  apr_status_t ReadStatusLine(serf_bucket_t* response) {
    serf_status_line status_line;
    apr_status_t status = serf_bucket_response_status(response, &status_line);
    ResponseHeaders* response_headers = async_fetch_->response_headers();
    if (status == APR_SUCCESS) {
      response_headers->SetStatusAndReason(
          static_cast<HttpStatus::Code>(status_line.code));
      response_headers->set_major_version(status_line.version / 1000);
      response_headers->set_minor_version(status_line.version % 1000);
      status_line_read_ = true;
    }
    return status;
  }

  // Know what's weird?  You have do a body-read to get access to the
  // headers.  You need to read 1 byte of body to force an FSM inside
  // Serf to parse the headers.  Then you can parse the headers and
  // finally read the rest of the body.  I know, right?
  //
  // The simpler approach, and likely what the Serf designers intended,
  // is that you read the entire body first, and then read the headers.
  // But if you are trying to stream the data as its fetched through some
  // kind of function that needs to know the content-type, then it's
  // really a drag to have to wait till the end of the body to get the
  // content type.
  apr_status_t ReadOneByteFromBody(serf_bucket_t* response) {
    apr_size_t len = 0;
    const char* data = NULL;
    apr_status_t status = serf_bucket_read(response, 1, &data, &len);
    if (!APR_STATUS_IS_EINTR(status) && IsStatusOk(status)) {
      one_byte_read_ = true;
      if (len == 1) {
        has_saved_byte_ = true;
        saved_byte_ = data[0];
      }
    }
    return status;
  }

  // Once that one byte is read from the body, we can go ahead and
  // parse the headers.  The dynamics of this appear that for N
  // headers we'll get 2N calls to serf_bucket_read: one each for
  // attribute names & values.
  apr_status_t ReadHeaders(serf_bucket_t* response) {
    serf_bucket_t* headers = serf_bucket_response_get_headers(response);
    const char* data = NULL;
    apr_size_t len = 0;
    apr_status_t status = serf_bucket_read(headers, SERF_READ_ALL_AVAIL,
                                           &data, &len);

    // Feed valid chunks to the header parser --- but skip empty ones,
    // which can occur for value-less headers, since otherwise they'd
    // look like parse errors.
    if (IsStatusOk(status) && (len > 0)) {
      if (parser_.ParseChunk(StringPiece(data, len), message_handler_)) {
        if (parser_.headers_complete()) {
          ResponseHeaders* response_headers = async_fetch_->response_headers();
          if (ssl_error_message_ != NULL) {
            response_headers->set_status_code(HttpStatus::kNotFound);
            message_handler_->Message(kInfo, "%s: %s", str_url_.c_str(),
                                      ssl_error_message_);
            has_saved_byte_ = false;
          }

          if (fetcher_->track_original_content_length()) {
            // Set X-Original-Content-Length, if Content-Length is available.
            int64 content_length;
            if (response_headers->FindContentLength(&content_length)) {
              response_headers->SetOriginalContentLength(content_length);
            }
          }
          // Stream the one byte read from ReadOneByteFromBody to writer.
          if (has_saved_byte_) {
            ++bytes_received_;
            if (!async_fetch_->Write(StringPiece(&saved_byte_, 1),
                                     message_handler_)) {
              status = APR_EGENERAL;
            }
          }
        }
      } else {
        status = APR_EGENERAL;
      }
    }
    return status;
  }

  // Once headers are complete we can get the body.  The dynamics of this
  // are likely dependent on everything on the network between the client
  // and server, but for a 10k buffer I seem to frequently get 8k chunks.
  apr_status_t ReadBody(serf_bucket_t* response) {
    apr_status_t status = APR_EAGAIN;
    const char* data = NULL;
    apr_size_t len = 0;
    apr_size_t bytes_to_flush = 0;
    while (MoreDataAvailable(status) && (async_fetch_ != NULL)) {
      status = serf_bucket_read(response, SERF_READ_ALL_AVAIL, &data, &len);
      bytes_received_ += len;
      bytes_to_flush += len;
      if (IsStatusOk(status) && (len != 0) &&
          !async_fetch_->Write(StringPiece(data, len), message_handler_)) {
        status = APR_EGENERAL;
      }
    }
    if ((bytes_to_flush != 0) && !async_fetch_->Flush(message_handler_)) {
      status = APR_EGENERAL;
    }
    return status;
  }

  // Ensures that a user-agent string is included, and that the mod_pagespeed
  // version is appended.
  void FixUserAgent() {
    // Supply a default user-agent if none is present, and in any case
    // append on a 'serf' suffix.
    GoogleString user_agent;
    ConstStringStarVector v;
    RequestHeaders* request_headers = async_fetch_->request_headers();
    if (request_headers->Lookup(HttpAttributes::kUserAgent, &v)) {
      for (int i = 0, n = v.size(); i < n; ++i) {
        if (i != 0) {
          user_agent += " ";
        }
        if (v[i] != NULL) {
          user_agent += *(v[i]);
        }
      }
      request_headers->RemoveAll(HttpAttributes::kUserAgent);
    }
    if (user_agent.empty()) {
      user_agent += "Serf/" SERF_VERSION_STRING;
    }
    GoogleString version = StrCat(
        " ", kModPagespeedSubrequestUserAgent,
        "/" MOD_PAGESPEED_VERSION_STRING "-" LASTCHANGE_STRING);
    if (!StringPiece(user_agent).ends_with(version)) {
      user_agent += version;
    }
    request_headers->Add(HttpAttributes::kUserAgent, user_agent);
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

    // If there is an explicit Host header, then override the
    // host field in the Serf structure, as we will not be able
    // to override it after it is created; only append to it.
    //
    // Serf automatically populates the Host field based on the
    // URL, and provides no mechanism to override it, except
    // by hacking source.  We hacked source.
    //
    // See src/third_party/serf/src/instaweb_context.c

    fetch->FixUserAgent();

    RequestHeaders* request_headers = fetch->async_fetch_->request_headers();
    *req_bkt = serf_request_bucket_request_create_for_host(
        request, request_headers->method_string(),
        url_path, NULL,
        serf_request_get_alloc(request), fetch->host_header_);
    serf_bucket_t* hdrs_bkt = serf_bucket_request_get_headers(*req_bkt);

    // Add other headers from the caller's request.  Skip the "Host:" header
    // because it's set above.
    for (int i = 0; i < request_headers->NumAttributes(); ++i) {
      const GoogleString& name = request_headers->Name(i);
      const GoogleString& value = request_headers->Value(i);
      if (!(StringCaseEqual(name, HttpAttributes::kHost))) {
        // Note: *_setn() stores a pointer to name and value instead of a
        // copy of those values. So name and value must have long lifetimes.
        // In this case, we depend on request_headers being unchanged for
        // the lifetime of hdrs_bkt, which is a documented requirement of
        // the UrlAsyncFetcher interface.
        serf_bucket_headers_setn(hdrs_bkt, name.c_str(), value.c_str());
      }
    }

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
    bool is_https = StringCaseEqual(url_.scheme, "https");
    if (is_https && !fetcher_->allow_https()) {
      return false;
    }
    if (!url_.port) {
      url_.port = apr_uri_port_of_scheme(url_.scheme);
    }
    if (!url_.path) {
      url_.path = apr_pstrdup(pool_, "/");
    }

    // Compute our host header. First see if there is an explicit specified
    // Host: in the fetch object.
    RequestHeaders* request_headers = async_fetch_->request_headers();
    const char* host = request_headers->Lookup1(HttpAttributes::kHost);
    if (host == NULL) {
      host = SerfUrlAsyncFetcher::ExtractHostHeader(url_, pool_);
    }

    host_header_ = apr_pstrdup(pool_, host);

    if (is_https) {
      // SNI hosts, unlike Host: do not have a port number.
      GoogleString sni_host =
          SerfUrlAsyncFetcher::RemovePortFromHostHeader(host_header_);
      sni_host_ = apr_pstrdup(pool_, sni_host.c_str());
    }

    return true;
  }

  SerfUrlAsyncFetcher* fetcher_;
  Timer* timer_;
  const GoogleString str_url_;
  AsyncFetch* async_fetch_;
  ResponseHeadersParser parser_;
  bool status_line_read_;
  bool one_byte_read_;
  bool has_saved_byte_;
  char saved_byte_;
  MessageHandler* message_handler_;

  apr_pool_t* pool_;
  serf_bucket_alloc_t* bucket_alloc_;
  apr_uri_t url_;
  const char* host_header_;  // in pool_
  const char* sni_host_;  // in pool_
  serf_connection_t* connection_;
  size_t bytes_received_;
  int64 fetch_start_ms_;
  int64 fetch_end_ms_;

  // Variables used for HTTPS connection handling
  bool using_https_;
  serf_ssl_context_t* ssl_context_;
  const char* ssl_error_message_;

  DISALLOW_COPY_AND_ASSIGN(SerfFetch);
};

class SerfThreadedFetcher : public SerfUrlAsyncFetcher {
 public:
  SerfThreadedFetcher(SerfUrlAsyncFetcher* parent, const char* proxy) :
      SerfUrlAsyncFetcher(parent, proxy),
      thread_id_(NULL),
      initiate_mutex_(parent->thread_system()->NewMutex()),
      initiate_fetches_(new SerfFetchPool()),
      initiate_fetches_nonempty_(initiate_mutex_->NewCondvar()),
      thread_finish_(false),
      thread_started_(false) {
  }

  ~SerfThreadedFetcher() {
    // Let the thread terminate naturally by telling it to unblock,
    // then waiting for it to finish its next active Poll operation.
    {
      // Indicate termination and unblock the worker thread so it can clean up.
      ScopedMutex lock(initiate_mutex_.get());
      if (thread_started_) {
        thread_finish_ = true;
        initiate_fetches_nonempty_->Signal();
      } else {
        LOG(INFO) << "Serf threaded not actually started, quick shutdown.";
        return;
      }
    }

    LOG(INFO) << "Waiting for threaded serf fetcher to terminate";
    apr_status_t ignored_retval;
    apr_thread_join(&ignored_retval, thread_id_);

    // Under normal circumstances there shouldn't be any active fetches at
    // this point.  However, in practice we may have some lingering fetches that
    // have timed out, and we need to clean those up properly before we can
    // exit.  We try to do this gracefully, but fall back to graceless cleanup
    // if that fails.

    // Before we can clean up, we must make sure we haven't initiated any
    // fetches that haven't moved to the active pool yet.  This should not
    // happen, but we're exercising undue caution here.  We do this by just
    // moving them across.  From this point, calls to InitiateFetch(...) are
    // illegal, but we should be invoking this destructor from the only thread
    // that could have called InitiateFetch anyhow.
    TransferFetchesAndCheckDone(false);
    // Although Cancel will be called in the base class destructor, we
    // want to call it here as well, as it will make it easier for the
    // thread to terminate.
    CancelActiveFetches();
    completed_fetches_.DeleteAll();
    initiate_fetches_->DeleteAll();
  }

  void StartThread() {
    CHECK_EQ(APR_SUCCESS,
             apr_thread_create(&thread_id_, NULL, SerfThreadFn, this, pool_));
    thread_started_ = true;
  }

  // Called from mainline to queue up a fetch for the thread.  If the
  // thread is idle then we can unlock it.
  void InitiateFetch(SerfFetch* fetch) {
    ScopedMutex lock(initiate_mutex_.get());

    // We delay thread startup until we actually want to fetch something
    // to avoid problems with ITK.
    if (!thread_started_) {
      StartThread();
    }

    // TODO(jmaessen): Consider adding an awaiting_nonempty_ flag to avoid
    // spurious calls to Signal().
    bool signal = initiate_fetches_->empty();
    initiate_fetches_->Add(fetch);
    if (signal) {
      initiate_fetches_nonempty_->Signal();
    }
  }

  void ShutDown() {
    // See comments in the destructor above.. The big difference is that
    // because we set shutdown_ to true new jobs can't actually come in.
    {
      // Acquisition order is initiate before hold, see e.g. AnyPendingFetches()
      ScopedMutex hold_initiate(initiate_mutex_.get());
      ScopedMutex hold(mutex_);
      set_shutdown(true);
      if (!thread_started_) {
        return;
      }
    }
    TransferFetchesAndCheckDone(false);
    CancelActiveFetches();
  }

 protected:
  bool AnyPendingFetches() {
    ScopedMutex lock(initiate_mutex_.get());
    // NOTE: We must hold both mutexes to avoid the case where we miss a fetch
    // in transit.
    return !initiate_fetches_->empty() ||
        SerfUrlAsyncFetcher::AnyPendingFetches();
  }

 private:
  static void* APR_THREAD_FUNC SerfThreadFn(apr_thread_t* thread_id,
                                            void* context) {
    SerfThreadedFetcher* stc = static_cast<SerfThreadedFetcher*>(context);
    CHECK_EQ(thread_id, stc->thread_id_);
    stc->SerfThread();
    return NULL;
  }

  // Transfer fetches from initiate_fetches_ to active_fetches_.  If there's no
  // new fetches to initiate, check whether the webserver thread is trying to
  // shut down the worker thread, and return true to indicate "done".  Doesn't
  // do any work if initiate_fetches_ is empty, but in that case if
  // block_on_empty is true it will perform a bounded wait for
  // initiate_fetches_nonempty_.  Called by worker thread and during thread
  // cleanup.
  bool TransferFetchesAndCheckDone(bool block_on_empty) {
    // Use a temp to minimize the amount of time we hold the
    // initiate_mutex_ lock, so that the parent thread doesn't get
    // blocked trying to initiate fetches.
    scoped_ptr<SerfFetchPool> xfer_fetches;
    {
      ScopedMutex lock(initiate_mutex_.get());
      // We must do this checking under the initiate_mutex_ lock.
      if (initiate_fetches_->empty()) {
        // No new work to do now.
        if (!block_on_empty || thread_finish_) {
          return thread_finish_;
        } else {
          // Wait until some work shows up.  Note that after the wait we still
          // must actually check that there's some work to be done.
          initiate_fetches_nonempty_->TimedWait(Timer::kSecondMs);
          if (initiate_fetches_->empty()) {
            // On timeout / false wakeup, return control to caller; we might be
            // finished or have other things to attend to.
            return thread_finish_;
          }
        }
      }
      xfer_fetches.reset(new SerfFetchPool());

      // Take mutex_ before relinquishing initiate_mutex_.  This guarantees that
      // AnyPendingFetches cannot see us in the time between emptying
      // initiate_fetches_ and inserting into active_fetches_.  At that time, it
      // can look as though no fetch work is occurring.  Note that we obtain
      // mutex_ before performing the swap (but after creating the new pool)
      // because additional fetches might arrive in the mean time.  This was
      // causing problems with timeout in TestThreeThreaded under valgrind,
      // because we'd block waiting for mutex_ after a single fetch had been
      // initiated, but not obtain mutex_ until after several more fetches
      // arrived (at which point we'd go into the poll loop without initiating
      // all available fetches).
      mutex_->Lock();
      xfer_fetches.swap(initiate_fetches_);
    }

    // Now that we've unblocked the parent thread, we can leisurely
    // queue up the fetches, employing the proper lock for the active_fetches_
    // set.  Actually we expect we wll never have contention on this mutex
    // from the thread.
    while (!xfer_fetches->empty()) {
      SerfFetch* fetch = xfer_fetches->RemoveOldest();
      if (StartFetch(fetch)) {
        SERF_DEBUG(LOG(INFO) << "Adding threaded fetch to url "
                   << fetch->str_url()
                   << " (" << active_fetches_.size() << ")");
      }
    }
    mutex_->Unlock();
    return false;
  }

  void SerfThread() {
    // Make sure we don't get yet-another copy of signals used by the webserver
    // to shutdown here, to avoid double-free.
    // TODO(morlovich): Port this to use ThreadSystem stuff, and have
    // SystemThreadSystem take care of this automatically.
    apr_setup_signal_thread();

    // Initially there's no active fetch work to be done.
    int num_active_fetches = 0;
    while (!TransferFetchesAndCheckDone(num_active_fetches == 0)) {
      // If initiate_fetches is empty, and there's no current active fetch
      // work to do, we'll block in the above call.  Otherwise the call will
      // start initiated fetches (if any) without blocking.

      // We set the poll interval to try to start new fetches promptly from the
      // observer's perspective (ie .1s is perceptible, so we try to make sure
      // new fetches are started after at most half that time).  The downside is
      // that we don't hand off control to serf / the OS for long periods when
      // fetches are active but no data is arriving.  We trust that doesn't
      // happen often.
      // TODO(jmaessen): Break out of Poll before timeout if work becomes
      // available, so that we initiate new fetches as promptly as possible
      // while continuing to serve the old ones.  This would let us dial the
      // poll interval up high (to multiple seconds).  The classic trick here is
      // to set up a pipe/FIFO/socket and add it to the set of things being
      // read, then use a write to force wakeup.  But will serf support this
      // kind of thing?
      const int64 kPollIntervalMs = Timer::kSecondMs / 20;
      // If active_fetches_ is empty, we will not do any work and won't block
      // here.  num_active_fetches will be 0, and we'll block in the next
      // call to TransferFetches above.
      num_active_fetches = Poll(kPollIntervalMs);
      SERF_DEBUG(LOG(INFO) << "Finished polling from serf thread ("
                 << this << ")");
    }
  }

  apr_thread_t* thread_id_;

  // protects initiate_fetches_, initiate_fetches_nonempty_, thread_finish_
  // and thread_started_.
  scoped_ptr<ThreadSystem::CondvarCapableMutex> initiate_mutex_;
  // pushed in the main thread; popped by TransferFetches().
  scoped_ptr<SerfFetchPool> initiate_fetches_;
  // condvar that indicates that initiate_fetches_ has become nonempty.  During
  // normal operation, only the serf worker thread consumes initiated fetches
  // (this can change during thread shutdown), but the usual condition variable
  // caveats apply: Just because the condition variable indicates
  // initiate_fetches_nonempty_ doesn't mean it's true, and a waiting thread
  // must check initiate_fetches_ explicitly while holding initiate_mutex_.
  scoped_ptr<ThreadSystem::Condvar> initiate_fetches_nonempty_;

  // Flag to signal worker to finish working and terminate.
  bool thread_finish_;

  // True if we actually started the worker thread. Protected by initiate_mutex_
  bool thread_started_;

  DISALLOW_COPY_AND_ASSIGN(SerfThreadedFetcher);
};

bool SerfFetch::Start(SerfUrlAsyncFetcher* fetcher) {
  // Note: this is called in the thread's context, so this is when we do
  // the pool ops.
  fetcher_ = fetcher;
  apr_pool_create(&pool_, fetcher_->pool());
  bucket_alloc_ = serf_bucket_allocator_create(pool_, NULL, NULL);

  fetch_start_ms_ = timer_->NowMs();
  // Parse and validate the URL.
  if (!ParseUrl()) {
    return false;
  }

  using_https_ = StringCaseEqual("https", url_.scheme);
  DCHECK(fetcher->allow_https() || !using_https_);

  apr_status_t status = serf_connection_create2(&connection_,
                                                fetcher_->serf_context(),
                                                url_,
                                                ConnectionSetup, this,
                                                ClosedConnection, this,
                                                pool_);
  if (status != APR_SUCCESS) {
    message_handler_->Error(str_url_.c_str(), 0,
                            "Error status=%d (%s) serf_connection_create2",
                            status, GetAprErrorString(status).c_str());
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

SerfUrlAsyncFetcher::SerfUrlAsyncFetcher(const char* proxy, apr_pool_t* pool,
                                         ThreadSystem* thread_system,
                                         Statistics* statistics, Timer* timer,
                                         int64 timeout_ms,
                                         MessageHandler* message_handler)
    : pool_(NULL),
      thread_system_(thread_system),
      timer_(timer),
      mutex_(NULL),
      serf_context_(NULL),
      threaded_fetcher_(NULL),
      active_count_(NULL),
      request_count_(NULL),
      byte_count_(NULL),
      time_duration_ms_(NULL),
      cancel_count_(NULL),
      timeout_count_(NULL),
      failure_count_(NULL),
      cert_errors_(NULL),
      timeout_ms_(timeout_ms),
      shutdown_(false),
      list_outstanding_urls_on_error_(false),
      track_original_content_length_(false),
      https_options_(0),
      message_handler_(message_handler) {
  CHECK(statistics != NULL);
  request_count_  =
      statistics->GetVariable(SerfStats::kSerfFetchRequestCount);
  byte_count_ = statistics->GetVariable(SerfStats::kSerfFetchByteCount);
  time_duration_ms_ =
      statistics->GetVariable(SerfStats::kSerfFetchTimeDurationMs);
  cancel_count_ = statistics->GetVariable(SerfStats::kSerfFetchCancelCount);
  active_count_ = statistics->GetVariable(SerfStats::kSerfFetchActiveCount);
  timeout_count_ = statistics->GetVariable(SerfStats::kSerfFetchTimeoutCount);
  failure_count_ = statistics->GetVariable(SerfStats::kSerfFetchFailureCount);
  cert_errors_ = statistics->GetVariable(SerfStats::kSerfFetchCertErrors);
  Init(pool, proxy);
  threaded_fetcher_ = new SerfThreadedFetcher(this, proxy);
}

SerfUrlAsyncFetcher::SerfUrlAsyncFetcher(SerfUrlAsyncFetcher* parent,
                                         const char* proxy)
    : pool_(NULL),
      thread_system_(parent->thread_system_),
      timer_(parent->timer_),
      mutex_(NULL),
      serf_context_(NULL),
      threaded_fetcher_(NULL),
      active_count_(parent->active_count_),
      request_count_(parent->request_count_),
      byte_count_(parent->byte_count_),
      time_duration_ms_(parent->time_duration_ms_),
      cancel_count_(parent->cancel_count_),
      timeout_count_(parent->timeout_count_),
      failure_count_(parent->failure_count_),
      cert_errors_(parent->cert_errors_),
      timeout_ms_(parent->timeout_ms()),
      shutdown_(false),
      list_outstanding_urls_on_error_(parent->list_outstanding_urls_on_error_),
      track_original_content_length_(parent->track_original_content_length_),
      https_options_(parent->https_options_),
      message_handler_(parent->message_handler_) {
  Init(parent->pool(), proxy);
}

SerfUrlAsyncFetcher::~SerfUrlAsyncFetcher() {
  CancelActiveFetches();
  completed_fetches_.DeleteAll();
  int orphaned_fetches = active_fetches_.size();
  if (orphaned_fetches != 0) {
    message_handler_->Message(
        kError, "SerfFetcher destructed with %d orphaned fetches.",
        orphaned_fetches);
    if (active_count_ != NULL) {
      active_count_->Add(-orphaned_fetches);
    }
    if (cancel_count_ != NULL) {
      cancel_count_->Add(orphaned_fetches);
    }
  }

  active_fetches_.DeleteAll();
  if (threaded_fetcher_ != NULL) {
    delete threaded_fetcher_;
  }
  delete mutex_;
  apr_pool_destroy(pool_);  // also calls apr_allocator_destroy on the allocator
}

void SerfUrlAsyncFetcher::ShutDown() {
  // Note that we choose not to delete the threaded_fetcher_ to avoid worrying
  // about races on its deletion.
  if (threaded_fetcher_ != NULL) {
    threaded_fetcher_->ShutDown();
  }

  ScopedMutex lock(mutex_);
  shutdown_ = true;
  CancelActiveFetchesMutexHeld();
}

void SerfUrlAsyncFetcher::Init(apr_pool_t* parent_pool, const char* proxy) {
  // Here, we give each our Serf threads' (main and work) separate pools
  // with separate threadsafe allocators.
  pool_ = AprCreateThreadCompatiblePool(parent_pool);
  mutex_ = thread_system_->NewMutex();
  serf_context_ = serf_context_create(pool_);

  if (!SetupProxy(proxy)) {
    message_handler_->Message(kError, "Proxy failed: %s", proxy);
  }
}

void SerfUrlAsyncFetcher::CancelActiveFetches() {
  ScopedMutex lock(mutex_);
  CancelActiveFetchesMutexHeld();
}

void SerfUrlAsyncFetcher::CancelActiveFetchesMutexHeld() {
  // If there are still active requests, cancel them.
  int num_canceled = 0;
  while (!active_fetches_.empty()) {
    // Canceling a fetch requires that the fetch reside in active_fetches_,
    // but can invalidate iterators pointing to the affected fetch.  To avoid
    // trouble, we simply ask for the oldest element, knowing it will go away.
    SerfFetch* fetch = active_fetches_.oldest();
    LOG(WARNING) << "Aborting fetch of " << fetch->str_url();
    fetch->Cancel();
    ++num_canceled;
  }

  if (num_canceled != 0) {
    if (cancel_count_ != NULL) {
      cancel_count_->Add(num_canceled);
    }
  }
}

bool SerfUrlAsyncFetcher::StartFetch(SerfFetch* fetch) {
  bool started = !shutdown_ && fetch->Start(this);
  if (started) {
    active_fetches_.Add(fetch);
    active_count_->Add(1);
  } else {
    fetch->message_handler()->Message(kWarning, "Fetch failed to start: %s",
                                      fetch->str_url());
    fetch->CallbackDone(false);
    delete fetch;
  }
  return started;
}

void SerfUrlAsyncFetcher::Fetch(const GoogleString& url,
                                MessageHandler* message_handler,
                                AsyncFetch* async_fetch) {
  async_fetch = EnableInflation(async_fetch);
  SerfFetch* fetch = new SerfFetch(url, async_fetch, message_handler, timer_);

  request_count_->Add(1);
  threaded_fetcher_->InitiateFetch(fetch);

  // TODO(morlovich): There is quite a bit of code related to doing work
  // both on 'this' and threaded_fetcher_ that could use cleaning up.
}

void SerfUrlAsyncFetcher::PrintActiveFetches(
    MessageHandler* handler) const {
  ScopedMutex mutex(mutex_);
  for (SerfFetchPool::const_iterator p = active_fetches_.begin(),
           e = active_fetches_.end(); p != e; ++p) {
    SerfFetch* fetch = *p;
    handler->Message(kInfo, "Active fetch: %s",
                     fetch->str_url());
  }
}

// If active_fetches_ is empty, this does no work and returns 0.
int SerfUrlAsyncFetcher::Poll(int64 max_wait_ms) {
  // Run serf polling up to microseconds.
  ScopedMutex mutex(mutex_);
  if (!active_fetches_.empty()) {
    apr_status_t status =
        serf_context_run(serf_context_, 1000*max_wait_ms, pool_);
    completed_fetches_.DeleteAll();
    if (APR_STATUS_IS_TIMEUP(status)) {
      // Remove expired fetches from the front of the queue.
      // This relies on the insertion-ordering guarantee
      // provided by the Pool iterator.
      int64 stale_cutoff = timer_->NowMs() - timeout_ms_;
      // This loop calls Cancel, which deletes a fetch and thus invalidates
      // iterators; we thus rely on retrieving oldest().
      while (!active_fetches_.empty()) {
        SerfFetch* fetch = active_fetches_.oldest();
        if (fetch->fetch_start_ms() >= stale_cutoff) {
          // This and subsequent fetches are still active, so we're done.
          break;
        }
        message_handler_->Message(
            kWarning, "Fetch timed out: %s (%ld) waiting for %ld ms",
            fetch->str_url(),
            static_cast<long>(active_fetches_.size()),  // NOLINT
            static_cast<long>(max_wait_ms));            // NOLINT
        // Note that canceling the fetch will ultimately call FetchComplete and
        // delete it from the pool.
        if (timeout_count_ != NULL) {
          timeout_count_->Add(1);
        }
        fetch->Cancel();
      }
    }
    bool success = ((status == APR_SUCCESS) || APR_STATUS_IS_TIMEUP(status));
    // TODO(jmarantz): provide the success status to the caller if there is a
    // need.
    if (!success && !active_fetches_.empty()) {
      // TODO(jmarantz): I have a new theory that we are getting
      // behind when our self-directed URL fetches queue up multiple
      // requests for the same URL, which might be sending the Serf
      // library into an n^2 situation with its polling, even though
      // we are using an rb_tree to hold the active fetches.  We
      // should fix this by keeping a map from url->SerfFetch, where
      // we'd have to store lists of Callback*, ResponseHeader*, Writer* so
      // all interested parties were updated if and when the fetch finally
      // completed.
      // NOTE(jmaessen): this is actually hard because all the above data is
      // process-local, and the multiple requests are likely cross-process.
      //
      // In the meantime by putting more detail into the log here, we'll
      // know whether we are accumulating active fetches to make the
      // server fall over.
      message_handler_->Message(
          kError,
          "Serf status %d(%s) polling for %ld %s fetches for %g seconds",
          status, GetAprErrorString(status).c_str(),
          static_cast<long>(active_fetches_.size()),  // NOLINT
          (threaded_fetcher_ == NULL) ? "threaded" : "non-blocking",
          max_wait_ms/1.0e3);
      if (list_outstanding_urls_on_error_) {
        int64 now_ms = timer_->NowMs();
        for (Pool<SerfFetch>::iterator p = active_fetches_.begin(),
                 e = active_fetches_.end(); p != e; ++p) {
          SerfFetch* fetch = *p;
          int64 age_ms = now_ms - fetch->fetch_start_ms();
          message_handler_->Message(kError, "URL %s active for %ld ms",
                                    fetch->str_url(),
                                    static_cast<long>(age_ms));  // NOLINT
        }
      }
      CleanupFetchesWithErrors();
    }
  }
  return active_fetches_.size();
}

void SerfUrlAsyncFetcher::FetchComplete(SerfFetch* fetch) {
  // We do not have a ScopedMutex in FetchComplete, because it is only
  // called from Poll and CancelActiveFetches, which have ScopedMutexes.
  // Note that SerfFetch::Cancel is currently not exposed from outside this
  // class.
  active_fetches_.Remove(fetch);
  completed_fetches_.Add(fetch);
}

void SerfUrlAsyncFetcher::ReportCompletedFetchStats(SerfFetch* fetch) {
  if (time_duration_ms_) {
    time_duration_ms_->Add(fetch->TimeDuration());
  }
  if (byte_count_) {
    byte_count_->Add(fetch->bytes_received());
  }
  if (active_count_) {
    active_count_->Add(-1);
  }
}

bool SerfUrlAsyncFetcher::AnyPendingFetches() {
  ScopedMutex lock(mutex_);
  return !active_fetches_.empty();
}

int SerfUrlAsyncFetcher:: ApproximateNumActiveFetches() {
  ScopedMutex lock(mutex_);
  return active_fetches_.size();
}

bool SerfUrlAsyncFetcher::WaitForActiveFetches(
    int64 max_ms, MessageHandler* message_handler, WaitChoice wait_choice) {
  bool ret = true;
  if ((threaded_fetcher_ != NULL) && (wait_choice != kMainlineOnly)) {
    ret &= threaded_fetcher_->WaitForActiveFetchesHelper(
        max_ms, message_handler);
  }
  if (wait_choice != kThreadedOnly) {
    ret &= WaitForActiveFetchesHelper(max_ms, message_handler);
  }
  return ret;
}

bool SerfUrlAsyncFetcher::WaitForActiveFetchesHelper(
    int64 max_ms, MessageHandler* message_handler) {
  bool any_pending_fetches = AnyPendingFetches();
  if (any_pending_fetches) {
    int64 now_ms = timer_->NowMs();
    int64 end_ms = now_ms + max_ms;
    while ((now_ms < end_ms) && any_pending_fetches) {
      int64 remaining_ms = end_ms - now_ms;
      SERF_DEBUG(LOG(INFO) << "Blocking process waiting " << remaining_ms
                 << "ms for " << ApproximateNumActiveFetches()
                 << " fetches to complete");
      SERF_DEBUG(PrintActiveFetches(message_handler));
      Poll(remaining_ms);
      now_ms = timer_->NowMs();
      any_pending_fetches = AnyPendingFetches();
    }
    if (any_pending_fetches) {
      message_handler->Message(
          kError, "Serf timeout waiting for fetches to complete:");
      PrintActiveFetches(message_handler);
      return false;
    }
    SERF_DEBUG(LOG(INFO) << "Serf successfully completed "
               << ApproximateNumActiveFetches() << " active fetches");
  }
  return true;
}

void SerfUrlAsyncFetcher::CleanupFetchesWithErrors() {
  // Create a copy of list of active fetches, as we may have to cancel
  // some failed ones, modifying the list.
  std::vector<SerfFetch*> fetches;
  for (SerfFetchPool::iterator i = active_fetches_.begin();
       i != active_fetches_.end(); ++i) {
    fetches.push_back(*i);
  }

  // Check each fetch to see if it needs cleanup because its serf connection
  // got into an error state.
  for (int i = 0, size = fetches.size(); i < size; ++i) {
    fetches[i]->CleanupIfError();
  }
}

void SerfUrlAsyncFetcher::InitStats(Statistics* statistics) {
  statistics->AddVariable(SerfStats::kSerfFetchRequestCount);
  statistics->AddVariable(SerfStats::kSerfFetchByteCount);
  statistics->AddVariable(SerfStats::kSerfFetchTimeDurationMs);
  statistics->AddVariable(SerfStats::kSerfFetchCancelCount);
  statistics->AddVariable(SerfStats::kSerfFetchActiveCount);
  statistics->AddVariable(SerfStats::kSerfFetchTimeoutCount);
  statistics->AddVariable(SerfStats::kSerfFetchFailureCount);
  statistics->AddVariable(SerfStats::kSerfFetchCertErrors);
}

void SerfUrlAsyncFetcher::set_list_outstanding_urls_on_error(bool x) {
  list_outstanding_urls_on_error_ = x;
  if (threaded_fetcher_ != NULL) {
    threaded_fetcher_->set_list_outstanding_urls_on_error(x);
  }
}

void SerfUrlAsyncFetcher::set_track_original_content_length(bool x) {
  track_original_content_length_ = x;
  if (threaded_fetcher_ != NULL) {
    threaded_fetcher_->set_track_original_content_length(x);
  }
}

bool SerfUrlAsyncFetcher::ParseHttpsOptions(StringPiece directive,
                                            uint32* options,
                                            GoogleString* error_message) {
  StringPieceVector keywords;
  SplitStringPieceToVector(directive, ",", &keywords, true);
  uint32 https_options = 0;
  for (int i = 0, n = keywords.size(); i < n; ++i) {
    StringPiece keyword = keywords[i];
    if (keyword == "enable") {
      https_options |= kEnableHttps;
    } else if (keyword == "disable") {
      https_options &= ~static_cast<uint32>(kEnableHttps);
    } else if (keyword == "allow_self_signed") {
      https_options |= kAllowSelfSigned;
    } else if (keyword == "allow_unknown_certificate_authority") {
      https_options |= kAllowUnknownCertificateAuthority;
    } else if (keyword == "allow_certificate_not_yet_valid") {
      https_options |= kAllowCertificateNotYetValid;
    } else {
      StrAppend(error_message,
                "Invalid HTTPS keyword: ", keyword, ", legal options are: "
                SERF_HTTPS_KEYWORDS);
      return false;
    }
  }
  *options = https_options;
  return true;
}

const char* SerfUrlAsyncFetcher::ExtractHostHeader(
    const apr_uri_t& uri, apr_pool_t* pool) {
  // Construct it ourselves from URL. Note that we shouldn't include the
  // user info here, just host and any explicit port. The reason this is done
  // with APR functions and not GoogleUrl is that APR URLs are what we have,
  // as that's what Serf takes.
  const char* host = apr_uri_unparse(pool, &uri,
                                     APR_URI_UNP_OMITPATHINFO |
                                     APR_URI_UNP_OMITUSERINFO);
  // This still normally has the scheme, which we should drop.
  stringpiece_ssize_type slash_pos = StringPiece(host).find_last_of('/');
  if (slash_pos != StringPiece::npos) {
    host += (slash_pos + 1);
  }
  return host;
}


GoogleString SerfUrlAsyncFetcher::RemovePortFromHostHeader(
    const GoogleString& host) {
  // SNI hosts, unlike Host: do not have a port number, so remove it.
  // Note that the input isn't a URL, so using GoogleUrl would be awkward and
  // a bit of an overkill. We need to be a bit careful, however, since IPv6
  // Also uses :, but inside [].
  size_t colon_pos = StringPiece(host).find_last_of(':');
  size_t bracket_pos = StringPiece(host).find_last_of(']');
  if (colon_pos == std::string::npos ||
      (bracket_pos != std::string::npos && colon_pos < bracket_pos)) {
    return host;
  } else {
    return host.substr(0, colon_pos);
  }
}

bool SerfUrlAsyncFetcher::SetHttpsOptions(StringPiece directive) {
  GoogleString error_message;
  if (!ParseHttpsOptions(directive, &https_options_, &error_message)) {
    message_handler_->Message(kError, "%s", error_message.c_str());
    return false;
  }

#if !SERF_HTTPS_FETCHING
  if (allow_https()) {
    message_handler_->Message(kError, "HTTPS fetching has not been compiled "
                              "into the binary, so it has not been enabled.");
    https_options_ = 0;
  }
#endif
  if (threaded_fetcher_ != NULL) {
    threaded_fetcher_->set_https_options(https_options_);
  }
  return true;
}

void SerfUrlAsyncFetcher::SetSslCertificatesDir(StringPiece dir) {
  dir.CopyToString(&ssl_certificates_dir_);
  if (threaded_fetcher_ != NULL) {
    threaded_fetcher_->SetSslCertificatesDir(dir);
  }
}

void SerfUrlAsyncFetcher::SetSslCertificatesFile(StringPiece file) {
  file.CopyToString(&ssl_certificates_file_);
  if (threaded_fetcher_ != NULL) {
    threaded_fetcher_->SetSslCertificatesFile(file);
  }
}

bool SerfUrlAsyncFetcher::allow_https() const {
  return ((https_options_ & kEnableHttps) != 0);
}

bool SerfUrlAsyncFetcher::allow_self_signed() const {
  return ((https_options_ & kAllowSelfSigned) != 0);
}

bool SerfUrlAsyncFetcher::allow_unknown_certificate_authority() const {
  return ((https_options_ & kAllowUnknownCertificateAuthority) != 0);
}

bool SerfUrlAsyncFetcher::allow_certificate_not_yet_valid() const {
  return ((https_options_ & kAllowCertificateNotYetValid) != 0);
}

bool SerfUrlAsyncFetcher::SupportsHttps() const {
  return allow_https();
}

}  // namespace net_instaweb
