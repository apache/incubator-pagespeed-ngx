/*
 * Copyright 2010 Google Inc.
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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/http/public/wait_url_async_fetcher.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

namespace {

const char kUrl[] = "http://www.example.com/";
const char kBody[] = "Contents.";

class WaitUrlAsyncFetcherTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    thread_system_.reset(ThreadSystem::CreateThreadSystem());
    wait_fetcher_.reset(new WaitUrlAsyncFetcher(
        &base_fetcher_, thread_system_->NewMutex()));

    ResponseHeaders header;
    header.set_first_line(1, 1, 200, "OK");
    base_fetcher_.SetResponse(kUrl, header, kBody);
  }

  WaitUrlAsyncFetcher* wait_fetcher() { return wait_fetcher_.get(); }

 private:
  MockUrlFetcher base_fetcher_;
  scoped_ptr<ThreadSystem> thread_system_;
  scoped_ptr<WaitUrlAsyncFetcher> wait_fetcher_;
};

TEST_F(WaitUrlAsyncFetcherTest, FetcherWaits) {
  GoogleMessageHandler handler;
  ExpectStringAsyncFetch callback(true);

  EXPECT_FALSE(wait_fetcher()->Fetch(kUrl, &handler, &callback));

  // Nothing gets set ...
  EXPECT_EQ(false, callback.done());
  EXPECT_EQ("", callback.buffer());

  // ... until we CallCallbacks.
  wait_fetcher()->CallCallbacks();
  EXPECT_EQ(true, callback.done());
  EXPECT_EQ(kBody, callback.buffer());
}

TEST_F(WaitUrlAsyncFetcherTest, PassThrough) {
  GoogleMessageHandler handler;
  ExpectStringAsyncFetch callback(true);

  EXPECT_FALSE(wait_fetcher()->Fetch(kUrl, &handler, &callback));

  // Nothing gets set ...
  EXPECT_EQ(false, callback.done());
  EXPECT_EQ("", callback.buffer());

  // Now switch to pass-through mode.  This causes the callback to get called.
  bool prev_mode = wait_fetcher()->SetPassThroughMode(true);
  EXPECT_FALSE(prev_mode);
  EXPECT_EQ(true, callback.done());
  EXPECT_EQ(kBody, callback.buffer());

  // Now fetches happen instantly.
  ExpectStringAsyncFetch callback2(true);
  EXPECT_TRUE(wait_fetcher()->Fetch(kUrl, &handler, &callback2));
  EXPECT_EQ(true, callback2.done());
  EXPECT_EQ(kBody, callback2.buffer());
}

}  // namespace

}  // namespace net_instaweb
