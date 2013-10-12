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

#include <cstdlib>                     // for getenv
#include <vector>

#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"

#include "base/logging.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/rate_controller.h"
#include "net/instaweb/http/public/rate_controlling_url_async_fetcher.h"
#include "net/instaweb/http/public/wait_url_async_fetcher.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/test_distributed_fetcher.h"
#include "net/instaweb/rewriter/public/test_url_namer.h"
#include "net/instaweb/util/public/basictypes.h"        // for int64
#include "net/instaweb/util/public/cache_property_store.h"
#include "net/instaweb/util/public/delay_cache.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/mock_time_cache.h"
#include "net/instaweb/util/public/platform.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/scoped_ptr.h"            // for scoped_ptr
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"  // for StrCat, etc
#include "net/instaweb/util/public/threadsafe_cache.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "pagespeed/kernel/util/mock_nonce_generator.h"

namespace net_instaweb {

class FileSystem;
class Hasher;
class HtmlFilter;
class MessageHandler;
class NonceGenerator;
class RewriteFilter;
class Scheduler;
class Statistics;
class UrlAsyncFetcher;
class UrlNamer;

namespace {

const int kCacheSize  = 10*1000*1000;

class TestServerContext : public ServerContext {
 public:
  explicit TestServerContext(RewriteDriverFactory* factory)
      : ServerContext(factory) {
  }

  virtual ~TestServerContext() {
  }

  virtual bool ProxiesHtml() const { return true; }
};

}  // namespace

const int64 TestRewriteDriverFactory::kStartTimeMs =
    MockTimer::kApr_5_2010_ms - 2 * Timer::kMonthMs;

const int TestRewriteDriverFactory::kMaxFetchGlobalQueueSize;
const int TestRewriteDriverFactory::kFetchesPerHostOutgoingRequestThreshold;
const int TestRewriteDriverFactory::kFetchesPerHostQueuedRequestThreshold;

const char TestRewriteDriverFactory::kUrlNamerScheme[] = "URL_NAMER_SCHEME";

TestRewriteDriverFactory::TestRewriteDriverFactory(
    const StringPiece& temp_dir, MockUrlFetcher* mock_fetcher,
    TestDistributedFetcher* test_distributed_fetcher)
    : RewriteDriverFactory(Platform::CreateThreadSystem()),
      mock_timer_(NULL),
      mock_scheduler_(NULL),
      delay_cache_(NULL),
      mock_url_fetcher_(mock_fetcher),
      test_distributed_fetcher_(test_distributed_fetcher),
      rate_controlling_url_async_fetcher_(NULL),
      counting_distributed_async_fetcher_(NULL),
      mem_file_system_(NULL),
      mock_hasher_(NULL),
      mock_message_handler_(NULL),
      mock_html_message_handler_(NULL),
      use_beacon_results_in_filters_(false),
      add_platform_specific_decoding_passes_(true) {
  set_filename_prefix(StrCat(temp_dir, "/"));
  use_test_url_namer_ = (getenv(kUrlNamerScheme) != NULL &&
                         strcmp(getenv(kUrlNamerScheme), "test") == 0);
}

TestRewriteDriverFactory::~TestRewriteDriverFactory() {
}

void TestRewriteDriverFactory::InitStats(Statistics* statistics) {
  RateController::InitStats(statistics);
  RewriteDriverFactory::InitStats(statistics);
}

void TestRewriteDriverFactory::SetupWaitFetcher() {
  wait_url_async_fetcher_.reset(new WaitUrlAsyncFetcher(
      mock_url_fetcher_, thread_system()->NewMutex()));
  counting_url_async_fetcher_->set_fetcher(wait_url_async_fetcher_.get());
}

void TestRewriteDriverFactory::CallFetcherCallbacksForDriver(
    RewriteDriver* driver) {
  // Temporarily change the delayed-fetcher's mode so that it calls
  // callbacks immediately.  This is so that any further fetches
  // queued from a Done callback are immediately executed, until the
  // end of this method when we reset the state back to whatever it
  // was previously.
  bool pass_through_mode = wait_url_async_fetcher_->SetPassThroughMode(true);

  // TODO(jmarantz): parameterize whether this is to be used for
  // simulating delayed fetches for a ResourceFetch, in which case
  // we'll want WaitForCompletion, or whether this is to be used for
  // simulation of Rewrites, in which case we can do a TimedWait
  // according to the needs of the simulation.
  driver->WaitForCompletion();
  // Await quiescence will wait for cache puts to finish.
  mock_scheduler()->AwaitQuiescence();
  wait_url_async_fetcher_->SetPassThroughMode(pass_through_mode);
}

UrlAsyncFetcher* TestRewriteDriverFactory::DefaultAsyncUrlFetcher() {
  DCHECK(counting_url_async_fetcher_.get() == NULL);
  counting_url_async_fetcher_.reset(new CountingUrlAsyncFetcher(
      mock_url_fetcher_));
  rate_controlling_url_async_fetcher_ = new RateControllingUrlAsyncFetcher(
      counting_url_async_fetcher_.get(),
      kMaxFetchGlobalQueueSize,
      kFetchesPerHostOutgoingRequestThreshold,
      kFetchesPerHostQueuedRequestThreshold,
      thread_system(),
      statistics());
  return rate_controlling_url_async_fetcher_;
}

UrlAsyncFetcher* TestRewriteDriverFactory::DefaultDistributedUrlFetcher() {
  DCHECK(counting_distributed_async_fetcher_ == NULL);
  counting_distributed_async_fetcher_ = new CountingUrlAsyncFetcher(
      test_distributed_fetcher_);
  return counting_distributed_async_fetcher_;
}

FileSystem* TestRewriteDriverFactory::DefaultFileSystem() {
  DCHECK(mem_file_system_ == NULL);
  timer();  // ensures that mock_timer_ is initialized.
  mem_file_system_ = new MemFileSystem(thread_system(), mock_timer_);
  return mem_file_system_;
}

NonceGenerator* TestRewriteDriverFactory::DefaultNonceGenerator() {
  DCHECK(thread_system() != NULL);
  return new MockNonceGenerator(thread_system()->NewMutex());
}

Timer* TestRewriteDriverFactory::DefaultTimer() {
  DCHECK(mock_timer_ == NULL);
  mock_timer_ = new MockTimer(kStartTimeMs);
  return mock_timer_;
}

void TestRewriteDriverFactory::SetupCaches(ServerContext* server_context) {
  // TODO(jmarantz): Make the cache-ownership semantics consistent between
  // DelayCache and ThreadsafeCache.
  DCHECK(lru_cache_ == NULL);
  lru_cache_.reset(new LRUCache(kCacheSize));
  threadsafe_cache_.reset(
      new ThreadsafeCache(lru_cache_.get(), thread_system()->NewMutex()));
  mock_time_cache_.reset(new MockTimeCache(scheduler(),
                                           threadsafe_cache_.get()));
  delay_cache_ = new DelayCache(mock_time_cache_.get(), thread_system());
  HTTPCache* http_cache = new HTTPCache(delay_cache_, timer(),
                                        hasher(), statistics());
  server_context->set_http_cache(http_cache);
  server_context->set_metadata_cache(delay_cache_);
  cache_property_store_ =
      new CachePropertyStore(
          "test/", delay_cache_, timer(), statistics(), thread_system());
  server_context->set_cache_property_store(cache_property_store_);
  server_context->MakePagePropertyCache(cache_property_store_);
  TakeOwnership(delay_cache_);
}

Hasher* TestRewriteDriverFactory::NewHasher() {
  DCHECK(mock_hasher_ == NULL);
  mock_hasher_ = new MockHasher;
  return mock_hasher_;
}

MessageHandler* TestRewriteDriverFactory::DefaultMessageHandler() {
  DCHECK(mock_message_handler_ == NULL);
  mock_message_handler_ = new MockMessageHandler(thread_system()->NewMutex());
  return mock_message_handler_;
}

MessageHandler* TestRewriteDriverFactory::DefaultHtmlParseMessageHandler() {
  DCHECK(mock_html_message_handler_ == NULL);
  mock_html_message_handler_ = new MockMessageHandler(
      thread_system()->NewMutex());
  return mock_html_message_handler_;
}

UrlNamer* TestRewriteDriverFactory::DefaultUrlNamer() {
  return (use_test_url_namer_
          ? new TestUrlNamer()
          : RewriteDriverFactory::DefaultUrlNamer());
}

void TestRewriteDriverFactory::SetUseTestUrlNamer(bool x) {
  if (use_test_url_namer_ != x) {
    use_test_url_namer_ = x;
    set_url_namer(DefaultUrlNamer());
  }
}

Scheduler* TestRewriteDriverFactory::CreateScheduler() {
  DCHECK(mock_scheduler_ == NULL);
  timer();  // make sure mock_timer_ is created.
  mock_scheduler_ = new MockScheduler(thread_system(), mock_timer_);
  return mock_scheduler_;
}

RewriteOptions* TestRewriteDriverFactory::NewRewriteOptions() {
  RewriteOptions* options = RewriteDriverFactory::NewRewriteOptions();
  options->set_in_place_rewriting_enabled(false);
  // As we are using mock time, we need to set a consistent deadline here,
  // as otherwise when running under Valgrind some tests will finish
  // with different HTML headers than expected.
  options->set_rewrite_deadline_ms(20);
  // TODO(sligocki): Once this becomes default on in RewriteOptions, remove
  // this set here.
  options->set_preserve_url_relativity(true);
  return options;
}

ServerContext* TestRewriteDriverFactory::NewServerContext() {
  return new TestServerContext(this);
}

void TestRewriteDriverFactory::AddPlatformSpecificDecodingPasses(
    RewriteDriver* driver) {
  if (add_platform_specific_decoding_passes_) {
    for (std::size_t i = 0; i < rewriter_callback_vector_.size(); i++) {
      RewriteFilter* filter = rewriter_callback_vector_[i]->Done(driver);
      driver->AppendRewriteFilter(filter);
    }
  }
}

void TestRewriteDriverFactory::AddPlatformSpecificRewritePasses(
    RewriteDriver* driver) {
  for (std::size_t i = 0; i < filter_callback_vector_.size(); i++) {
    HtmlFilter* filter = filter_callback_vector_[i]->Done(driver);
    driver->AddOwnedPostRenderFilter(filter);
  }
  for (std::size_t i = 0; i < rewriter_callback_vector_.size(); i++) {
    RewriteFilter* filter = rewriter_callback_vector_[i]->Done(driver);
    driver->AppendRewriteFilter(filter);
  }
}

void TestRewriteDriverFactory::ApplyPlatformSpecificConfiguration(
    RewriteDriver* driver) {
  for (std::size_t i = 0; i < platform_config_vector_.size(); i++) {
    platform_config_vector_[i]->Done(driver);
  }
}

void TestRewriteDriverFactory::AdvanceTimeMs(int64 delta_ms) {
  mock_scheduler_->AdvanceTimeMs(delta_ms);
}

const PropertyCache::Cohort* TestRewriteDriverFactory::SetupCohort(
    PropertyCache* cache, const GoogleString& cohort_name) {
  PropertyCache::InitCohortStats(cohort_name, statistics());
  cache_property_store_->AddCohort(cohort_name);
  return cache->AddCohort(cohort_name);
}

TestRewriteDriverFactory::CreateFilterCallback::~CreateFilterCallback() {
}

TestRewriteDriverFactory::CreateRewriterCallback::~CreateRewriterCallback() {
}

TestRewriteDriverFactory::PlatformSpecificConfigurationCallback::
    ~PlatformSpecificConfigurationCallback() {
}

}  // namespace net_instaweb
