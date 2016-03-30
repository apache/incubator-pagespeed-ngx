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
// Author: jefftk@google.com (Jeff Kaufman)

#include "pagespeed/apache/apache_fetch.h"

#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/apache/apache_httpd_includes.h"
#include "pagespeed/apache/header_util.h"
#include "pagespeed/apache/mock_apache.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/delay_cache.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace {

const char kJsData[]        = "alert ( 'hello, world!' ) ";
const char kJsUrl[]         = "http://example.com/foo.js";
const char kExampleUrl[]    = "http://www.example.com";
const char kCacheFragment[] = "";
const char kDebugInfo[]     = "ignored";
const char kExpectedHeaders[] =
    "Content-Type: text/plain\n"
    "Set-Cookie: test=cookie\n"
    "Set-Cookie: tasty=cookie\n"
    "Set-Cookie2: obselete\n"
    "Date: Tue, 02 Feb 2010 18:51:26 GMT\n"
    "X-Content-Type-Options: nosniff\n"
    "Cache-Control: max-age=0, no-cache\n";

}  // namespace

namespace net_instaweb {

class ApacheFetchTest : public RewriteTestBase {
 public:
  ApacheFetchTest()
      : apache_writer_(
            new ApacheWriter(&request_, server_context_->thread_system())),
        request_headers_(new RequestHeaders()),
        request_ctx_(RequestContext::NewTestRequestContextWithTimer(
            server_context_->thread_system(), timer())) {
  }

  virtual void SetUp() {
    RewriteTestBase::SetUp();

    MockApache::Initialize();
    MockApache::PrepareRequest(&request_);
  }

  virtual void TearDown() {
    apache_fetch_.reset(NULL);
    RewriteTestBase::TearDown();
  }

  void InitFetchUnbuffered(StringPiece url, HttpStatus::Code code) {
    InitFetch(url, code, false);
  }

  void InitFetchBuffered(StringPiece url, HttpStatus::Code code) {
    InitFetch(url, code, true);
  }

  void InitFetch(StringPiece url, HttpStatus::Code code, bool buffered) {
    // Takes ownership of apache_writer and request_headers but nothing else.
    // Keeping a copy them violates ApacheFetch's mutex guarantees, and should
    // only be done by tests.
    apache_fetch_.reset(new ApacheFetch(
        url.as_string(), kDebugInfo, rewrite_driver_, apache_writer_,
        request_headers_, request_ctx_, options(), message_handler()));
    apache_fetch_->set_buffered(buffered);
    apache_fetch_->response_headers()->SetStatusAndReason(code);

    // HTTP 1.1 is most common now.
    apache_fetch_->response_headers()->set_major_version(1);
    apache_fetch_->response_headers()->set_minor_version(1);

    // Responses need a content type or we will 403.
    apache_fetch_->response_headers()->MergeContentType("text/plain");

    // Set cookies to make sure they get removed.
    apache_fetch_->response_headers()->Add("Set-Cookie", "test=cookie");
    apache_fetch_->response_headers()->Add("Set-Cookie", "tasty=cookie");
    apache_fetch_->response_headers()->Add("Set-Cookie2", "obselete");

    // None of these should have been passed through to the apache_writer.
    EXPECT_STREQ("", MockApache::ActionsSinceLastCall());
  }

  virtual ~ApacheFetchTest() {
    MockApache::CleanupRequest(&request_);
    MockApache::Terminate();
  }

  void WaitIproTest(bool buffered) {
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
                      &response_headers, kJsData, &message_handler_);

    GoogleUrl gurl(kJsUrl);
    InitFetch(kJsUrl, HttpStatus::kOK, buffered);
    rewrite_driver_->FetchInPlaceResource(gurl, false /* proxy_mode */,
                                          apache_fetch_.get());
    apache_fetch_->Wait();
    EXPECT_EQ(0, message_handler()->TotalMessages());

    EXPECT_EQ(StrCat("ap_set_content_length(26) "
                     "ap_set_content_type(application/javascript) "
                     "ap_remove_output_filter(MOD_EXPIRES) "
                     "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
                     "ap_set_content_type(application/javascript) "
                     "ap_rwrite(", kJsData, ")"),
              MockApache::ActionsSinceLastCall());
    apache_fetch_.reset(NULL);
  }

  void ReleaseKey(GoogleString key) {
    delay_cache()->ReleaseKey(key);
  }

 protected:
  void FetchDone() {
    apache_fetch_->Done(true);
  }

  // After a request is all set up, run through a successful completion and
  // verify that no issues were sent to the message handler.
  void WaitExpectSuccess() {
    rewrite_driver_->scheduler()->AddAlarmAtUs(
        timer()->NowUs() + 1,
        MakeFunction(this, &ApacheFetchTest::FetchDone));
    apache_fetch_->Wait();
    EXPECT_EQ(0, message_handler_.TotalMessages());
  }

  GoogleString SynthesizeWarning(StringPiece message) {
    return StrCat("W[Wed Jan 01 00:00:00 2014] [Warning] [00000] ", message);
  }

  request_rec request_;
  ApacheWriter* apache_writer_;
  RequestHeaders* request_headers_;
  RequestContextPtr request_ctx_;
  scoped_ptr<ApacheFetch> apache_fetch_;
};

TEST_F(ApacheFetchTest, WaitIproUnbuffered) {
  WaitIproTest(false);
}

TEST_F(ApacheFetchTest, WaitIproUnbufferedWithTest) {
  GoogleString key = http_cache()->CompositeKey(kJsUrl, kCacheFragment);
  delay_cache()->DelayKey(key);
  // This form looks clearer and more concise, and doesn't require a
  // helper method.  However it doesn't compile because of template
  // problems I didn't feel like debugging.
  //
  // Function* alarm = MakeFunction(delay_cache(), &DelayCache::ReleaseKey,key);
  ApacheFetchTest* test = this;
  Function* alarm = MakeFunction(test, &ApacheFetchTest::ReleaseKey, key);
  server_context_->scheduler()->AddAlarmAtUs(timer()->NowUs() + 100, alarm);
  WaitIproTest(false);
}

TEST_F(ApacheFetchTest, WaitIproBuffered) {
  WaitIproTest(true);
}

TEST_F(ApacheFetchTest, SuccessBuffered) {
  InitFetchBuffered(kExampleUrl, HttpStatus::kOK);
  EXPECT_TRUE(apache_fetch_->Write("hello ", &message_handler_));
  EXPECT_TRUE(apache_fetch_->Write("world", &message_handler_));
  EXPECT_TRUE(apache_fetch_->Flush(&message_handler_));
  EXPECT_TRUE(apache_fetch_->Write(".", &message_handler_));
  EXPECT_TRUE(apache_fetch_->Flush(&message_handler_));
  // All flushes are dropped, all writed are buffered.
  EXPECT_STREQ("", MockApache::ActionsSinceLastCall());

  WaitExpectSuccess();

  EXPECT_EQ(200, request_.status);
  EXPECT_EQ(
      "ap_set_content_type(text/plain) "
      "ap_remove_output_filter(MOD_EXPIRES) "
      "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
      "ap_set_content_type(text/plain) "
      "ap_rwrite(hello world.)",  // writes combined into one call.
      MockApache::ActionsSinceLastCall());

  // TODO(jefftk): Cookies are present here even though we asked ApacheWriter to
  // remove them because of an ApacheWriter bug.
  EXPECT_EQ(kExpectedHeaders, HeadersOutToString(&request_));
}

TEST_F(ApacheFetchTest, SuccessUnbuffered) {
  InitFetchUnbuffered(kExampleUrl, HttpStatus::kOK);
  EXPECT_TRUE(apache_fetch_->Write("hello ", &message_handler_));
  EXPECT_EQ(200, request_.status);
  EXPECT_STREQ(
      "ap_set_content_type(text/plain) "
      "ap_remove_output_filter(MOD_EXPIRES) "
      "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
      "ap_set_content_type(text/plain) "
      "ap_rwrite(hello )",
      MockApache::ActionsSinceLastCall());

  // TODO(jefftk): Cookies are present here even though we asked ApacheWriter to
  // remove them because of an ApacheWriter bug.
  EXPECT_EQ(kExpectedHeaders, HeadersOutToString(&request_));

  EXPECT_TRUE(apache_fetch_->Write("world", message_handler()));
  EXPECT_STREQ("ap_rwrite(world)", MockApache::ActionsSinceLastCall());
  EXPECT_TRUE(apache_fetch_->Flush(message_handler()));
  EXPECT_STREQ("ap_rflush()", MockApache::ActionsSinceLastCall());
  EXPECT_TRUE(apache_fetch_->Write(".", message_handler()));
  EXPECT_STREQ("ap_rwrite(.)", MockApache::ActionsSinceLastCall());
  EXPECT_TRUE(apache_fetch_->Flush(message_handler()));
  EXPECT_STREQ("ap_rflush()", MockApache::ActionsSinceLastCall());
  apache_fetch_->Done(true);
}

TEST_F(ApacheFetchTest, NotFound404Buffered) {
  InitFetchBuffered(kExampleUrl, HttpStatus::kNotFound);
  EXPECT_TRUE(apache_fetch_->Write("Couldn't find it.", &message_handler_));
  // Writes are buffered.
  EXPECT_STREQ("", MockApache::ActionsSinceLastCall());

  WaitExpectSuccess();

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

TEST_F(ApacheFetchTest, NotFound404Unbuffered) {
  InitFetchUnbuffered(kExampleUrl, HttpStatus::kNotFound);
  EXPECT_TRUE(apache_fetch_->Write("Couldn't find it.", message_handler()));
  EXPECT_EQ(404, request_.status);
  EXPECT_EQ(kExpectedHeaders, HeadersOutToString(&request_));
  EXPECT_EQ(
      "ap_set_content_type(text/plain) "
      "ap_remove_output_filter(MOD_EXPIRES) "
      "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
      "ap_set_content_type(text/plain) "
      "ap_rwrite(Couldn't find it.)",
      MockApache::ActionsSinceLastCall());
  apache_fetch_->Done(true);
}

TEST_F(ApacheFetchTest, NoContentType200Buffered) {
  InitFetchBuffered(kExampleUrl, HttpStatus::kOK);
  EXPECT_TRUE(apache_fetch_->response_headers()->RemoveAll(
      HttpAttributes::kContentType));
  EXPECT_TRUE(apache_fetch_->Write("Example response.", &message_handler_));
  // Writes are buffered.
  EXPECT_STREQ("", MockApache::ActionsSinceLastCall());

  WaitExpectSuccess();

  EXPECT_EQ(403, request_.status);
  EXPECT_STREQ("Set-Cookie: test=cookie\n"
               "Set-Cookie: tasty=cookie\n"
               "Set-Cookie2: obselete\n"
               "Content-Type: text/html\n"
               "Date: Tue, 02 Feb 2010 18:51:26 GMT\n"
               "X-Content-Type-Options: nosniff\n"
               "Cache-Control: max-age=0, no-cache\n",
               HeadersOutToString(&request_));
  EXPECT_EQ(
      "ap_set_content_type(text/html) "
      "ap_remove_output_filter(MOD_EXPIRES) "
      "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
      "ap_set_content_type(text/html) "
      "ap_rwrite(Missing Content-Type required for proxied resource)",
      MockApache::ActionsSinceLastCall());
}

TEST_F(ApacheFetchTest, NoContentType200Unbuffered) {
  InitFetchUnbuffered(kExampleUrl, HttpStatus::kOK);
  EXPECT_TRUE(apache_fetch_->response_headers()->RemoveAll(
      HttpAttributes::kContentType));
  EXPECT_TRUE(apache_fetch_->Write("Example response.", message_handler()));
  EXPECT_EQ(403, request_.status);
  EXPECT_STREQ("Set-Cookie: test=cookie\n"
               "Set-Cookie: tasty=cookie\n"
               "Set-Cookie2: obselete\n"
               "Content-Type: text/html\n"
               "Date: Tue, 02 Feb 2010 18:51:26 GMT\n"
               "X-Content-Type-Options: nosniff\n"
               "Cache-Control: max-age=0, no-cache\n",
               HeadersOutToString(&request_));
  EXPECT_EQ(
      "ap_set_content_type(text/html) "
      "ap_remove_output_filter(MOD_EXPIRES) "
      "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
      "ap_set_content_type(text/html) "
      "ap_rwrite(Missing Content-Type required for proxied resource)",
      MockApache::ActionsSinceLastCall());
  apache_fetch_->Done(true);
}

TEST_F(ApacheFetchTest, NoContentType301Buffered) {
  InitFetchBuffered(kExampleUrl, HttpStatus::kMovedPermanently);
  EXPECT_TRUE(apache_fetch_->response_headers()->RemoveAll("Content-Type"));
  apache_fetch_->response_headers()->Add("Location", "elsewhere");

  EXPECT_TRUE(apache_fetch_->Write("moved elsewhere", &message_handler_));
  // Writes are buffered.
  EXPECT_STREQ("", MockApache::ActionsSinceLastCall());

  WaitExpectSuccess();

  EXPECT_EQ(403, request_.status);
  EXPECT_STREQ("Set-Cookie: test=cookie\n"
               "Set-Cookie: tasty=cookie\n"
               "Set-Cookie2: obselete\n"
               "Location: elsewhere\n"
               "Content-Type: text/html\n"
               "Date: Tue, 02 Feb 2010 18:51:26 GMT\n"
               "X-Content-Type-Options: nosniff\n"
               "Cache-Control: max-age=0, no-cache\n",
               HeadersOutToString(&request_));
  EXPECT_EQ(
      "ap_set_content_type(text/html) "
      "ap_remove_output_filter(MOD_EXPIRES) "
      "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
      "ap_set_content_type(text/html) "
      "ap_rwrite(Missing Content-Type required for proxied resource)",
      MockApache::ActionsSinceLastCall());
}

TEST_F(ApacheFetchTest, NoContentType301Unbuffered) {
  InitFetchUnbuffered(kExampleUrl, HttpStatus::kMovedPermanently);
  EXPECT_TRUE(apache_fetch_->response_headers()->RemoveAll("Content-Type"));
  apache_fetch_->response_headers()->Add("Location", "elsewhere");
  EXPECT_TRUE(apache_fetch_->Write("moved elsewhere", message_handler()));
  EXPECT_EQ(403, request_.status);
  EXPECT_STREQ("Set-Cookie: test=cookie\n"
               "Set-Cookie: tasty=cookie\n"
               "Set-Cookie2: obselete\n"
               "Location: elsewhere\n"
               "Content-Type: text/html\n"
               "Date: Tue, 02 Feb 2010 18:51:26 GMT\n"
               "X-Content-Type-Options: nosniff\n"
               "Cache-Control: max-age=0, no-cache\n",
               HeadersOutToString(&request_));
  EXPECT_EQ(
      "ap_set_content_type(text/html) "
      "ap_remove_output_filter(MOD_EXPIRES) "
      "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
      "ap_set_content_type(text/html) "
      "ap_rwrite(Missing Content-Type required for proxied resource)",
      MockApache::ActionsSinceLastCall());
  apache_fetch_->Done(true);
}

TEST_F(ApacheFetchTest, NoContentType304Buffered) {
  InitFetchBuffered(kExampleUrl, HttpStatus::kNotModified);
  EXPECT_TRUE(apache_fetch_->response_headers()->RemoveAll("Content-Type"));

  EXPECT_TRUE(apache_fetch_->Write("not modified", &message_handler_));
  // Writes are buffered.
  EXPECT_STREQ("", MockApache::ActionsSinceLastCall());

  WaitExpectSuccess();

  EXPECT_EQ(304, request_.status);
  EXPECT_STREQ("Set-Cookie: test=cookie\n"
               "Set-Cookie: tasty=cookie\n"
               "Set-Cookie2: obselete\n"
               "Date: Tue, 02 Feb 2010 18:51:26 GMT\n"
               "X-Content-Type-Options: nosniff\n"
               "Cache-Control: max-age=0, no-cache\n",
               HeadersOutToString(&request_));
  EXPECT_EQ(
      "ap_remove_output_filter(MOD_EXPIRES) "
      "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
      "ap_rwrite(not modified)",
      MockApache::ActionsSinceLastCall());
}

TEST_F(ApacheFetchTest, NoContentType304Unbuffered) {
  InitFetchUnbuffered(kExampleUrl, HttpStatus::kNotModified);
  apache_fetch_->response_headers()->set_status_code(304);
  EXPECT_TRUE(apache_fetch_->response_headers()->RemoveAll("Content-Type"));
  EXPECT_TRUE(apache_fetch_->Write("not modified", message_handler()));
  EXPECT_EQ(304, request_.status);
  EXPECT_STREQ("Set-Cookie: test=cookie\n"
               "Set-Cookie: tasty=cookie\n"
               "Set-Cookie2: obselete\n"
               "Date: Tue, 02 Feb 2010 18:51:26 GMT\n"
               "X-Content-Type-Options: nosniff\n"
               "Cache-Control: max-age=0, no-cache\n",
               HeadersOutToString(&request_));
  EXPECT_EQ(
      "ap_remove_output_filter(MOD_EXPIRES) "
      "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
      "ap_rwrite(not modified)",
      MockApache::ActionsSinceLastCall());
  apache_fetch_->Done(true);
}

TEST_F(ApacheFetchTest, NoContentType204Buffered) {
  InitFetchBuffered(kExampleUrl, HttpStatus::kNoContent);
  EXPECT_TRUE(apache_fetch_->response_headers()->RemoveAll("Content-Type"));
  apache_fetch_->HeadersComplete();
  // Headers are buffered.
  EXPECT_STREQ("", MockApache::ActionsSinceLastCall());

  WaitExpectSuccess();
  EXPECT_EQ(204, request_.status);
  EXPECT_STREQ("Set-Cookie: test=cookie\n"
               "Set-Cookie: tasty=cookie\n"
               "Set-Cookie2: obselete\n"
               "Date: Tue, 02 Feb 2010 18:51:26 GMT\n"
               "X-Content-Type-Options: nosniff\n"
               "Cache-Control: max-age=0, no-cache\n",
               HeadersOutToString(&request_));
  EXPECT_STREQ("ap_remove_output_filter(MOD_EXPIRES) "
               "ap_remove_output_filter(FIXUP_HEADERS_OUT)",
               MockApache::ActionsSinceLastCall());
}

TEST_F(ApacheFetchTest, NoContentType204Unbuffered) {
  InitFetchUnbuffered(kExampleUrl, HttpStatus::kNoContent);
  apache_fetch_->response_headers()->set_status_code(204);
  EXPECT_TRUE(apache_fetch_->response_headers()->RemoveAll("Content-Type"));
  apache_fetch_->HeadersComplete();
  EXPECT_EQ(204, request_.status);
  EXPECT_STREQ("Set-Cookie: test=cookie\n"
               "Set-Cookie: tasty=cookie\n"
               "Set-Cookie2: obselete\n"
               "Date: Tue, 02 Feb 2010 18:51:26 GMT\n"
               "X-Content-Type-Options: nosniff\n"
               "Cache-Control: max-age=0, no-cache\n",
               HeadersOutToString(&request_));
  EXPECT_STREQ("ap_remove_output_filter(MOD_EXPIRES) "
               "ap_remove_output_filter(FIXUP_HEADERS_OUT)",
               MockApache::ActionsSinceLastCall());
  apache_fetch_->Done(true);
}

}  // namespace net_instaweb
