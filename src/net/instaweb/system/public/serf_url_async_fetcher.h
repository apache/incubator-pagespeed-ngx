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

#ifndef NET_INSTAWEB_SYSTEM_PUBLIC_SERF_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_SYSTEM_PUBLIC_SERF_URL_ASYNC_FETCHER_H_

#include <set>
#include <vector>

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/pool.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"

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
struct serf_context_t;

namespace net_instaweb {

class AsyncFetch;
class MessageHandler;
class Statistics;
class SerfFetch;
class SerfThreadedFetcher;
class Timer;
class Variable;
struct ContentType;

struct SerfStats {
  static const char kSerfFetchRequestCount[];
  static const char kSerfFetchByteCount[];
  static const char kSerfFetchTimeDurationMs[];
  static const char kSerfFetchCancelCount[];
  static const char kSerfFetchActiveCount[];
  static const char kSerfFetchTimeoutCount[];
  static const char kSerfFetchFailureCount[];
  static const char kSerfFetchCertErrors[];
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
  void ReportCompletedFetchStats(SerfFetch* fetch);

  apr_pool_t* pool() const { return pool_; }
  serf_context_t* serf_context() const { return serf_context_; }

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

  void set_inflation_content_type_blacklist(
      const std::set<const ContentType*>& bypass_set) {
    inflation_content_type_blacklist_ = bypass_set;
  }

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

  void Init(apr_pool_t* parent_pool, const char* proxy);
  bool SetupProxy(const char* proxy);

  // Start a SerfFetch. Takes ownership of fetch and makes sure callback is
  // called even if fetch fails to start.
  //
  // mutex_ must be held before calling StartFetch.
  bool StartFetch(SerfFetch* fetch);

  // AnyPendingFetches is accurate only at the time of call; this is
  // used conservatively during shutdown.  It counts fetches that have been
  // requested by some thread, and can include fetches for which no action
  // has yet been taken (ie fetches that are not active).
  virtual bool AnyPendingFetches();
  // ApproximateNumActiveFetches can under- or over-count and is used only for
  // error reporting.
  int ApproximateNumActiveFetches();

  void CancelActiveFetches();
  void CancelActiveFetchesMutexHeld();
  bool WaitForActiveFetchesHelper(int64 max_ms,
                                  MessageHandler* message_handler);

  // This cleans up the serf resources for fetches that errored out.
  // Must be called only immediately after running the serf event loop.
  // Must be called with mutex_ held.
  void CleanupFetchesWithErrors();

  // These must be accessed with mutex_ held.
  bool shutdown() const { return shutdown_; }
  void set_shutdown(bool s) { shutdown_ = s; }

  apr_pool_t* pool_;
  ThreadSystem* thread_system_;
  Timer* timer_;

  // mutex_ protects serf_context_ and active_fetches_.
  ThreadSystem::CondvarCapableMutex* mutex_;
  serf_context_t* serf_context_;
  SerfFetchPool active_fetches_;

  typedef std::vector<SerfFetch*> FetchVector;
  SerfFetchPool completed_fetches_;
  SerfThreadedFetcher* threaded_fetcher_;

  // This is protected because it's updated along with active_fetches_,
  // which happens in subclass SerfThreadedFetcher as well as this class.
  Variable* active_count_;

 private:
  friend class SerfFetch;  // To access stats variables below.

  static bool ParseHttpsOptions(StringPiece directive, uint32* options,
                                GoogleString* error_message);

  Variable* request_count_;
  Variable* byte_count_;
  Variable* time_duration_ms_;
  Variable* cancel_count_;
  Variable* timeout_count_;
  Variable* failure_count_;
  Variable* cert_errors_;
  const int64 timeout_ms_;
  bool shutdown_;
  bool list_outstanding_urls_on_error_;
  bool track_original_content_length_;
  uint32 https_options_;  // Composed of HttpsOptions ORed together.
  MessageHandler* message_handler_;
  GoogleString ssl_certificates_dir_;
  GoogleString ssl_certificates_file_;

  // Set of content types that will not be inflated, when passing through
  // inflating fetch.
  std::set<const ContentType*> inflation_content_type_blacklist_;

  DISALLOW_COPY_AND_ASSIGN(SerfUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SYSTEM_PUBLIC_SERF_URL_ASYNC_FETCHER_H_
