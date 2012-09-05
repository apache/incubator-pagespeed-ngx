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

// Author: morlovichz@google.com (Maksim Orlovich)
// Wrapper around a UrlAsyncFetcher that adds statistics and histograms.

#ifndef NET_INSTAWEB_HTTP_PUBLIC_URL_ASYNC_FETCHER_STATS_H_
#define NET_INSTAWEB_HTTP_PUBLIC_URL_ASYNC_FETCHER_STATS_H_

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AsyncFetch;
class Histogram;
class MessageHandler;
class Statistics;
class Timer;
class Variable;

class UrlAsyncFetcherStats : public UrlAsyncFetcher {
 public:
  // Creates a fetcher that delegates to base_fetcher, while collecting
  // statistics. The variables will be prefixed with 'prefix'; which must
  // have been passed to ::Initialize during statistics initialization process.
  //
  // Note that base_fetcher should not have fetch_with_gzip() as it would break
  // usage metering; if you want that functionality you should turn it off on
  // base_fetcher and turn it on UrlAsyncFetcherStats.
  //
  // Does not own base_fetcher (so you can have multiple UrlAsyncFetcherStats
  // objects around a single UrlAsyncFetcher object).
  UrlAsyncFetcherStats(StringPiece prefix,
                       UrlAsyncFetcher* base_fetcher,
                       Timer* timer,
                       Statistics* statistics);
  virtual ~UrlAsyncFetcherStats();

  // This must be called once for every unique prefix used with
  // UrlAsyncFetcherStats.
  static void Initialize(StringPiece prefix, Statistics* statistics);

  // Reimplementation of UrlAsyncFetcher methods. See base class
  // for API specifications.
  virtual bool SupportsHttps() const;
  virtual bool Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch);
  virtual int64 timeout_ms();
  virtual void ShutDown();

 private:
  class StatsAsyncFetch;

  UrlAsyncFetcher* base_fetcher_;
  Timer* timer_;

  Histogram* fetch_latency_us_histogram_;
  Variable* fetches_;
  Variable* bytes_fetched_;

  DISALLOW_COPY_AND_ASSIGN(UrlAsyncFetcherStats);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_URL_ASYNC_FETCHER_H_
