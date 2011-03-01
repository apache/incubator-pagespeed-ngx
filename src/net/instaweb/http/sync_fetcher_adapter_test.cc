//  Copyright 2011 Google Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

// Author: morlovich@google.com (Maksim Orlovich)

// Test the pollable async -> sync fetcher adapter and its callback helper

#include "net/instaweb/http/public/sync_fetcher_adapter.h"

#include <algorithm>

#include "base/basictypes.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

namespace {

const char kText[] = "Result";
const char kHeader[] = "X-Test-HeaderCopy";

// Writer that should never be invoked.
class TrapWriter : public Writer {
 public:
  TrapWriter() {}

  virtual bool Write(const StringPiece& str, MessageHandler* message_handler) {
    ADD_FAILURE() << "Should not do a Write";
    return false;
  }

  virtual bool Flush(MessageHandler* message_handler) {
    ADD_FAILURE() << "Should not do a Flush";
    return false;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(TrapWriter);
};

// A pollable fetcher that writes out a response at given number of microseconds
// elapsed, or when asked to immediately. Note that it's only capable of one
// fetch at a time!
class DelayedFetcher : public UrlPollableAsyncFetcher {
 public:
  // Note: if sim_delay <= 0, will report immediately at StreamingFetch
  DelayedFetcher(Timer* timer, MessageHandler* handler,
                 int64 sim_delay_ms, bool sim_success)
      : timer_(timer), handler_(handler), sim_delay_ms_(sim_delay_ms),
        sim_success_(sim_success) {
    CleanFetchSettings();
  }

  virtual bool StreamingFetch(const std::string& url,
                              const RequestHeaders& request_headers,
                              ResponseHeaders* response_headers,
                              Writer* response_writer,
                              MessageHandler* message_handler,
                              Callback* callback) {
    CHECK(!fetch_pending_);
    response_headers_ = response_headers;
    response_writer_ = response_writer;
    callback_ = callback;
    remaining_ms_ = sim_delay_ms_;
    fetch_pending_ = true;

    if (remaining_ms_ <= 0) {
      ReportResult();
      return true;
    } else {
      return false;
    }
  }

  virtual int Poll(int64 max_wait_ms) {
    if (fetch_pending_) {
      int64 delay_ms = std::min(max_wait_ms, remaining_ms_);
      timer_->SleepMs(delay_ms);
      remaining_ms_ -= delay_ms;

      if (remaining_ms_ <= 0) {
        ReportResult();
      }
    }

    return fetch_pending_ ? 1 : 0;
  }

 private:
  void CleanFetchSettings() {
    fetch_pending_ = false;

    // Defensively set active fetch variables to catch us doing something silly.
    response_headers_ = NULL;
    response_writer_ = NULL;
    callback_ = NULL;
    remaining_ms_ = 0;
  }

  void ReportResult() {
    ResponseHeaders headers;
    if (sim_success_) {
      response_headers_->CopyFrom(headers);
      response_headers_->Add(kHeader, kText);
      response_writer_->Write(kText, handler_);
      response_writer_->Flush(handler_);
    }
    callback_->Done(sim_success_);
    CleanFetchSettings();
  }

  // Simulation settings:
  Timer* timer_;
  MessageHandler* handler_;
  int64 sim_delay_ms_;  // how long till we report the result
  bool sim_success_;  // whether to report success or failure

  // Fetch session:
  bool fetch_pending_;
  ResponseHeaders* response_headers_;
  Writer* response_writer_;
  Callback* callback_;  // callback for current fetch
  int64 remaining_ms_;  // how much time left to report result of current fetch
};

}  // namespace

class SyncFetcherAdapterTest : public testing::Test {
 public:
  SyncFetcherAdapterTest(): timer_(0) {}

 protected:
  bool DoFetch(UrlFetcher* fetcher, ResponseHeaders* response_headers,
               Writer* response_writer) {
    RequestHeaders request_headers;
    return fetcher->StreamingFetchUrl("http://www.example.com/",
                                      request_headers,
                                      response_headers,
                                      response_writer,
                                      &handler_);
  }

  void TestSuccessfulFetch(UrlFetcher* fetcher) {
    ResponseHeaders out_headers;
    std::string out_str;
    StringWriter out_writer(&out_str);
    EXPECT_TRUE(DoFetch(fetcher, &out_headers, &out_writer));
    EXPECT_EQ(kText, out_str);

    StringStarVector values;
    EXPECT_TRUE(out_headers.Lookup(kHeader, &values));
    ASSERT_EQ(1, values.size());
    EXPECT_EQ(std::string(kText), *(values[0]));
  }

  void TestFailedFetch(UrlFetcher* fetcher) {
    ResponseHeaders out_headers;
    TrapWriter trap_writer;
    EXPECT_FALSE(DoFetch(fetcher, &out_headers, &trap_writer));
  }

  void TestTimeoutFetch(UrlPollableAsyncFetcher* async_fetcher,
                        UrlFetcher* fetcher) {
    // First let the sync fetcher tineout, and return failure.
    TestFailedFetch(fetcher);

    // Now spin until async fetcher delivers the result, to make sure
    // we do not blow up
    while (async_fetcher->Poll(1000) != 0) {}
  }

  MockMessageHandler handler_;
  MockTimer timer_;
};

TEST_F(SyncFetcherAdapterTest, QuickOk) {
  DelayedFetcher async_fetcher(&timer_, &handler_, 0, true);
  SyncFetcherAdapter sync_fetcher(&timer_, 1000, &async_fetcher);
  TestSuccessfulFetch(&sync_fetcher);
}

TEST_F(SyncFetcherAdapterTest, SlowOk) {
  DelayedFetcher async_fetcher(&timer_, &handler_, 500, true);
  SyncFetcherAdapter sync_fetcher(&timer_, 1000, &async_fetcher);
  TestSuccessfulFetch(&sync_fetcher);
}

TEST_F(SyncFetcherAdapterTest, QuickFail) {
  DelayedFetcher async_fetcher(&timer_, &handler_, 0, false);
  SyncFetcherAdapter sync_fetcher(&timer_, 1000, &async_fetcher);
  TestFailedFetch(&sync_fetcher);
}

TEST_F(SyncFetcherAdapterTest, SlowFail) {
  DelayedFetcher async_fetcher(&timer_, &handler_, 500, false);
  SyncFetcherAdapter sync_fetcher(&timer_, 1000, &async_fetcher);
  TestFailedFetch(&sync_fetcher);
}

TEST_F(SyncFetcherAdapterTest, TimeoutOk) {
  DelayedFetcher async_fetcher(&timer_, &handler_, 5000, true);
  SyncFetcherAdapter sync_fetcher(&timer_, 1000, &async_fetcher);
  TestTimeoutFetch(&async_fetcher, &sync_fetcher);
}

TEST_F(SyncFetcherAdapterTest, TimeoutFail) {
  DelayedFetcher async_fetcher(&timer_, &handler_, 5000, false);
  SyncFetcherAdapter sync_fetcher(&timer_, 1000, &async_fetcher);
  TestTimeoutFetch(&async_fetcher, &sync_fetcher);
}

}  // namespace net_instaweb
