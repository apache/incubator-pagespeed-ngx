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

#include "pagespeed/apache/header_util.h"
#include "pagespeed/apache/mock_apache.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/null_thread_system.h"
#include "pagespeed/kernel/base/string_writer.h"

#include "httpd.h"  // NOLINT

namespace net_instaweb {

class TimedWaitCallbackCallDone : public NullCondvar::TimedWaitCallback {
 public:
  explicit TimedWaitCallbackCallDone(ApacheFetch* apache_fetch)
      : apache_fetch_(apache_fetch) {}
  virtual void Call() { apache_fetch_->Done(true /* success */); }

 private:
  ApacheFetch* apache_fetch_;
};

// The standard way to use an ApacheFetch is with two threads:
//   apache thread:
//    - set things up
//    - put a rewrite task on the rewrite thread
//    - wait (timed)
//      - success: done, can use the fetch
//      - timeout: abandon fetch to complete in the background, can't use it
//   rewrite thread:
//    - call Write(), Flush(), etc
//      - send things through to Apache if not abandoned
//    - call Done()
//      - if abandoned we delete ourself
//      - otherwise signal the apache thread to stop waiting
//
// To test this flow here we run both sides in the same thread.  Success and
// timeout are mocked differently:
//
// success:
//  - Use a NullCondvar so TimedWait will immediately invoke Done() on the
//    ApacheFetch.  Now Wait() returns via the success path.
// timeout:
//  - Set the timeout to -1 and make TimedWait return immediately.  Now Wait()
//    returns via the abandonment path.
//
class ApacheFetchTest : public testing::Test {
 public:
  ApacheFetchTest()
      : mapped_url_("http://www.example.com"),
        debug_info_("ignored"),
        thread_system_(),
        timer_(new NullMutex(), 0),
        apache_writer_(new ApacheWriter(&request_, &thread_system_)),
        request_headers_(new RequestHeaders()),
        request_ctx_(RequestContext::NewTestRequestContextWithTimer(
            &thread_system_, &timer_)),
        message_handler_(new NullMutex()) {
    MockApache::Initialize();
    MockApache::PrepareRequest(&request_);
    RewriteOptions::Initialize();
    rewrite_options_.reset(new RewriteOptions(&thread_system_));

    // Takes ownership of apache_writer and request_headers but nothing else.
    // Keeping a copy them violates ApacheFetch's mutex guarantees, and should
    // only be done by tests.
    apache_fetch_.reset(new ApacheFetch(
        mapped_url_, debug_info_, &thread_system_, &timer_,
        apache_writer_, request_headers_, request_ctx_,
        rewrite_options_.get(), &message_handler_));

    condvar_ = dynamic_cast<NullCondvar*>(apache_fetch_->CondvarForTesting());
    CHECK(condvar_ != NULL);

    // These can be overriden later in tests that want to test something
    // unusual.
    apache_fetch_->response_headers()->set_status_code(200 /* OK */);

    // HTTP 1.1 is most common now.
    apache_fetch_->response_headers()->set_major_version(1);
    apache_fetch_->response_headers()->set_minor_version(1);

    // Responses need a content type or we will 403.
    apache_fetch_->response_headers()->MergeContentType("text/plain");

    // Set cookies to make sure they get removed.
    apache_fetch_->response_headers()->Add("Set-Cookie", "test=cookie");
    apache_fetch_->response_headers()->Add("Set-Cookie", "tasty=cookie");
    apache_fetch_->response_headers()->Add("Set-Cookie2", "obselete");

    apache_fetch_->set_max_wait_ms(-1);  // Never actually wait in tests.

    // None of these should have been passed through to the apache_writer.
    EXPECT_EQ("", MockApache::ActionsSinceLastCall());
  }

  virtual ~ApacheFetchTest() {
    RewriteOptions::Terminate();
    MockApache::CleanupRequest(&request_);
    MockApache::Terminate();
  }

 protected:
  // After a request is all set up, run through a successful completion and
  // verify that no issues were sent to the message handler.
  void WaitExpectSuccess() {
    TimedWaitCallbackCallDone callback(apache_fetch_.get());
    condvar_->set_timed_wait_callback(&callback);

    EXPECT_EQ(true, apache_fetch_->Wait(NULL /* rewrite_driver */));

    EXPECT_EQ(0, message_handler_.TotalMessages());

    EXPECT_EQ("TimedWait(5000) Signal()", condvar_->ActionsSinceLastCall());
  }

  GoogleString SynthesizeWarning(StringPiece message) {
    return StrCat("W[Wed Jan 01 00:00:00 2014] [Warning] [00000] ", message,
                  "\n");
  }

  // After a request is set up and Wait returned that the fetch had been
  // abandoned, finish the fetch and verify that we get the error messages we
  // expected.
  void PostAbandonmentHelper() {
    // Should normally null apache_fetch_ here because Wait failed but we're
    // mocking multiple threads here and still have to call Done() on it.

    EXPECT_FALSE(apache_fetch_->Write("abandoned", &message_handler_));
    // Write dropped.
    EXPECT_EQ("", MockApache::ActionsSinceLastCall());

    EXPECT_EQ("TimedWait(5000)",  // Note no Signal().
              condvar_->ActionsSinceLastCall());

    // Simulate the fetch finishing after being abandoned.
    apache_fetch_->Done(true /* success */);
    apache_fetch_.release();  // An abandoned fetch deletes itself on Done().

    GoogleString message_output;
    StringWriter writer(&message_output);
    message_handler_.Dump(&writer);

    GoogleString expected = StrCat(
        SynthesizeWarning(
            "Abandoned URL http://www.example.com after 0 sec "
            "(debug=ignored,"
            " driver=null rewrite driver, should only be used in tests)."),
        SynthesizeWarning(
            "Write of 9 bytes for url http://www.example.com received "
            "after being abandoned for timing out."),
        SynthesizeWarning(
            "Response for url http://www.example.com completed "
            "with status 200 (null) after being abandoned for timing out."));

    EXPECT_EQ(expected, message_output);
  }

  GoogleString mapped_url_;
  GoogleString debug_info_;
  NullThreadSystem thread_system_;
  MockTimer timer_;
  request_rec request_;
  ApacheWriter* apache_writer_;
  RequestHeaders* request_headers_;
  RequestContextPtr request_ctx_;
  MockMessageHandler message_handler_;

  scoped_ptr<RewriteOptions> rewrite_options_;
  scoped_ptr<ApacheFetch> apache_fetch_;
  NullCondvar* condvar_;  // owned by apache_fetch_
};

TEST_F(ApacheFetchTest, SuccessBuffered) {
  EXPECT_TRUE(apache_fetch_->Write("hello ", &message_handler_));
  EXPECT_TRUE(apache_fetch_->Write("world", &message_handler_));
  EXPECT_TRUE(apache_fetch_->Flush(&message_handler_));
  EXPECT_TRUE(apache_fetch_->Write(".", &message_handler_));
  EXPECT_TRUE(apache_fetch_->Flush(&message_handler_));
  // All flushes are dropped, all writed are buffered.
  EXPECT_EQ("", MockApache::ActionsSinceLastCall());

  WaitExpectSuccess();

  EXPECT_EQ(200, request_.status);
  EXPECT_EQ(
      "ap_set_content_type(text/plain) "
      "ap_remove_output_filter(MOD_EXPIRES) "
      "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
      "ap_set_content_type(text/plain) "
      "ap_rwrite(hello world.)",  /* note: writes combined into one call */
      MockApache::ActionsSinceLastCall());

  // TODO(jefftk): Cookies are present here even though we asked ApacheWriter to
  // remove them because of an ApacheWriter bug.
  EXPECT_EQ("Content-Type: text/plain\n"
            "Set-Cookie: test=cookie\n"
            "Set-Cookie: tasty=cookie\n"
            "Set-Cookie2: obselete\n"
            "Date: Thu, 01 Jan 1970 00:00:00 GMT\n"
            "X-Content-Type-Options: nosniff\n"
            "Cache-Control: max-age=0, no-cache\n",
            HeadersOutToString(&request_));
}

TEST_F(ApacheFetchTest, SuccessUnbuffered) {
  apache_fetch_->set_buffered(false);
  EXPECT_TRUE(apache_fetch_->Write("hello ", &message_handler_));
  EXPECT_EQ(200, request_.status);
  EXPECT_EQ(
      "ap_set_content_type(text/plain) "
      "ap_remove_output_filter(MOD_EXPIRES) "
      "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
      "ap_set_content_type(text/plain) "
      "ap_rwrite(hello )",
      MockApache::ActionsSinceLastCall());

  // TODO(jefftk): Cookies are present here even though we asked ApacheWriter to
  // remove them because of an ApacheWriter bug.
  EXPECT_EQ("Content-Type: text/plain\n"
            "Set-Cookie: test=cookie\n"
            "Set-Cookie: tasty=cookie\n"
            "Set-Cookie2: obselete\n"
            "Date: Thu, 01 Jan 1970 00:00:00 GMT\n"
            "X-Content-Type-Options: nosniff\n"
            "Cache-Control: max-age=0, no-cache\n",
            HeadersOutToString(&request_));

  EXPECT_TRUE(apache_fetch_->Write("world", &message_handler_));
  EXPECT_EQ("ap_rwrite(world)", MockApache::ActionsSinceLastCall());
  EXPECT_TRUE(apache_fetch_->Flush(&message_handler_));
  EXPECT_EQ("ap_rflush()", MockApache::ActionsSinceLastCall());
  EXPECT_TRUE(apache_fetch_->Write(".", &message_handler_));
  EXPECT_EQ("ap_rwrite(.)", MockApache::ActionsSinceLastCall());
  EXPECT_TRUE(apache_fetch_->Flush(&message_handler_));
  EXPECT_EQ("ap_rflush()", MockApache::ActionsSinceLastCall());

  apache_fetch_->Done(true);
}

TEST_F(ApacheFetchTest, NotFound404Buffered) {
  apache_fetch_->response_headers()->set_status_code(404);
  EXPECT_TRUE(apache_fetch_->Write("Couldn't find it.", &message_handler_));
  // Writes are buffered.
  EXPECT_EQ("", MockApache::ActionsSinceLastCall());

  WaitExpectSuccess();

  EXPECT_EQ(404, request_.status);
  EXPECT_EQ("Content-Type: text/plain\n"
            "Set-Cookie: test=cookie\n"
            "Set-Cookie: tasty=cookie\n"
            "Set-Cookie2: obselete\n"
            "Date: Thu, 01 Jan 1970 00:00:00 GMT\n"
            "X-Content-Type-Options: nosniff\n"
            "Cache-Control: max-age=0, no-cache\n",
            HeadersOutToString(&request_));
  EXPECT_EQ(
      "ap_set_content_type(text/plain) "
      "ap_remove_output_filter(MOD_EXPIRES) "
      "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
      "ap_set_content_type(text/plain) "
      "ap_rwrite(Couldn't find it.)",
      MockApache::ActionsSinceLastCall());
}

TEST_F(ApacheFetchTest, NotFound404Unbuffered) {
  apache_fetch_->set_buffered(false);
  apache_fetch_->response_headers()->set_status_code(404);
  EXPECT_TRUE(apache_fetch_->Write("Couldn't find it.", &message_handler_));
  EXPECT_EQ(404, request_.status);
  EXPECT_EQ("Content-Type: text/plain\n"
            "Set-Cookie: test=cookie\n"
            "Set-Cookie: tasty=cookie\n"
            "Set-Cookie2: obselete\n"
            "Date: Thu, 01 Jan 1970 00:00:00 GMT\n"
            "X-Content-Type-Options: nosniff\n"
            "Cache-Control: max-age=0, no-cache\n",
            HeadersOutToString(&request_));
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
  apache_fetch_->response_headers()->set_status_code(200);
  EXPECT_TRUE(apache_fetch_->response_headers()->RemoveAll(
      HttpAttributes::kContentType));
  EXPECT_TRUE(apache_fetch_->Write("Example response.", &message_handler_));
  // Writes are buffered.
  EXPECT_EQ("", MockApache::ActionsSinceLastCall());

  WaitExpectSuccess();

  EXPECT_EQ(403, request_.status);
  EXPECT_EQ("Set-Cookie: test=cookie\n"
            "Set-Cookie: tasty=cookie\n"
            "Set-Cookie2: obselete\n"
            "Content-Type: text/html\n"
            "Date: Thu, 01 Jan 1970 00:00:00 GMT\n"
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
  apache_fetch_->set_buffered(false);
  apache_fetch_->response_headers()->set_status_code(200);
  EXPECT_TRUE(apache_fetch_->response_headers()->RemoveAll(
      HttpAttributes::kContentType));
  EXPECT_TRUE(apache_fetch_->Write("Example response.", &message_handler_));
  EXPECT_EQ(403, request_.status);
  EXPECT_EQ("Set-Cookie: test=cookie\n"
            "Set-Cookie: tasty=cookie\n"
            "Set-Cookie2: obselete\n"
            "Content-Type: text/html\n"
            "Date: Thu, 01 Jan 1970 00:00:00 GMT\n"
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
  apache_fetch_->response_headers()->set_status_code(301);
  EXPECT_TRUE(apache_fetch_->response_headers()->RemoveAll("Content-Type"));
  apache_fetch_->response_headers()->Add("Location", "elsewhere");

  EXPECT_TRUE(apache_fetch_->Write("moved elsewhere", &message_handler_));
  // Writes are buffered.
  EXPECT_EQ("", MockApache::ActionsSinceLastCall());

  WaitExpectSuccess();

  EXPECT_EQ(403, request_.status);
  EXPECT_EQ("Set-Cookie: test=cookie\n"
            "Set-Cookie: tasty=cookie\n"
            "Set-Cookie2: obselete\n"
            "Location: elsewhere\n"
            "Content-Type: text/html\n"
            "Date: Thu, 01 Jan 1970 00:00:00 GMT\n"
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
  apache_fetch_->set_buffered(false);
  apache_fetch_->response_headers()->set_status_code(301);
  EXPECT_TRUE(apache_fetch_->response_headers()->RemoveAll("Content-Type"));
  apache_fetch_->response_headers()->Add("Location", "elsewhere");
  EXPECT_TRUE(apache_fetch_->Write("moved elsewhere", &message_handler_));
  EXPECT_EQ(403, request_.status);
  EXPECT_EQ("Set-Cookie: test=cookie\n"
            "Set-Cookie: tasty=cookie\n"
            "Set-Cookie2: obselete\n"
            "Location: elsewhere\n"
            "Content-Type: text/html\n"
            "Date: Thu, 01 Jan 1970 00:00:00 GMT\n"
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
  apache_fetch_->response_headers()->set_status_code(304);
  EXPECT_TRUE(apache_fetch_->response_headers()->RemoveAll("Content-Type"));

  EXPECT_TRUE(apache_fetch_->Write("not modified", &message_handler_));
  // Writes are buffered.
  EXPECT_EQ("", MockApache::ActionsSinceLastCall());

  WaitExpectSuccess();

  EXPECT_EQ(304, request_.status);
  EXPECT_EQ("Set-Cookie: test=cookie\n"
            "Set-Cookie: tasty=cookie\n"
            "Set-Cookie2: obselete\n"
            "Date: Thu, 01 Jan 1970 00:00:00 GMT\n"
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
  apache_fetch_->set_buffered(false);
  apache_fetch_->response_headers()->set_status_code(304);
  EXPECT_TRUE(apache_fetch_->response_headers()->RemoveAll("Content-Type"));
  EXPECT_TRUE(apache_fetch_->Write("not modified", &message_handler_));
  EXPECT_EQ(304, request_.status);
  EXPECT_EQ("Set-Cookie: test=cookie\n"
            "Set-Cookie: tasty=cookie\n"
            "Set-Cookie2: obselete\n"
            "Date: Thu, 01 Jan 1970 00:00:00 GMT\n"
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
  apache_fetch_->response_headers()->set_status_code(204);
  EXPECT_TRUE(apache_fetch_->response_headers()->RemoveAll("Content-Type"));
  apache_fetch_->HeadersComplete();
  // Headers are buffered.
  EXPECT_EQ("", MockApache::ActionsSinceLastCall());

  WaitExpectSuccess();

  EXPECT_EQ(204, request_.status);
  EXPECT_EQ("Set-Cookie: test=cookie\n"
            "Set-Cookie: tasty=cookie\n"
            "Set-Cookie2: obselete\n"
            "Date: Thu, 01 Jan 1970 00:00:00 GMT\n"
            "X-Content-Type-Options: nosniff\n"
            "Cache-Control: max-age=0, no-cache\n",
            HeadersOutToString(&request_));
  EXPECT_EQ("ap_remove_output_filter(MOD_EXPIRES) "
            "ap_remove_output_filter(FIXUP_HEADERS_OUT)",
            MockApache::ActionsSinceLastCall());
}

TEST_F(ApacheFetchTest, NoContentType204Unbuffered) {
  apache_fetch_->set_buffered(false);
  apache_fetch_->response_headers()->set_status_code(204);
  EXPECT_TRUE(apache_fetch_->response_headers()->RemoveAll("Content-Type"));
  apache_fetch_->HeadersComplete();
  EXPECT_EQ(204, request_.status);
  EXPECT_EQ("Set-Cookie: test=cookie\n"
            "Set-Cookie: tasty=cookie\n"
            "Set-Cookie2: obselete\n"
            "Date: Thu, 01 Jan 1970 00:00:00 GMT\n"
            "X-Content-Type-Options: nosniff\n"
            "Cache-Control: max-age=0, no-cache\n",
            HeadersOutToString(&request_));
  EXPECT_EQ("ap_remove_output_filter(MOD_EXPIRES) "
            "ap_remove_output_filter(FIXUP_HEADERS_OUT)",
            MockApache::ActionsSinceLastCall());
  apache_fetch_->Done(true);
}

TEST_F(ApacheFetchTest, AbandonedAfterWritingBuffered) {
  // Write() will call HeadersComplete().
  EXPECT_TRUE(apache_fetch_->Write("hello ", &message_handler_));
  EXPECT_EQ("", MockApache::ActionsSinceLastCall());

  EXPECT_FALSE(apache_fetch_->Wait(NULL /* rewrite_driver */));
  PostAbandonmentHelper();
}

TEST_F(ApacheFetchTest, AbandonedBeforeWritingBuffered) {
  EXPECT_FALSE(apache_fetch_->Wait(NULL /* rewrite_driver */));

  PostAbandonmentHelper();
}

// Unbuffered fetches can't get abandoned, so not testing that case.

}  // namespace net_instaweb
