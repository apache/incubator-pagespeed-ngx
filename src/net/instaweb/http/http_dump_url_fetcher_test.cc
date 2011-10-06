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

// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test the http dump fetcher, using a mock fetcher.  Note that
// the HTTP Dump Fetcher is, in essence, a caching fetcher except that:
//    1. It ignores caching headers completely
//    2. It uses file-based storage with no expectation of ever evicting
//       anything.
//
// TODO(jmarantz): consider making this class a special case of the
// combination of HTTPCache, FileCache, and HttpDumpUrlFetcher.

#include "net/instaweb/http/public/http_dump_url_fetcher.h"

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

namespace {

class HttpDumpUrlFetcherTest : public testing::Test {
 public:
  HttpDumpUrlFetcherTest()
      : content_writer_(&content_),
        mock_timer_(0),
        http_dump_fetcher_(
            GTestSrcDir() + "/net/instaweb/http/testdata",
            &file_system_,
            &mock_timer_) {
  }

 protected:
  StdioFileSystem file_system_;
  GoogleString content_;
  StringWriter content_writer_;
  MockTimer mock_timer_;
  HttpDumpUrlFetcher http_dump_fetcher_;
  GoogleMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpDumpUrlFetcherTest);
};

TEST_F(HttpDumpUrlFetcherTest, TestReadWithGzip) {
  RequestHeaders request;
  ResponseHeaders response;
  request.Add(HttpAttributes::kAcceptEncoding, HttpAttributes::kGzip);
  ASSERT_TRUE(http_dump_fetcher_.StreamingFetchUrl(
      "http://www.google.com", request, &response, &content_writer_,
      &message_handler_));
  ConstStringStarVector v;
  ASSERT_TRUE(response.Lookup(HttpAttributes::kContentEncoding, &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(GoogleString(HttpAttributes::kGzip), *(v[0]));
  EXPECT_EQ(5513, content_.size());
  v.clear();
  ASSERT_TRUE(response.Lookup(HttpAttributes::kContentLength, &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(GoogleString("5513"), *(v[0]));
}

TEST_F(HttpDumpUrlFetcherTest, TestReadUncompressedFromGzippedDump) {
  RequestHeaders request;
  ResponseHeaders response;
  ASSERT_TRUE(http_dump_fetcher_.StreamingFetchUrl(
      "http://www.google.com", request, &response, &content_writer_,
      &message_handler_));
  ConstStringStarVector v;
  if (response.Lookup(HttpAttributes::kContentEncoding, &v)) {
    ASSERT_EQ(1, v.size());
    EXPECT_NE(GoogleString(HttpAttributes::kGzip), *(v[0]));
  }
  EXPECT_EQ(14450, content_.size());
  v.clear();
  ASSERT_TRUE(response.Lookup(HttpAttributes::kContentLength, &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(GoogleString("14450"), *(v[0]));
}

// Helper that checks the Date: field as it starts writing.
class CheckDateHeaderWriter : public Writer {
 public:
  CheckDateHeaderWriter(const MockTimer* timer, ResponseHeaders* headers)
      : write_called_(false), timer_(timer), headers_(headers) {}
  virtual ~CheckDateHeaderWriter() {}

  virtual bool Write(const StringPiece& content, MessageHandler* handler) {
    write_called_ = true;
    headers_->ComputeCaching();
    EXPECT_EQ(timer_->NowMs(), headers_->fetch_time_ms());
    return true;
  }

  virtual bool Flush(MessageHandler* handler) {
    return true;
  }

  bool write_called() const { return write_called_; }

 private:
  bool write_called_;
  const MockTimer* timer_;
  ResponseHeaders* headers_;
  DISALLOW_COPY_AND_ASSIGN(CheckDateHeaderWriter);
};


TEST_F(HttpDumpUrlFetcherTest, TestDateAdjustment) {
  // Set a time in 2030s, which should be bigger than the time of the slurp,
  // which is a prerequisite for date adjustment
  mock_timer_.SetTimeUs(60 * Timer::kYearMs * Timer::kMsUs);

  // Make sure that date fixing up works in time for first write ---
  // which is needed for adapting it into an async fetcher.
  RequestHeaders request;
  ResponseHeaders response;
  CheckDateHeaderWriter check_date(&mock_timer_, &response);
  EXPECT_TRUE(http_dump_fetcher_.StreamingFetchUrl(
      "http://www.google.com", request, &response, &check_date,
      &message_handler_));
  EXPECT_TRUE(check_date.write_called());
}

}  // namespace

}  // namespace net_instaweb
