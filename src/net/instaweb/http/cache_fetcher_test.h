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

// Unit-test framework for caching fetchers.  This is used by
// both cache_url_fetcher_test.cc and cache_url_async_fetcher_test.cc.

#ifndef NET_INSTAWEB_HTTP_CACHE_FETCHER_TEST_H_
#define NET_INSTAWEB_HTTP_CACHE_FETCHER_TEST_H_

#include "net/instaweb/util/public/basictypes.h"
#include "base/logging.h"
#include "net/instaweb/http/public/fetcher_test.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_timer.h"

namespace net_instaweb {

class CacheFetcherTest : public FetcherTest {
 protected:
  static const int kMaxSize;

  CacheFetcherTest()
      : mock_timer_(0),
        http_cache_(new LRUCache(kMaxSize), &mock_timer_, statistics_) {
    int64 start_time_ms;
    bool parsed = ResponseHeaders::ParseTime(kStartDate, &start_time_ms);
    CHECK(parsed);
    mock_timer_.set_time_ms(start_time_ms);
  }

  MockTimer mock_timer_;
  HTTPCache http_cache_;
  GoogleMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheFetcherTest);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_CACHE_FETCHER_TEST_H_
