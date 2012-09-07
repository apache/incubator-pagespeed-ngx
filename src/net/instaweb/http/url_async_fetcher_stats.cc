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

// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/http/public/url_async_fetcher_stats.h"

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace {

const int kFetchLatencyUsHistogramMaxValue = 500 * 1000;

const char kFetchLatencyUsHistogram[] = "_fetch_latency_us";

const char kFetches[] = "_fetches";
const char kBytesFetched[] = "_bytes_fetched";
const char kApproxHeaderBytesFetched[] = "_approx_header_bytes_fetched";

}  // namespace

namespace net_instaweb {

class UrlAsyncFetcherStats::StatsAsyncFetch : public SharedAsyncFetch {
 public:
  StatsAsyncFetch(UrlAsyncFetcherStats* stats_fetcher,
                  AsyncFetch* base_fetch)
      : SharedAsyncFetch(base_fetch),
        stats_fetcher_(stats_fetcher),
        size_(0) {
    start_time_us_ = stats_fetcher_->timer_->NowUs();
  }

  virtual ~StatsAsyncFetch() {
  }

  virtual void HandleHeadersComplete() {
    stats_fetcher_->approx_header_bytes_fetched_->Add(
        response_headers()->SizeEstimate());
    SharedAsyncFetch::HandleHeadersComplete();
  }

  virtual void HandleDone(bool success) {
    int64 end_time_us = stats_fetcher_->timer_->NowUs();
    stats_fetcher_->fetch_latency_us_histogram_->Add(
        end_time_us - start_time_us_);
    stats_fetcher_->fetches_->Add(1);
    stats_fetcher_->bytes_fetched_->Add(size_);

    SharedAsyncFetch::HandleDone(success);
    delete this;
  }

  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    size_ += content.size();
    return SharedAsyncFetch::HandleWrite(content, handler);
  }

 private:
  UrlAsyncFetcherStats* stats_fetcher_;
  int64 start_time_us_;
  int64 size_;

  DISALLOW_COPY_AND_ASSIGN(StatsAsyncFetch);
};

UrlAsyncFetcherStats::UrlAsyncFetcherStats(StringPiece prefix,
                                           UrlAsyncFetcher* base_fetcher,
                                           Timer* timer,
                                           Statistics* statistics)
    : base_fetcher_(base_fetcher),
      timer_(timer),
      fetch_latency_us_histogram_(statistics->GetHistogram(
          StrCat(prefix, kFetchLatencyUsHistogram))),
      fetches_(statistics->GetVariable(StrCat(prefix, kFetches))),
      bytes_fetched_(statistics->GetVariable(StrCat(prefix, kBytesFetched))),
      approx_header_bytes_fetched_(
          statistics->GetVariable(StrCat(prefix, kApproxHeaderBytesFetched))) {
  fetch_latency_us_histogram_->SetMaxValue(kFetchLatencyUsHistogramMaxValue);

  DCHECK(!base_fetcher->fetch_with_gzip())
      << "A fetcher wrapped by UrlAsyncFetcherStats should not be handling "
      << "gzip itself, but rather letting UrlAsyncFetcherStats handle it";
}

UrlAsyncFetcherStats::~UrlAsyncFetcherStats() {
}

void UrlAsyncFetcherStats::InitStats(StringPiece prefix,
                                     Statistics* statistics) {
  Histogram* fetch_latency_us_histogram =
      statistics->AddHistogram(StrCat(prefix, kFetchLatencyUsHistogram));
  fetch_latency_us_histogram->SetMaxValue(kFetchLatencyUsHistogramMaxValue);
  statistics->AddVariable(StrCat(prefix, kFetches));
  statistics->AddVariable(StrCat(prefix, kBytesFetched));
  statistics->AddVariable(StrCat(prefix, kApproxHeaderBytesFetched));
}

bool UrlAsyncFetcherStats::SupportsHttps() const {
  return base_fetcher_->SupportsHttps();
}


bool UrlAsyncFetcherStats::Fetch(const GoogleString& url,
                                 MessageHandler* message_handler,
                                 AsyncFetch* fetch) {
  fetch = EnableInflation(fetch);
  return base_fetcher_->Fetch(url, message_handler,
                              new StatsAsyncFetch(this, fetch));
}

int64 UrlAsyncFetcherStats::timeout_ms() {
  return base_fetcher_->timeout_ms();
}

void UrlAsyncFetcherStats::ShutDown() {
  base_fetcher_->ShutDown();
}

}  // namespace net_instaweb
