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

#include <vector>

#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CountingUrlAsyncFetcher;
class DelayCache;
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
class MockTimeCache;
class MockUrlFetcher;
class PropertyCache;
class ServerContext;
class RewriteDriver;
class RewriteFilter;
class RewriteOptions;
class Scheduler;
class ThreadsafeCache;
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

  class CreateFilterCallback {
   public:
    CreateFilterCallback() {}
    virtual ~CreateFilterCallback();
    virtual HtmlFilter* Done(RewriteDriver* driver) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(CreateFilterCallback);
  };

  class CreateRewriterCallback {
   public:
    CreateRewriterCallback() {}
    virtual ~CreateRewriterCallback();
    virtual RewriteFilter* Done(RewriteDriver* driver) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(CreateRewriterCallback);
  };

  class PlatformSpecificConfigurationCallback {
   public:
    PlatformSpecificConfigurationCallback() {}
    virtual ~PlatformSpecificConfigurationCallback();
    virtual void Done(RewriteDriver* driver) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(PlatformSpecificConfigurationCallback);
  };

  TestRewriteDriverFactory(const StringPiece& temp_dir,
                           MockUrlFetcher* mock_fetcher,
                           MockUrlFetcher* mock_distributed_fetcher);
  virtual ~TestRewriteDriverFactory();

  DelayCache* delay_cache() { return delay_cache_; }
  LRUCache* lru_cache() { return lru_cache_; }
  MockTimer* mock_timer() { return mock_timer_; }
  MockHasher* mock_hasher() { return mock_hasher_; }
  MemFileSystem* mem_file_system() { return mem_file_system_; }
  FakeUrlAsyncFetcher* mock_url_async_fetcher() {
    return mock_url_async_fetcher_.get();
  }
  WaitUrlAsyncFetcher* wait_url_async_fetcher() {
    return wait_url_async_fetcher_.get();
  }
  CountingUrlAsyncFetcher* counting_url_async_fetcher() {
    return counting_url_async_fetcher_;
  }
  CountingUrlAsyncFetcher* counting_distributed_async_fetcher() {
    return counting_distributed_async_fetcher_;
  }
  MockTimeCache* mock_time_cache() { return mock_time_cache_.get(); }

  void SetupWaitFetcher();
  void CallFetcherCallbacksForDriver(RewriteDriver* driver);
  MockMessageHandler* mock_message_handler() { return mock_message_handler_; }
  MockScheduler* mock_scheduler() { return mock_scheduler_; }
  bool use_test_url_namer() const { return use_test_url_namer_; }
  void SetUseTestUrlNamer(bool x);

  // Does NOT take ownership of the callback.
  void AddCreateFilterCallback(CreateFilterCallback* callback) {
    filter_callback_vector_.push_back(callback);
  }

  void ClearFilterCallbackVector() {
    filter_callback_vector_.clear();
  }

  // Does NOT take ownership of the callback.
  void AddCreateRewriterCallback(CreateRewriterCallback* callback) {
    rewriter_callback_vector_.push_back(callback);
  }

  void ClearRewriterCallbackVector() {
    rewriter_callback_vector_.clear();
  }

  // By default this is false, but can be reset.
  virtual bool UseBeaconResultsInFilters() const {
    return use_beacon_results_in_filters_;
  }

  void set_use_beacon_results_in_filters(bool b) {
    use_beacon_results_in_filters_ = b;
  }

  // Does NOT take ownership of the callback.
  void AddPlatformSpecificConfigurationCallback(
      PlatformSpecificConfigurationCallback* callback) {
    platform_config_vector_.push_back(callback);
  }

  void ClearPlatformSpecificConfigurationCallback() {
    platform_config_vector_.clear();
  }

  // Note that this disables ajax rewriting by default.
  virtual RewriteOptions* NewRewriteOptions();

  // Note that this enables html proxying.
  virtual ServerContext* NewServerContext();

  virtual bool IsDebugClient(const GoogleString& ip) const {
    return ip == "127.0.0.1";
  }

  // Enable or disable adding the contents of rewriter_callback_vector_ within
  // AddPlatformSpecificRewritePasses.
  void set_add_platform_specific_decoding_passes(bool value) {
    add_platform_specific_decoding_passes_ = value;
  }

  bool add_platform_specific_decoding_passes() const {
    return add_platform_specific_decoding_passes_;
  }

  // Advances the mock scheduler by delta_ms.
  void AdvanceTimeMs(int64 delta_ms);

  // Sets up the cohort in the PropertyCache provided.
  void SetupCohort(PropertyCache* cache, const GoogleString& cohort_name);

 protected:
  virtual Hasher* NewHasher();
  virtual MessageHandler* DefaultHtmlParseMessageHandler();
  virtual MessageHandler* DefaultMessageHandler();
  virtual UrlFetcher* DefaultUrlFetcher();
  virtual UrlAsyncFetcher* DefaultAsyncUrlFetcher();
  virtual UrlAsyncFetcher* DefaultDistributedUrlFetcher();
  virtual FileSystem* DefaultFileSystem();
  virtual Timer* DefaultTimer();
  virtual void SetupCaches(ServerContext* resource_manager);
  virtual UrlNamer* DefaultUrlNamer();
  virtual Scheduler* CreateScheduler();
  virtual void AddPlatformSpecificDecodingPasses(RewriteDriver* driver);
  virtual void AddPlatformSpecificRewritePasses(RewriteDriver* driver);
  virtual void ApplyPlatformSpecificConfiguration(RewriteDriver* driver);

 private:
  MockTimer* mock_timer_;  // owned by base class timer_.
  MockScheduler* mock_scheduler_;  // owned by RewriteDriverFactory::scheduler_.
  DelayCache* delay_cache_;
  scoped_ptr<ThreadsafeCache> threadsafe_cache_;
  LRUCache* lru_cache_;
  UrlFetcher* proxy_url_fetcher_;
  MockUrlFetcher* mock_url_fetcher_;
  MockUrlFetcher* mock_distributed_fetcher_;
  scoped_ptr<FakeUrlAsyncFetcher> mock_url_async_fetcher_;
  scoped_ptr<FakeUrlAsyncFetcher> mock_distributed_async_fetcher_;
  CountingUrlAsyncFetcher* counting_url_async_fetcher_;
  CountingUrlAsyncFetcher* counting_distributed_async_fetcher_;
  scoped_ptr<WaitUrlAsyncFetcher> wait_url_async_fetcher_;
  scoped_ptr<MockTimeCache> mock_time_cache_;
  MemFileSystem* mem_file_system_;  // owned by base class file_system_.
  MockHasher* mock_hasher_;
  SimpleStats simple_stats_;
  MockMessageHandler* mock_message_handler_;
  MockMessageHandler* mock_html_message_handler_;
  bool use_beacon_results_in_filters_;
  bool use_test_url_namer_;
  bool add_platform_specific_decoding_passes_;
  std::vector<CreateFilterCallback*> filter_callback_vector_;
  std::vector<CreateRewriterCallback*> rewriter_callback_vector_;
  std::vector<PlatformSpecificConfigurationCallback*> platform_config_vector_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_TEST_REWRITE_DRIVER_FACTORY_H_
