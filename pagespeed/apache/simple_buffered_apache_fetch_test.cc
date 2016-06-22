// Copyright 2015 Google Inc.
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
// Author: jefftk@google.com (Jeff Kaufman),
//         morlovich@google.com (Maksim Orlovich)

#include "pagespeed/apache/simple_buffered_apache_fetch.h"

#include <functional>
#include <memory>

#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/apache/apache_httpd_includes.h"
#include "pagespeed/apache/header_util.h"
#include "pagespeed/apache/mock_apache.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/thread.h"
#include "pagespeed/kernel/cache/delay_cache.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace {

const char kJsData[]        = "alert ( 'hello, world!' ) ";
const char kJsUrl[]         = "http://example.com/foo.js";
const char kCacheFragment[] = "";
const char kExpectedHeaders[] =
    "Content-Type: text/plain\n"
    "Set-Cookie: test=cookie\n";

}  // namespace

namespace net_instaweb {

class SimpleBufferedApacheFetchTest : public RewriteTestBase {
 public:
  SimpleBufferedApacheFetchTest()
      : request_headers_(new RequestHeaders()),
        request_ctx_(RequestContext::NewTestRequestContextWithTimer(
            server_context_->thread_system(), timer())) {
  }

  ~SimpleBufferedApacheFetchTest() override {
    MockApache::CleanupRequest(&request_);
    MockApache::Terminate();
  }

  void SetUp() override {
    RewriteTestBase::SetUp();
    MockApache::Initialize();
    MockApache::PrepareRequest(&request_);
  }

  void TearDown() override {
    fetch_.reset();
    RewriteTestBase::TearDown();
  }

  void InitFetch(HttpStatus::Code code) {
    // Takes ownership of apache_writer and request_headers.
    fetch_.reset(new SimpleBufferedApacheFetch(
        request_ctx_, request_headers_, server_context()->thread_system(),
        &request_, message_handler()));
    rewrite_driver_->SetRequestHeaders(*fetch_->request_headers());
    fetch_->response_headers()->SetStatusAndReason(code);

    // HTTP 1.1 is most common now.
    fetch_->response_headers()->set_major_version(1);
    fetch_->response_headers()->set_minor_version(1);

    // Give it a content-type, and a cookie --- so we can make sure it
    // forwards them to Apache.
    fetch_->response_headers()->MergeContentType("text/plain");
    fetch_->response_headers()->Add("Set-Cookie", "test=cookie");

    // Nothing should have been passed through to the apache_writer yet
    EXPECT_STREQ("", MockApache::ActionsSinceLastCall());
  }


  void WaitIproTest() {
    // Run through a successful completion and verify that no issues were
    // sent to the message handler.
    AddFilter(RewriteOptions::kRewriteJavascriptExternal);
    ResponseHeaders response_headers;
    DefaultResponseHeaders(kContentTypeJavascript, 600, &response_headers);
    RequestHeaders::Properties req_properties;
    req_properties.has_cookie = false;
    req_properties.has_cookie2 = false;
    http_cache()->Put(kJsUrl, kCacheFragment, req_properties,
                      ResponseHeaders::kIgnoreVaryOnResources,
                      &response_headers, kJsData, message_handler());

    GoogleUrl gurl(kJsUrl);
    InitFetch(HttpStatus::kOK);
    rewrite_driver_->FetchInPlaceResource(gurl, false /* proxy_mode */,
                                          fetch_.get());
    fetch_->Wait();
    EXPECT_EQ(0, message_handler()->TotalMessages());

    EXPECT_EQ(StrCat("ap_set_content_length(26) "
                     "ap_set_content_type(application/javascript) "
                     "ap_remove_output_filter(MOD_EXPIRES) "
                     "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
                     "ap_set_content_type(application/javascript) "
                     "ap_rwrite(", kJsData, ")"),
              MockApache::ActionsSinceLastCall());
    fetch_.reset();
  }

  class ThreadRunner : public ThreadSystem::Thread {
   public:
    ThreadRunner(RewriteTestBase* fixture, std::function<void()> fn)
        : Thread(fixture->server_context()->thread_system(), "run_function",
                 ThreadSystem::kJoinable),
          fn_(fn) {
      Start();
    }

    ~ThreadRunner() override {
      Join();
    }

    void Run() override {
      fn_();
    }

   private:
    std::function<void()> fn_;
  };

  request_rec request_;
  RequestHeaders* request_headers_;
  RequestContextPtr request_ctx_;
  std::unique_ptr<SimpleBufferedApacheFetch> fetch_;
};

TEST_F(SimpleBufferedApacheFetchTest, WaitIpro) {
  WaitIproTest();
}

TEST_F(SimpleBufferedApacheFetchTest, WaitIpro2) {
  GoogleString key = http_cache()->CompositeKey(kJsUrl, kCacheFragment);
  delay_cache()->DelayKey(key);
  ThreadRunner t(this, [this, key] {
    delay_cache()->ReleaseKey(key);
  });
  WaitIproTest();
}

TEST_F(SimpleBufferedApacheFetchTest, Success) {
  InitFetch(HttpStatus::kOK);

  ThreadRunner t(this, [this] {
    EXPECT_TRUE(fetch_->Write("hello ", message_handler()));
    EXPECT_TRUE(fetch_->Write("world", message_handler()));
    EXPECT_TRUE(fetch_->Flush(message_handler()));
    EXPECT_TRUE(fetch_->Write(".", message_handler()));
    EXPECT_TRUE(fetch_->Flush(message_handler()));
    fetch_->Done(true);
  });

  fetch_->Wait();
  EXPECT_EQ(200, request_.status);

  // Two outputs are possible: we can combine writes, but not accross flush...
  const char kExpectedSequence1[] =
      "ap_set_content_type(text/plain) "
      "ap_remove_output_filter(MOD_EXPIRES) "
      "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
      "ap_set_content_type(text/plain) "
      "ap_rwrite(hello world) "
      "ap_rflush() "
      "ap_rwrite(.) "
      "ap_rflush()";

  // ... or we can end up not combining if this thread pulls the first
  // write before the worker produces a 2nd writes.
  const char kExpectedSequence2[] =
      "ap_set_content_type(text/plain) "
      "ap_remove_output_filter(MOD_EXPIRES) "
      "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
      "ap_set_content_type(text/plain) "
      "ap_rwrite(hello ) "
      "ap_rwrite(world) "
      "ap_rflush() "
      "ap_rwrite(.) "
      "ap_rflush()";

  GoogleString actual = MockApache::ActionsSinceLastCall();

  EXPECT_TRUE(actual == kExpectedSequence1 || actual == kExpectedSequence2);
  EXPECT_EQ(kExpectedHeaders, HeadersOutToString(&request_));
}

TEST_F(SimpleBufferedApacheFetchTest, NotFound404) {
  InitFetch(HttpStatus::kNotFound);

  ThreadRunner t(this, [this] {
    EXPECT_TRUE(fetch_->Write("Couldn't find it.", message_handler()));
    fetch_->Done(true);
  });

  fetch_->Wait();
  EXPECT_EQ(404, request_.status);
  EXPECT_EQ(kExpectedHeaders, HeadersOutToString(&request_));
  EXPECT_EQ(
      "ap_set_content_type(text/plain) "
      "ap_remove_output_filter(MOD_EXPIRES) "
      "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
      "ap_set_content_type(text/plain) "
      "ap_rwrite(Couldn't find it.)",
      MockApache::ActionsSinceLastCall());
}

}  // namespace net_instaweb
