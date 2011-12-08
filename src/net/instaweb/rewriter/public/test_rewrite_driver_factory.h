/*
 * Copyright 2011 Google Inc.
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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_TEST_REWRITE_DRIVER_FACTORY_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_TEST_REWRITE_DRIVER_FACTORY_H_

#include "base/scoped_ptr.h"            // for scoped_ptr
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/mock_time_cache.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/string_util.h"        // for StringPiece

namespace net_instaweb {

class CacheInterface;
class CountingUrlAsyncFetcher;
class FakeUrlAsyncFetcher;
class FileSystem;
class Hasher;
class HtmlFilter;
class LRUCache;
class MemFileSystem;
class MessageHandler;
class MockHasher;
class MockMessageHandler;
class MockScheduler;
class MockTimer;
class MockUrlFetcher;
class RewriteDriver;
class RewriteOptions;
class Scheduler;
class Timer;
class UrlAsyncFetcher;
class UrlFetcher;
class UrlNamer;
class WaitUrlAsyncFetcher;

// RewriteDriverFactory implementation for use in tests, using mock time,
// mock fetchers, and a memory-based file system.
class TestRewriteDriverFactory : public RewriteDriverFactory {
 public:
  static const int64 kStartTimeMs;  // Arbitrary time to start MockTimer.
  static const char kUrlNamerScheme[];  // Env.var URL_NAMER_SCHEME

  TestRewriteDriverFactory(const StringPiece& temp_dir,
                           MockUrlFetcher* mock_fetcher);
  virtual ~TestRewriteDriverFactory();

  LRUCache* lru_cache() { return lru_cache_; }
  MockTimer* mock_timer() { return mock_timer_; }
  MockHasher* mock_hasher() { return mock_hasher_; }
  MemFileSystem* mem_file_system() { return mem_file_system_; }
  WaitUrlAsyncFetcher* wait_url_async_fetcher() {
    return wait_url_async_fetcher_.get();
  }
  CountingUrlAsyncFetcher* counting_url_async_fetcher() {
    return counting_url_async_fetcher_;
  }
  MockTimeCache* mock_time_cache() { return &mock_time_cache_; }

  void SetupWaitFetcher();
  void CallFetcherCallbacksForDriver(RewriteDriver* driver);
  MockMessageHandler* mock_message_handler() { return mock_message_handler_; }
  MockScheduler* mock_scheduler() { return mock_scheduler_; }
  bool use_test_url_namer() const { return use_test_url_namer_; }
  void SetUseTestUrlNamer(bool x);

  class CreateFilterCallback {
   public:
    CreateFilterCallback() {}
    virtual ~CreateFilterCallback();
    virtual HtmlFilter* Done(RewriteDriver* driver) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(CreateFilterCallback);
  };

  void AddCreateFilterCallback(CreateFilterCallback* callback) {
    callback_vector_.push_back(callback);
  }

  void ClearFilterCallbackVector() {
    callback_vector_.clear();
  }

  // Note that this disables ajax rewriting by default.
  virtual RewriteOptions* NewRewriteOptions();

 protected:
  virtual Hasher* NewHasher();
  virtual MessageHandler* DefaultHtmlParseMessageHandler();
  virtual MessageHandler* DefaultMessageHandler();
  virtual UrlFetcher* DefaultUrlFetcher();
  virtual UrlAsyncFetcher* DefaultAsyncUrlFetcher();
  virtual FileSystem* DefaultFileSystem();
  virtual Timer* DefaultTimer();
  virtual CacheInterface* DefaultCacheInterface();
  virtual UrlNamer* DefaultUrlNamer();
  virtual bool ShouldWriteResourcesToFileSystem() { return true; }
  virtual Scheduler* CreateScheduler();
  virtual void AddPlatformSpecificRewritePasses(RewriteDriver* driver);

 private:
  MockTimer* mock_timer_;  // owned by base class timer_.
  MockScheduler* mock_scheduler_;  // owned by base class scheduler_;
  LRUCache* lru_cache_;
  UrlFetcher* proxy_url_fetcher_;
  MockUrlFetcher* mock_url_fetcher_;
  scoped_ptr<FakeUrlAsyncFetcher> mock_url_async_fetcher_;
  CountingUrlAsyncFetcher* counting_url_async_fetcher_;
  scoped_ptr<WaitUrlAsyncFetcher> wait_url_async_fetcher_;
  MockTimeCache mock_time_cache_;
  MemFileSystem* mem_file_system_;  // owned by base class file_system_.
  MockHasher* mock_hasher_;
  SimpleStats simple_stats_;
  MockMessageHandler* mock_message_handler_;
  MockMessageHandler* mock_html_message_handler_;
  bool use_test_url_namer_;
  std::vector<CreateFilterCallback*> callback_vector_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_TEST_REWRITE_DRIVER_FACTORY_H_
