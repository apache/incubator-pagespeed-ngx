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

#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

namespace {

const char kUrl[] = "http://www.example.com/";
const char kUrl2[] = "http://www.example.com/2";
const char kBody[] = "Contents.";
const char kBody2[] = "Contents.";

class WaitUrlAsyncFetcherTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    thread_system_.reset(Platform::CreateThreadSystem());
    wait_fetcher_.reset(new WaitUrlAsyncFetcher(
        &base_fetcher_, thread_system_->NewMutex()));

    ResponseHeaders header;
    header.set_first_line(1, 1, 200, "OK");
    base_fetcher_.SetResponse(kUrl, header, kBody);
    base_fetcher_.SetResponse(kUrl2, header, kBody2);
  }

  WaitUrlAsyncFetcher* wait_fetcher() { return wait_fetcher_.get(); }

  scoped_ptr<ThreadSystem> thread_system_;

 private:
  MockUrlFetcher base_fetcher_;
  scoped_ptr<WaitUrlAsyncFetcher> wait_fetcher_;
};

TEST_F(WaitUrlAsyncFetcherTest, FetcherWaits) {
  GoogleMessageHandler handler;
  ExpectStringAsyncFetch callback(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));

  wait_fetcher()->Fetch(kUrl, &handler, &callback);

  // Nothing gets set ...
  EXPECT_FALSE(callback.done());
  EXPECT_EQ("", callback.buffer());

  // ... until we CallCallbacks.
  wait_fetcher()->CallCallbacks();
  EXPECT_TRUE(callback.done());
  EXPECT_EQ(kBody, callback.buffer());
}

TEST_F(WaitUrlAsyncFetcherTest, PassThrough) {
  GoogleMessageHandler handler;
  ExpectStringAsyncFetch callback(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));

  wait_fetcher()->Fetch(kUrl, &handler, &callback);

  // Nothing gets set ...
  EXPECT_FALSE(callback.done());
  EXPECT_EQ("", callback.buffer());

  // Now switch to pass-through mode.  This causes the callback to get called.
  bool prev_mode = wait_fetcher()->SetPassThroughMode(true);
  EXPECT_FALSE(prev_mode);
  EXPECT_TRUE(callback.done());
  EXPECT_EQ(kBody, callback.buffer());

  // Now fetches happen instantly.
  ExpectStringAsyncFetch callback2(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));
  wait_fetcher()->Fetch(kUrl, &handler, &callback2);
  EXPECT_TRUE(callback2.done());
  EXPECT_EQ(kBody, callback2.buffer());
}

TEST_F(WaitUrlAsyncFetcherTest, Exclusion) {
  GoogleMessageHandler handler;
  ExpectStringAsyncFetch callback1(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));
  ExpectStringAsyncFetch callback2(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));

  wait_fetcher()->DoNotDelay(kUrl2);

  wait_fetcher()->Fetch(kUrl, &handler, &callback1);
  wait_fetcher()->Fetch(kUrl2, &handler, &callback2);

  // kUrl is delayed.
  EXPECT_FALSE(callback1.done());
  EXPECT_EQ("", callback1.buffer());

  // kUrl2 isn't.
  EXPECT_TRUE(callback2.done());
  EXPECT_EQ(kBody2, callback2.buffer());

  // Now unblock kUrl
  wait_fetcher()->CallCallbacks();
  EXPECT_TRUE(callback1.done());
  EXPECT_EQ(kBody, callback1.buffer());
}

}  // namespace

}  // namespace net_instaweb
