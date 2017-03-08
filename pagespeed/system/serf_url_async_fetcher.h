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

#ifndef PAGESPEED_SYSTEM_SERF_URL_ASYNC_FETCHER_H_
#define PAGESPEED_SYSTEM_SERF_URL_ASYNC_FETCHER_H_

#include <cstddef>
#include <vector>

#include "apr_network_io.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest_prod.h"
#include "pagespeed/kernel/base/pool.h"
#include "pagespeed/kernel/base/pool_element.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/http/response_headers_parser.h"

#include "third_party/serf/src/serf.h"

// To enable HTTPS fetching with serf, we must link against OpenSSL,
// which is a a large library with licensing restrictions not known to
// be wholly inline with the Apache license.  To disable HTTPS fetching:
//   1. Set SERF_HTTPS_FETCHING to 0 here
//   2. Comment out the references to openssl.gyp and ssl_buckets.c in
//      src/third_party/serf/serf.gyp.
//   3. Comment out all references to openssl in src/DEPS.
//
// If this is enabled, then the HTTPS fetching can be tested with
//    install/apache_https_fetch_test.sh
#ifndef SERF_HTTPS_FETCHING
#define SERF_HTTPS_FETCHING 1
#endif

struct apr_pool_t;
struct apr_uri_t;
struct serf_context_t;

namespace net_instaweb {

class AsyncFetch;
class MessageHandler;
class Statistics;
class SerfFetch;
class SerfThreadedFetcher;
class Timer;
class UpDownCounter;
class Variable;

struct SerfStats {
  static const char kSerfFetchRequestCount[];
  static const char kSerfFetchByteCount[];
  static const char kSerfFetchTimeDurationMs[];
  static const char kSerfFetchCancelCount[];
  static const char kSerfFetchActiveCount[];
  static const char kSerfFetchTimeoutCount[];
  static const char kSerfFetchFailureCount[];
  static const char kSerfFetchCertErrors[];
  static const char kSerfFetchReadCalls[];

  // A fetch that finished with a 2xx or a 3xx code --- and not just a
  // mechanically successful one that's a 4xx or such.
  static const char kSerfFetchUltimateSuccess[];

  // A failure or an error status. Doesn't include fetches dropped due to
  // process exit and the like.
  static const char kSerfFetchUltimateFailure[];

  // When we last checked the ultimate failure/success numbers for a
  // possible concern.
  static const char kSerfFetchLastCheckTimestampMs[];
};

enum class SerfCompletionResult {
  kClientCancel,
  kSuccess,
  kFailure
};

// Identifies the set of HTML keywords.  This is used in error messages emitted
// both from the config parser in this module, and in the directives table in
// mod_instaweb.cc which must be statically constructed using a compile-time
// concatenation.  Hence this must be a literal string and not a const char*.
#define SERF_HTTPS_KEYWORDS \
  "enable,disable,allow_self_signed," \
  "allow_unknown_certificate_authority,allow_certificate_not_yet_valid"

// TODO(sligocki): Serf does not seem to act appropriately in IPv6
// environments, fix and test this.
// Specifically:
//   (1) It does not attempt to fall-back to IPv4 if IPv6 connection fails;
//   (2) It may not correctly signal failure, which causes the incoming
//       connection to hang.
class SerfUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  enum WaitChoice {
    kThreadedOnly,
    kMainlineOnly,
    kThreadedAndMainline
  };

  SerfUrlAsyncFetcher(const char* proxy, apr_pool_t* pool,
                      ThreadSystem* thread_system,
                      Statistics* statistics, Timer* timer, int64 timeout_ms,
                      MessageHandler* handler);
  SerfUrlAsyncFetcher(SerfUrlAsyncFetcher* parent, const char* proxy);
  virtual ~SerfUrlAsyncFetcher();

  static void InitStats(Statistics* statistics);

  // Stops all active fetches and prevents further fetches from starting
  // (they will instead quickly call back to ->Done(false).
  virtual void ShutDown();

  virtual bool SupportsHttps() const;

  virtual void Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* callback);
  // TODO(morlovich): Make private once non-thread mode concept removed.
  int Poll(int64 max_wait_ms);

  bool WaitForActiveFetches(int64 max_milliseconds,
                            MessageHandler* message_handler,
                            WaitChoice wait_choice);

  // Remove the completed fetch from the active fetch set, and put it into a
  // completed fetch list to be cleaned up.
  void FetchComplete(SerfFetch* fetch);

  // Update the statistics object with results of the (completed) fetch.
  void ReportCompletedFetchStats(const SerfFetch* fetch);

  // Updates states used for success/failure monitoring.
  void ReportFetchSuccessStats(SerfCompletionResult result,
                               const ResponseHeaders* headers,
                               const SerfFetch* fetch);


  apr_pool_t* pool() const { return pool_; }

  void PrintActiveFetches(MessageHandler* handler) const;
  virtual int64 timeout_ms() { return timeout_ms_; }
  ThreadSystem* thread_system() { return thread_system_; }

  // Indicates that Serf should enumerate failing URLs whenever the underlying
  // Serf library reports an error.
  void set_list_outstanding_urls_on_error(bool x);

  // Indicates that Serf should track the original content length for
  // fetched resources.
  bool track_original_content_length() const {
    return track_original_content_length_;
  }
  void set_track_original_content_length(bool x);

  // Indicates that direct HTTPS fetching should be allowed, and how picky
  // to be about certificates.  The directive is a comma separated list of
  // these keywords:
  //   enable
  //   disable
  //   allow_self_signed
  //   allow_unknown_certificate_authority
  //   allow_certificate_not_yet_valid
  // Returns 'false' if the directive does not parse properly.
  bool SetHttpsOptions(StringPiece directive);

  // Validates the correctness of an https directive.  Exposed as a static
  // method for early exit on mis-specified pagespeed.conf.
  static bool ValidateHttpsOptions(StringPiece directive,
                                   GoogleString* error_message) {
    uint32 options;
    return ParseHttpsOptions(directive, &options, error_message);
  }

  void SetSslCertificatesDir(StringPiece dir);
  const GoogleString& ssl_certificates_dir() const {
    return ssl_certificates_dir_;
  }

  void SetSslCertificatesFile(StringPiece file);
  const GoogleString& ssl_certificates_file() const {
    return ssl_certificates_file_;
  }

 protected:
  typedef Pool<SerfFetch> SerfFetchPool;

  // Determines whether https is allowed in the current configuration.
  inline bool allow_https() const;
  inline bool allow_self_signed() const;
  inline bool allow_unknown_certificate_authority() const;
  inline bool allow_certificate_not_yet_valid() const;

  void set_https_options(uint32 https_options) {
    https_options_ = https_options;
  }

  void Init(apr_pool_t* parent_pool, const char* proxy)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool SetupProxy(const char* proxy) EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Start a SerfFetch. Takes ownership of fetch and makes sure callback is
  // called even if fetch fails to start.
  bool StartFetch(SerfFetch* fetch) EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // AnyPendingFetches is accurate only at the time of call; this is
  // used conservatively during shutdown.  It counts fetches that have been
  // requested by some thread, and can include fetches for which no action
  // has yet been taken (ie fetches that are not active).
  virtual bool AnyPendingFetches();
  // ApproximateNumActiveFetches can under- or over-count and is used only for
  // error reporting.
  int ApproximateNumActiveFetches();

  void CancelActiveFetches();
  void CancelActiveFetchesMutexHeld() EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool WaitForActiveFetchesHelper(int64 max_ms,
                                  MessageHandler* message_handler);

  // This cleans up the serf resources for fetches that errored out.
  // Must be called only immediately after running the serf event loop.
  void CleanupFetchesWithErrors() EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool shutdown() const EXCLUSIVE_LOCKS_REQUIRED(mutex_) { return shutdown_; }
  void set_shutdown(bool s) EXCLUSIVE_LOCKS_REQUIRED(mutex_) { shutdown_ = s; }

  apr_pool_t* pool_;
  ThreadSystem* thread_system_;
  Timer* timer_;

  // mutex_ protects serf_context_, active_fetches_ and shutdown_. It's
  // protected because SerfThreadedFetcher needs access.
  ThreadSystem::CondvarCapableMutex* mutex_;

  typedef std::vector<SerfFetch*> FetchVector;
  SerfFetchPool completed_fetches_;
  SerfThreadedFetcher* threaded_fetcher_;

  // This is protected because it's updated along with active_fetches_,
  // which happens in subclass SerfThreadedFetcher as well as this class.
  UpDownCounter* active_count_;

 private:
  friend class SerfFetch;  // To access stats variables below.

  // Note: returned string memory substring of memory in the pool.
  static const char* ExtractHostHeader(const apr_uri_t& uri,
                                       apr_pool_t* pool);
  FRIEND_TEST(SerfUrlAsyncFetcherTest, TestHostConstruction);

  // Transforms Host: header into SNI host name by dropping the port.
  // Exposed for testability
  static GoogleString RemovePortFromHostHeader(const GoogleString& in);
  FRIEND_TEST(SerfUrlAsyncFetcherTest, TestPortRemoval);

  static bool ParseHttpsOptions(StringPiece directive, uint32* options,
                                GoogleString* error_message);

  serf_context_t* serf_context_ GUARDED_BY(mutex_);
  SerfFetchPool active_fetches_ GUARDED_BY(mutex_);

  Variable* request_count_;
  Variable* byte_count_;
  Variable* time_duration_ms_;
  Variable* cancel_count_;
  Variable* timeout_count_;
  Variable* failure_count_;
  Variable* cert_errors_;
  Variable* read_calls_count_;  // Non-NULL only on debug builds.
  Variable* ultimate_success_;
  Variable* ultimate_failure_;
  UpDownCounter* last_check_timestamp_ms_;
  const int64 timeout_ms_;
  bool shutdown_ GUARDED_BY(mutex_);
  bool list_outstanding_urls_on_error_;
  bool track_original_content_length_;
  uint32 https_options_;  // Composed of HttpsOptions ORed together.
  MessageHandler* message_handler_;
  GoogleString ssl_certificates_dir_;
  GoogleString ssl_certificates_file_;

  DISALLOW_COPY_AND_ASSIGN(SerfUrlAsyncFetcher);
};

// TODO(lsong): Move this to a separate file. Necessary?
class SerfFetch : public PoolElement<SerfFetch> {
 public:
  enum class CancelCause {
    kClientDecision,
    kSerfError,
    kFetchTimeout,
  };

  // TODO(lsong): make use of request_headers.
  SerfFetch(const GoogleString& url,
            AsyncFetch* async_fetch,
            MessageHandler* message_handler,
            Timer* timer);
  ~SerfFetch();

  // Start the fetch. It returns immediately.  This can only be run when
  // locked with fetcher->mutex_.
  bool Start(SerfUrlAsyncFetcher* fetcher, serf_context_t* context);

  GoogleString DebugInfo();

  // This must be called while holding SerfUrlAsyncFetcher's mutex_.
  void Cancel(CancelCause cause);

  // Calls the callback supplied by the user.  This needs to happen
  // exactly once.  In some error cases it appears that Serf calls
  // HandleResponse multiple times on the same object.
  //
  // This must be called while holding SerfUrlAsyncFetcher's mutex_.
  //
  // Note that when there are SSL error messages, we immediately call
  // CallCallback, which is robust against duplicate calls in that case.
  void CallCallback(SerfCompletionResult result);
  void CallbackDone(SerfCompletionResult result);

  // If last poll of this fetch's connection resulted in an error, clean it up.
  // Must be called after serf_context_run, with fetcher's mutex_ held.
  void CleanupIfError();

  // For use only by unit tests.  Calls ParseUrl(), then makes things available
  // for checking.
  void ParseUrlForTesting(bool* status,
                          apr_uri_t** url,
                          const char** host_header,
                          const char** sni_host);

  void SetFetcherForTesting(SerfUrlAsyncFetcher* fetcher);

  int64 TimeDuration() const;

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
  static apr_status_t SSLCertValidate(void *data, int failures,
                                   const serf_ssl_certificate_t *cert);

  static apr_status_t SSLCertChainValidate(
      void *data, int failures, int error_depth,
      const serf_ssl_certificate_t * const *certs,
      apr_size_t certs_count);
#endif

  static apr_status_t ConnectionSetup(
      apr_socket_t* socket, serf_bucket_t **read_bkt, serf_bucket_t **write_bkt,
      void* setup_baton, apr_pool_t* pool);
  static void ClosedConnection(serf_connection_t* conn,
                               void* closed_baton,
                               apr_status_t why,
                               apr_pool_t* pool);
  static serf_bucket_t* AcceptResponse(serf_request_t* request,
                                       serf_bucket_t* stream,
                                       void* acceptor_baton,
                                       apr_pool_t* pool);
  static apr_status_t HandleResponse(serf_request_t* request,
                                     serf_bucket_t* response,
                                     void* handler_baton,
                                     apr_pool_t* pool);
  // After a serf read operation, return true if the status indicates that
  // data might have been read. The "number of bytes read" paramater is not
  // guaranteed to be updated if the status is anything other than SUCCESS or
  // EOF. So, one must either zero "nread" before calling serf_bucket_read or
  // check for one of those statuses before consulting the value in "nread".
  static bool StatusIndicatesDataPossible(apr_status_t status);

#if SERF_HTTPS_FETCHING
  // Called indicating whether SSL certificate errors have occurred detected.
  // The function returns SUCCESS in all cases, but sets ssl_error_message_
  // non-null for errors as a signal to ReadHeaders that we should not let
  // any output thorugh.
  //
  // Interpretation of two of the error conditions is configurable:
  // 'allow_unknown_certificate_authority' and 'allow_self_signed'.
  //
  // If there is a cert that should be checked for a hostname match, that should
  // go in cert.  Otherwise cert should be null.
  apr_status_t HandleSSLCertValidation(
      int errors, int failure_depth, const serf_ssl_certificate_t *cert);
#endif

  apr_status_t HandleResponse(serf_bucket_t* response);

  apr_status_t ReadStatusLine(serf_bucket_t* response);

  // Parse the headers.  The dynamics of this appear that for N
  // headers we'll get 2N calls to serf_bucket_read: one each for
  // attribute names & values.
  apr_status_t ReadHeaders(serf_bucket_t* response);

  // Once headers are complete we can get the body.  The dynamics of this
  // are likely dependent on everything on the network between the client
  // and server, but for a 10k buffer I seem to frequently get 8k chunks.
  apr_status_t ReadBody(serf_bucket_t* response);

  // Ensures that a user-agent string is included, and that the mod_pagespeed
  // version is appended.
  void FixUserAgent();
  static apr_status_t SetupRequest(serf_request_t* request,
                                   void* setup_baton,
                                   serf_bucket_t** req_bkt,
                                   serf_response_acceptor_t* acceptor,
                                   void** acceptor_baton,
                                   serf_response_handler_t* handler,
                                   void** handler_baton,
                                   apr_pool_t* pool);
  bool ParseUrl();

  SerfUrlAsyncFetcher* fetcher_;
  Timer* timer_;
  const GoogleString str_url_;
  AsyncFetch* async_fetch_;
  ResponseHeadersParser parser_;
  bool status_line_read_;
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

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_SERF_URL_ASYNC_FETCHER_H_
