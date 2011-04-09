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

#include "net/instaweb/http/public/http_dump_url_writer.h"

#include "base/basictypes.h"
#include "net/instaweb/http/public/fetcher_test.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/stdio_file_system.h"

namespace net_instaweb {

class HttpDumpUrlWriterTest : public FetcherTest {
 protected:
  HttpDumpUrlWriterTest()
      : mock_timer_(0),
        http_dump_writer_(GTestTempDir() + "/http_dump/", &mock_fetcher_,
                          &file_system_, &mock_timer_) {
  }

  virtual UrlFetcher* sync_fetcher() { return &http_dump_writer_; }

  virtual void SetUp() {
    RemoveFileIfPresent(kGoodUrl);
    RemoveFileIfPresent(kNotCachedUrl);
    RemoveFileIfPresent(kBadUrl);
  }

  void RemoveFileIfPresent(const char* url) {
    GoogleUrl gurl(url);
    GoogleString path;
    HttpDumpUrlFetcher::GetFilenameFromUrl(GTestTempDir() + "/http_dump/",
                                           gurl, &path, &message_handler_);
    file_system_.RemoveFile(path.c_str(), &message_handler_);
  }

  StdioFileSystem file_system_;
  MockTimer mock_timer_;
  HttpDumpUrlWriter http_dump_writer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpDumpUrlWriterTest);
};

TEST_F(HttpDumpUrlWriterTest, TestCachableWithSyncFetcher) {
  EXPECT_EQ(1, CountFetchesSync(kGoodUrl, true, true));
  EXPECT_EQ(0, CountFetchesSync(kGoodUrl, true, true));
}

TEST_F(HttpDumpUrlWriterTest, TestNonCachableWithSyncFetcher) {
  // When a HttpDumpUrlFetcher is implemented using a sync fetcher,
  // then non-cacheable URLs will result in sync-fetch successes.
  // As we are ignoring caching headers, we don't expect the
  // underlying fetcher to be called here either.
  EXPECT_EQ(1, CountFetchesSync(kNotCachedUrl, true, true));
  EXPECT_EQ(0, CountFetchesSync(kNotCachedUrl, true, true));
}

TEST_F(HttpDumpUrlWriterTest, TestCacheWithSyncFetcherFail) {
  EXPECT_EQ(1, CountFetchesSync(kBadUrl, false, true));
  // For now, we don't cache failure, so we expect a new fetch
  // on each request
  EXPECT_EQ(1, CountFetchesSync(kBadUrl, false, true));
}

}  // namespace net_instaweb
