// Copyright 2010 and onwards Google Inc.
// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test framework for caching fetchers.  This is used by
// both cache_url_fetcher_test.cc and cache_url_async_fetcher_test.cc.

#ifndef NET_INSTAWEB_UTIL_CACHE_FETCHER_TEST_H_
#define NET_INSTAWEB_UTIL_CACHE_FETCHER_TEST_H_

#include <algorithm>
#include <utility>  // for pair
#include <vector>
#include "base/basictypes.h"
#include "base/logging.h"
#include "file/base/fileutils.h"
#include "net/base/selectserver.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/http_cache.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/meta_data.h"
#include "net/instaweb/util/public/mock_timer.h"
#include <string>
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/url_async_fetcher.h"
#include "net/instaweb/util/public/url_fetcher.h"

namespace net_instaweb {

class CacheFetcherTest : public testing::Test {
 protected:
  static const int kMaxSize;
  static const char kStartDate[];
  static const char kHtmlContent[];
  static const char kGoodUrl[];
  static const char kNotCachedUrl[];
  static const char kBadUrl[];
  static const char kHeaderName[];
  static const char kHeaderValue[];
  static const char kErrorMessage[];

  // This mock fetcher will only fetch kGoodUrl, returning kHtmlContent.
  // If you ask for any other URL it will fail.
  class MockFetcher : public UrlFetcher {
   public:
    MockFetcher() : num_fetches_(0) { }


    virtual bool StreamingFetchUrl(const std::string& url,
                                   const MetaData& request_headers,
                                   MetaData* response_headers,
                                   Writer* writer,
                                   MessageHandler* message_handler) {
      bool ret = false;
      if (url == kGoodUrl) {
        ret = Populate("public, max-age=300", response_headers, writer,
                       message_handler);
      } else if (url == kNotCachedUrl) {
        ret = Populate("no-cache", response_headers, writer,
                       message_handler);
      } else {
        writer->Write(kErrorMessage, message_handler);
      }
      ++num_fetches_;
      return ret;
    }

    int num_fetches() const { return num_fetches_; }
   private:
    bool Populate(const char* cache_control,
                  MetaData* response_headers, Writer* writer,
                  MessageHandler* message_handler) {
      response_headers->set_status_code(HttpStatus::kOK);
      response_headers->Add("Cache-Control", cache_control);
      response_headers->Add("Date", kStartDate);
      response_headers->Add(kHeaderName, kHeaderValue);
      response_headers->ComputeCaching();
      response_headers->set_headers_complete(true);
      writer->Write(kHtmlContent, message_handler);
      return true;
    }

    int num_fetches_;

    DISALLOW_COPY_AND_ASSIGN(MockFetcher);
  };

  // This is a pseudo-asynchronous interface to MockFetcher.  It performs
  // fetches instantly, but defers calling the callback until the user
  // calls CallCallbacks().  Then it will execute the deferred callbacks.
  class MockAsyncFetcher : public UrlAsyncFetcher {
   public:
    explicit MockAsyncFetcher(UrlFetcher* url_fetcher)
        : url_fetcher_(url_fetcher) {
    }

    virtual bool StreamingFetch(const std::string& url,
                                const MetaData& request_headers,
                                MetaData* response_headers,
                                Writer* writer,
                                MessageHandler* handler,
                                Callback* callback) {
      bool status = url_fetcher_->StreamingFetchUrl(
          url, request_headers, response_headers, writer, handler);
      deferred_callbacks_.push_back(std::make_pair(status, callback));
      return false;
    }

    void CallCallbacks() {
      for (int i = 0, n = deferred_callbacks_.size(); i < n; ++i) {
        bool status = deferred_callbacks_[i].first;
        Callback* callback = deferred_callbacks_[i].second;
        callback->Done(status);
      }
      deferred_callbacks_.clear();
    }

   private:
    UrlFetcher* url_fetcher_;
    std::vector<std::pair<bool, Callback*> > deferred_callbacks_;

    DISALLOW_COPY_AND_ASSIGN(MockAsyncFetcher);
  };

  CacheFetcherTest()
      : async_fetcher_(&mock_fetcher_),
        mock_timer_(0),
        http_cache_(new LRUCache(kMaxSize), &mock_timer_) {
    int64 start_time_ms;
    bool parsed = MetaData::ParseTime(kStartDate, &start_time_ms);
    CHECK(parsed);
    mock_timer_.set_time_ms(start_time_ms);
  }

  static void ValidateOutput(bool success, bool check_error_message,
                             const std::string& content,
                             const MetaData& response_headers) {
    if (success) {
      EXPECT_EQ(std::string(kHtmlContent), content);
      CharStarVector values;
      EXPECT_TRUE(response_headers.Lookup(kHeaderName, &values));
      EXPECT_EQ(1, values.size());
      EXPECT_EQ(std::string(kHeaderValue), values[0]);
    } else if (check_error_message) {
      EXPECT_EQ(std::string(kErrorMessage), content);
    }
  }

  MockFetcher mock_fetcher_;
  MockAsyncFetcher async_fetcher_;
  MockTimer mock_timer_;
  HTTPCache http_cache_;
  GoogleMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheFetcherTest);
};

const int CacheFetcherTest::kMaxSize = 10000;
const char CacheFetcherTest::kStartDate[] = "Sun, 16 Dec 1979 02:27:45 GMT";
const char CacheFetcherTest::kHtmlContent[] = "<html><body>Nuts!</body></html>";
const char CacheFetcherTest::kErrorMessage[] = "Invalid URL";
const char CacheFetcherTest::kGoodUrl[] = "http://pi.com";
const char CacheFetcherTest::kNotCachedUrl[] = "http://not_cacheable.com";
const char CacheFetcherTest::kBadUrl[] = "http://this_url_will_fail.com";
const char CacheFetcherTest::kHeaderName[] = "header-name";
const char CacheFetcherTest::kHeaderValue[] = "header value";

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_CACHE_FETCHER_TEST_H_
