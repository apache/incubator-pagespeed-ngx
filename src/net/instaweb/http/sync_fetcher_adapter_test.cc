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

// Test the blocking w/timeout callback helper.

#include "net/instaweb/http/public/sync_fetcher_adapter_callback.h"

#include <algorithm>

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/platform.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/thread.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/writer.h"
#include "net/instaweb/util/worker_test_base.h"

namespace net_instaweb {

class MessageHandler;

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

// An async fetcher that writes out a response at given number of milliseconds
// elapsed, or when asked to immediately (if delay_ms <= 0). Note that it's only
// capable of one fetch, and destroys itself after that completes. If a
// syncpoint is set using set_sync, it will be notified immediately before that.
class DelayedFetcher : public UrlAsyncFetcher {
 public:
  // Note: If sim_delay <= 0, will report immediately at Fetch.
  DelayedFetcher(ThreadSystem* thread_system,
                 Timer* timer, MessageHandler* handler,
                 int64 delay_ms, bool sim_success)
      : thread_system_(thread_system),
        timer_(timer), handler_(handler), delay_ms_(delay_ms),
        sim_success_(sim_success), fetch_pending_(false), fetch_(NULL),
        sync_(NULL) {
  }

  virtual void Fetch(const GoogleString& url, MessageHandler* handler,
                     AsyncFetch* fetch) {
    CHECK(!fetch_pending_);
    fetch_ = fetch;
    fetch_pending_ = true;

    if (delay_ms_ <= 0) {
      ReportResult();
    } else {
      InvokeCallbackThread* thread =
          new InvokeCallbackThread(thread_system_, this);
      EXPECT_TRUE(thread->Start());
    }
  }

  void set_sync(WorkerTestBase::SyncPoint* sync) { sync_ = sync; }

 private:
  ~DelayedFetcher() {}

  class InvokeCallbackThread : public ThreadSystem::Thread {
   public:
    InvokeCallbackThread(ThreadSystem* thread_system,
                         DelayedFetcher* parent)
        : Thread(thread_system, "delayed_fetch", ThreadSystem::kDetached),
          parent_(parent) {
    }

    virtual void Run() {
      parent_->timer_->SleepMs(parent_->delay_ms_);
      parent_->ReportResult();
      delete this;
    }

   private:
    DelayedFetcher* parent_;
  };

  void ReportResult() {
    ResponseHeaders headers;
    if (sim_success_) {
      fetch_->response_headers()->CopyFrom(headers);
      fetch_->response_headers()->Add(kHeader, kText);
      fetch_->response_headers()->set_status_code(HttpStatus::kOK);
      fetch_->HeadersComplete();
      fetch_->Write(kText, handler_);
    }
    fetch_->Done(sim_success_);
    if (sync_ != NULL) {
      sync_->Notify();
    }
    delete this;
  }

  ThreadSystem* thread_system_;
  Timer* timer_;
  MessageHandler* handler_;

  // Simulation settings:
  int64 delay_ms_;  // how long till we report the result
  bool sim_success_;  // whether to report success or failure

  // Fetch session:
  bool fetch_pending_;
  AsyncFetch* fetch_;

  // If non-NULL, will be used to wake up an owner when done.
  WorkerTestBase::SyncPoint* sync_;
};

}  // namespace

class SyncFetcherAdapterTest : public testing::Test {
 public:
  SyncFetcherAdapterTest()
      : timer_(Platform::CreateTimer()),
        thread_system_(Platform::CreateThreadSystem()),
        handler_(thread_system_->NewMutex()) {
  }

 protected:
  void DoFetch(UrlAsyncFetcher* fetcher, SyncFetcherAdapterCallback* callback) {
    fetcher->Fetch("http://www.example.com/", &handler_, callback);
  }

  void Wait(SyncFetcherAdapterCallback* callback, int64 timeout_ms) {
    bool locked_ok = callback->LockIfNotReleased();
    ASSERT_TRUE(locked_ok);
    // The thread safety annotation analysis doesn't recognize the ASSERT_TRUE
    // as guaranteeing that the lock is held, so provide a dummy check here to
    // provide that guarantee for the compiler.
    if (!locked_ok) {
      return;
    }
    // Should always succeed since we don't call ->Release
    // on callback until end of this method.
    int64 now_ms = timer_->NowMs();
    for (int64 end_ms = now_ms + timeout_ms;
         !callback->IsDoneLockHeld() && (now_ms < end_ms);
         now_ms = timer_->NowMs()) {
      int64 remaining_ms = std::max(static_cast<int64>(0), end_ms - now_ms);
      callback->TimedWait(remaining_ms);
    }
    callback->Unlock();
  }

  void TestSuccessfulFetch(UrlAsyncFetcher* async_fetcher) {
    GoogleString out_str;
    StringWriter out_writer(&out_str);
    RequestContextPtr ctx(
        RequestContext::NewTestRequestContext(thread_system_.get()));

    SyncFetcherAdapterCallback* callback =
        new SyncFetcherAdapterCallback(thread_system_.get(), &out_writer, ctx);
    DoFetch(async_fetcher, callback);
    Wait(callback, 1000);

    EXPECT_TRUE(callback->IsDone());
    EXPECT_TRUE(callback->success());
    EXPECT_EQ(kText, out_str);

    ConstStringStarVector values;
    EXPECT_TRUE(callback->response_headers()->Lookup(kHeader, &values));
    ASSERT_EQ(1, values.size());
    EXPECT_EQ(GoogleString(kText), *(values[0]));

    callback->Release();
  }

  void TestFailedFetch(UrlAsyncFetcher* async_fetcher, int64 timeout_ms) {
    TrapWriter writer;
    RequestContextPtr ctx(
        RequestContext::NewTestRequestContext(thread_system_.get()));
    SyncFetcherAdapterCallback* callback =
        new SyncFetcherAdapterCallback(thread_system_.get(), &writer, ctx);

    async_fetcher->Fetch("http://www.example.com", &handler_, callback);
    Wait(callback, timeout_ms);
    EXPECT_FALSE(callback->success());
    callback->Release();
  }

  void TestTimeoutFetch(DelayedFetcher* async_fetcher) {
    WorkerTestBase::SyncPoint sync(thread_system_.get());

    // We use a sync point to wait for the fetch thread to exit since the
    // fixture owns the timer and we need that to be alive from the callbacks.
    async_fetcher->set_sync(&sync);

    // First let the sync fetcher timeout, and return failure.
    TestFailedFetch(async_fetcher, 1 /* one millisecond timeout here */);
    sync.Wait();
  }

  scoped_ptr<Timer> timer_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockMessageHandler handler_;
};

TEST_F(SyncFetcherAdapterTest, QuickOk) {
  DelayedFetcher* async_fetcher = new DelayedFetcher(
      thread_system_.get(), timer_.get(), &handler_, 0, true);
  TestSuccessfulFetch(async_fetcher);
}

TEST_F(SyncFetcherAdapterTest, SlowOk) {
  DelayedFetcher* async_fetcher = new DelayedFetcher(
      thread_system_.get(), timer_.get(), &handler_, 5, true);
  TestSuccessfulFetch(async_fetcher);
}

TEST_F(SyncFetcherAdapterTest, QuickFail) {
  DelayedFetcher* async_fetcher = new DelayedFetcher(
      thread_system_.get(), timer_.get(), &handler_, 0, false);
  TestFailedFetch(async_fetcher, 1000);
}

TEST_F(SyncFetcherAdapterTest, SlowFail) {
  DelayedFetcher* async_fetcher = new DelayedFetcher(
      thread_system_.get(), timer_.get(), &handler_, 5, false);
  TestFailedFetch(async_fetcher, 1000);
}

TEST_F(SyncFetcherAdapterTest, TimeoutOk) {
  DelayedFetcher* async_fetcher = new DelayedFetcher(
      thread_system_.get(), timer_.get(), &handler_, 25, true);
  TestTimeoutFetch(async_fetcher);
}

TEST_F(SyncFetcherAdapterTest, TimeoutFail) {
  DelayedFetcher* async_fetcher = new DelayedFetcher(
      thread_system_.get(), timer_.get(), &handler_, 25, false);
  TestTimeoutFetch(async_fetcher);
}

}  // namespace net_instaweb
