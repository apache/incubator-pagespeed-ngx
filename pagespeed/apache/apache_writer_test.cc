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

#include "pagespeed/apache/apache_writer.h"

#include "pagespeed/apache/apache_httpd_includes.h"
#include "pagespeed/apache/header_util.h"
#include "pagespeed/apache/mock_apache.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/null_thread_system.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

class ApacheWriterTest : public testing::Test {
 public:
  ApacheWriterTest() {
    MockApache::Initialize();
    MockApache::PrepareRequest(&request_);

    apache_writer_.reset(new ApacheWriter(&request_, &thread_system_));
    response_headers_.reset(new ResponseHeaders());

    // Standard response headers, some overridden in tests.
    response_headers_->set_status_code(200 /* OK */);

    response_headers_->set_major_version(1);
    response_headers_->set_minor_version(1);  // HTTP 1.1

    // Set some example headers.
    response_headers_->MergeContentType("text/plain");
    response_headers_->Add("Set-Cookie", "test=cookie");
    response_headers_->Add("Transfer-Encoding", "chunked");  // Will be removed.
    response_headers_->Add("Content-Length", "42");          // Will be removed.
  }
  virtual ~ApacheWriterTest() {
    MockApache::CleanupRequest(&request_);
    MockApache::Terminate();
  }

 protected:
  request_rec request_;
  scoped_ptr<ApacheWriter> apache_writer_;
  scoped_ptr<ResponseHeaders> response_headers_;
  NullMessageHandler message_handler_;
  NullThreadSystem thread_system_;
};

TEST_F(ApacheWriterTest, SimpleUsage) {
  apache_writer_->OutputHeaders(response_headers_.get());

  EXPECT_TRUE(apache_writer_->Write("hello world", &message_handler_));
  EXPECT_EQ(200, request_.status);
  // TODO(jefftk): figure out why we're calling ap_set_content_type twice.
  EXPECT_EQ(
      "ap_set_content_type(text/plain) "
      "ap_set_content_type(text/plain) "
      "ap_rwrite(hello world)",
      MockApache::ActionsSinceLastCall());
  EXPECT_EQ("Content-Type: text/plain\n"
            "Set-Cookie: test=cookie\n",
            HeadersOutToString(&request_));

  EXPECT_TRUE(apache_writer_->Flush(&message_handler_));
  EXPECT_EQ("ap_rflush()", MockApache::ActionsSinceLastCall());

  EXPECT_TRUE(apache_writer_->Write(".", &message_handler_));
  EXPECT_EQ("ap_rwrite(.)", MockApache::ActionsSinceLastCall());
}

TEST_F(ApacheWriterTest, HTTP10) {
  // Test HTTP 1.0.
  response_headers_->set_major_version(1);
  response_headers_->set_minor_version(0);
  apache_writer_->OutputHeaders(response_headers_.get());
  EXPECT_TRUE(apache_writer_->Write("hello world", &message_handler_));
  EXPECT_EQ(200, request_.status);
  EXPECT_EQ(
      "ap_set_content_type(text/plain) "
      "ap_set_content_type(text/plain) "
      "ap_rwrite(hello world)",
      MockApache::ActionsSinceLastCall());
  EXPECT_EQ("Content-Type: text/plain\n"
            "Set-Cookie: test=cookie\n",
            HeadersOutToString(&request_));
  EXPECT_EQ("force-response-1.0: 1\n",
            SubprocessEnvToString(&request_));
}

TEST_F(ApacheWriterTest, ErrorStatusCode) {
  response_headers_->set_status_code(404);
  apache_writer_->OutputHeaders(response_headers_.get());
  EXPECT_TRUE(apache_writer_->Write("hello world", &message_handler_));
  EXPECT_EQ(404, request_.status);
  EXPECT_EQ(
      "ap_set_content_type(text/plain) "
      "ap_set_content_type(text/plain) "
      "ap_rwrite(hello world)",
      MockApache::ActionsSinceLastCall());
  EXPECT_EQ("Content-Type: text/plain\n"
            "Set-Cookie: test=cookie\n",
            HeadersOutToString(&request_));
}

TEST_F(ApacheWriterTest, DisableDownstream) {
  apache_writer_->set_disable_downstream_header_filters(true);
  apache_writer_->OutputHeaders(response_headers_.get());
  EXPECT_TRUE(apache_writer_->Write("hello world", &message_handler_));
  EXPECT_EQ(200, request_.status);
  EXPECT_EQ(
      "ap_set_content_type(text/plain) "
      "ap_remove_output_filter(MOD_EXPIRES) "
      "ap_remove_output_filter(FIXUP_HEADERS_OUT) "
      "ap_set_content_type(text/plain) "
      "ap_rwrite(hello world)",
      MockApache::ActionsSinceLastCall());
  EXPECT_EQ("Content-Type: text/plain\n"
            "Set-Cookie: test=cookie\n",
            HeadersOutToString(&request_));
}

TEST_F(ApacheWriterTest, DisableDownstreamNoneToDisable) {
  request_.output_filters = NULL;  // Not a leak because they're pool allocated.
  apache_writer_->set_disable_downstream_header_filters(true);
  apache_writer_->OutputHeaders(response_headers_.get());
  EXPECT_TRUE(apache_writer_->Write("hello world", &message_handler_));
  EXPECT_EQ(200, request_.status);
  EXPECT_EQ(
      "ap_set_content_type(text/plain) "
      "ap_set_content_type(text/plain) "
      "ap_rwrite(hello world)",
      MockApache::ActionsSinceLastCall());
  EXPECT_EQ("Content-Type: text/plain\n"
            "Set-Cookie: test=cookie\n",
            HeadersOutToString(&request_));
}

TEST_F(ApacheWriterTest, StripCookies) {
  apache_writer_->set_strip_cookies(true);
  apache_writer_->OutputHeaders(response_headers_.get());
  EXPECT_TRUE(apache_writer_->Write("hello world", &message_handler_));
  EXPECT_EQ(200, request_.status);
  EXPECT_EQ(
      "ap_set_content_type(text/plain) "
      "ap_set_content_type(text/plain) "
      "ap_rwrite(hello world)",
      MockApache::ActionsSinceLastCall());
  // TODO(jefftk): actually strip cookies.
  EXPECT_EQ("Content-Type: text/plain\n"
            "Set-Cookie: test=cookie\n",
            HeadersOutToString(&request_));
}

}  // namespace net_instaweb
