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

#include "base/basictypes.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include <string>
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

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
  std::string content_;
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
  CharStarVector v;
  ASSERT_TRUE(response.Lookup(HttpAttributes::kContentEncoding, &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(std::string(HttpAttributes::kGzip), v[0]);
  EXPECT_EQ(5513, content_.size());
  v.clear();
  ASSERT_TRUE(response.Lookup(HttpAttributes::kContentLength, &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(std::string("5513"), v[0]);
}

TEST_F(HttpDumpUrlFetcherTest, TestReadUncompressedFromGzippedDump) {
  RequestHeaders request;
  ResponseHeaders response;
  ASSERT_TRUE(http_dump_fetcher_.StreamingFetchUrl(
      "http://www.google.com", request, &response, &content_writer_,
      &message_handler_));
  CharStarVector v;
  if (response.Lookup(HttpAttributes::kContentEncoding, &v)) {
    ASSERT_EQ(1, v.size());
    EXPECT_NE(std::string(HttpAttributes::kGzip), v[0]);
  }
  EXPECT_EQ(14450, content_.size());
  v.clear();
  ASSERT_TRUE(response.Lookup(HttpAttributes::kContentLength, &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(std::string("14450"), v[0]);
}

}  // namespace net_instaweb
