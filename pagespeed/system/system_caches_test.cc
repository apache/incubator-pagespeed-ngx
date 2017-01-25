/*
 * Copyright 2013 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)
// Unit tests for common cache configuration code.

#include "pagespeed/system/system_caches.h"

#include <cstdlib>
#include <vector>

#include "apr_poll.h"
#include "apr_pools.h"
#include "apr_thread_proc.h"
#include "apr_version.h"
#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/custom_rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "pagespeed/system/admin_site.h"
#include "pagespeed/system/apr_mem_cache.h"
#include "pagespeed/system/system_cache_path.h"
#include "pagespeed/system/system_rewrite_options.h"
#include "pagespeed/system/system_server_context.h"
#include "pagespeed/system/external_server_spec.h"
#include "net/instaweb/util/public/cache_property_store.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/property_store.h"
#include "pagespeed/kernel/base/abstract_shared_mem.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/md5_hasher.h"
#include "pagespeed/kernel/base/mem_file_system.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/named_lock_manager.h"
#include "pagespeed/kernel/base/named_lock_tester.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/null_shared_mem.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/async_cache.h"
#include "pagespeed/kernel/cache/cache_batcher.h"
#include "pagespeed/kernel/cache/cache_spammer.h"
#include "pagespeed/kernel/cache/cache_stats.h"
#include "pagespeed/kernel/cache/compressed_cache.h"
#include "pagespeed/kernel/cache/fallback_cache.h"
#include "pagespeed/kernel/cache/file_cache.h"
#include "pagespeed/kernel/cache/lru_cache.h"
#include "pagespeed/kernel/cache/threadsafe_cache.h"
#include "pagespeed/kernel/cache/write_through_cache.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/sharedmem/inprocess_shared_mem.h"
#include "pagespeed/kernel/sharedmem/shared_mem_lock_manager.h"
#include "pagespeed/kernel/thread/blocking_callback.h"
#include "pagespeed/kernel/thread/worker_test_base.h"
#include "pagespeed/kernel/util/file_system_lock_manager.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/util/simple_random.h"

namespace net_instaweb {

namespace {

const char kCachePath[] = "/mem/path/";
const char kAltCachePath[] = "/mem/path_alt/";
const char kAltCachePath2[] = "/mem/path_alt2/";
const char kUrl1[] = "http://example.com/a.css";
const char kUrl2[] = "http://example.com/b.css";

class SystemServerContextNoProxyHtml : public SystemServerContext {
 public:
  explicit SystemServerContextNoProxyHtml(RewriteDriverFactory* factory)
      : SystemServerContext(factory, "fake_hostname", 80 /* fake port */) {
  }

  virtual bool ProxiesHtml() const { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemServerContextNoProxyHtml);
};

class SystemCachesTest : public CustomRewriteTestBase<SystemRewriteOptions> {
 protected:
  static const int kThreadLimit = 3;
  static const int kUsableMetadataCacheSize = 8 * 1024;

  // Helper that blocks for async HTTP cache lookups.
  class HTTPBlockingCallback : public HTTPCache::Callback {
   public:
    explicit HTTPBlockingCallback(ThreadSystem* threads)
        : Callback(RequestContext::NewTestRequestContext(threads)),
          sync_(threads) {}

    HTTPCache::FindResult result() const { return result_; }
    GoogleString value() const { return value_; }

    void Block() {
      sync_.Wait();
    }

    // RespectVary not relevant in this context.
    virtual ResponseHeaders::VaryOption RespectVaryOnResources() const {
      return ResponseHeaders::kRespectVaryOnResources;
    }

   protected:
    virtual void Done(HTTPCache::FindResult state) {
      result_ = state;
      if (state.status == HTTPCache::kFound) {
        StringPiece contents;
        http_value()->ExtractContents(&contents);
        contents.CopyToString(&value_);
      }
      sync_.Notify();
    }

    virtual bool IsCacheValid(const GoogleString& key,
                              const ResponseHeaders& headers) {
      return true;
    }

   private:
    WorkerTestBase::SyncPoint sync_;
    HTTPCache::FindResult result_;
    GoogleString value_;
  };

  SystemCachesTest()
      : thread_system_(Platform::CreateThreadSystem()),
        options_(new SystemRewriteOptions(thread_system_.get())),
        purge_done_(false),
        purge_success_(false) {
    shared_mem_.reset(new InProcessSharedMem(thread_system_.get()));
    factory_->set_hasher(new MD5Hasher());
    Statistics* stats = factory()->statistics();
    SystemCaches::InitStats(stats);
    SystemServerContext::InitStats(stats);
    // These are normally done by SystemRewriteDriverFactory.
    CacheStats::InitStats(
        PropertyCache::GetStatsPrefix(RewriteDriver::kBeaconCohort),
        stats);
    CacheStats::InitStats(
        PropertyCache::GetStatsPrefix(RewriteDriver::kDomCohort),
        stats);
    CacheStats::InitStats(
        PropertyCache::GetStatsPrefix(RewriteDriver::kDependenciesCohort),
        stats);
  }

  virtual void SetUp() {
    // TODO(jcrowell) factor out apr_initialize/terminate to share in static
    // constructor similar to rewrite_test_base.cc.
    apr_initialize();
    atexit(apr_terminate);
    SetUpSystemCaches();
    CustomRewriteTestBase<SystemRewriteOptions>::SetUp();
  }

  void SetUpSystemCaches() {
    system_caches_.reset(
        new SystemCaches(factory(), shared_mem_.get(), kThreadLimit));
  }

  void BreakShm() {
    // system_caches_ wants to be shut down in destructor
    system_caches_->StopCacheActivity();
    system_caches_->ShutDown(factory()->message_handler());

    shared_mem_.reset(new NullSharedMem());
    SetUpSystemCaches();
  }

  virtual void TearDown() {
    system_caches_->StopCacheActivity();
    RewriteTestBase::TearDown();
    system_caches_->ShutDown(factory()->message_handler());
  }

  void PrepareWithConfig(SystemRewriteOptions* config) {
    system_caches_->RegisterConfig(config);
    system_caches_->RootInit();
    // pretend we fork here.
    system_caches_->ChildInit();
  }

  // Takes ownership of config.
  SystemServerContext* SetupServerContext(SystemRewriteOptions* config) {
    scoped_ptr<SystemServerContext> server_context(
        new SystemServerContextNoProxyHtml(factory()));
    server_context->reset_global_options(config);
    server_context->set_statistics(factory()->statistics());
    server_context->set_timer(factory()->timer());
    system_caches_->SetupCaches(server_context.get(),
                                true /* enable_property_cache */);

    // Sanity-check that the two caches work.
    // Note: even though we may have AsyncCache under the hood, right now it's
    // possible with external caches only, and both of them are configured with
    // a single thread. So, Get() can only be executed after Put() finishes.
    //
    // TODO(yeputons): it's important that RedisCache is connected at that
    // point. Otherwise it will try to connect in Put() and combination of
    // AsyncCache and fast-fail in "connecting" state will play against us.
    TestPut(server_context->metadata_cache(), "a", "b");
    TestGet(server_context->metadata_cache(), "a",
            CacheInterface::kAvailable, "b");

    TestHttpPut(server_context->http_cache(), "http://www.example.com",
                "fragment", "a");
    TestHttpGet(server_context->http_cache(), "http://www.example.com",
                "fragment", kFoundResult, "a");
    return server_context.release();
  }

  void TestPut(CacheInterface* cache, StringPiece key, StringPiece value) {
    GoogleString value_copy;
    value.CopyToString(&value_copy);
    SharedString shared_value;
    shared_value.SwapWithString(&value_copy);
    cache->Put(key.as_string(), shared_value);
  }

  void TestGet(CacheInterface* cache, StringPiece key,
               CacheInterface::KeyState expected_result,
               StringPiece expected_value) {
    BlockingCallback callback(thread_system_.get());
    cache->Get(key.as_string(), &callback);
    callback.Block();
    EXPECT_EQ(expected_result, callback.result());
    EXPECT_EQ(expected_value, callback.value());
  }

  void TestHttpPut(HTTPCache* cache, StringPiece key,
                   StringPiece fragment, StringPiece value) {
    ResponseHeaders headers;
    SetDefaultLongCacheHeaders(&kContentTypeText, &headers);
    cache->Put(key.as_string(), fragment.as_string(),
               RequestHeaders::Properties(),
               ResponseHeaders::kRespectVaryOnResources, &headers, value,
               factory()->message_handler());
  }

  void TestHttpGet(HTTPCache* cache, StringPiece key, StringPiece fragment,
                   HTTPCache::FindResult expected_state,
                   StringPiece expected_value) {
    HTTPBlockingCallback callback(thread_system_.get());
    cache->Find(key.as_string(), fragment.as_string(),
                factory()->message_handler(), &callback);
    callback.Block();
    EXPECT_EQ(expected_state, callback.result());
    EXPECT_EQ(expected_value, callback.value());
  }

  // Unwraps any wrapper cache objects.
  CacheInterface* SkipWrappers(CacheInterface* in) {
    CacheInterface* backend = in->Backend();
    if (backend != in) {
      return SkipWrappers(backend);
    }
    return in;
  }

  // Wrapper functions to format expected cache descriptor strings with
  // concise function calls exposing the cache structure via normal code
  // indentation.
  GoogleString WriteThrough(StringPiece l1, StringPiece l2) {
    return WriteThroughCache::FormatName(l1, l2);
  }

  GoogleString HttpCache(StringPiece cache) {
    return HTTPCache::FormatName(cache);
  }

  GoogleString Fallback(StringPiece small, StringPiece large) {
    return FallbackCache::FormatName(small, large);
  }

  GoogleString Batcher(StringPiece cache, int parallel, int max) {
    return CacheBatcher::FormatName(cache, parallel, max);
  }

  GoogleString Stats(StringPiece prefix, StringPiece cache) {
    return CacheStats::FormatName(prefix, cache);
  }

  GoogleString ThreadsafeLRU() {
    return ThreadsafeCache::FormatName(LRUCache::FormatName());
  }

  GoogleString FileCacheName() { return FileCache::FormatName(); }


  GoogleString FileCacheWithStats() {
    return Stats("file_cache", FileCacheName());
  }

  GoogleString Pcache(StringPiece cache) {
    return CachePropertyStore::FormatName3(
        RewriteDriver::kBeaconCohort,
        Stats(PropertyCache::GetStatsPrefix(RewriteDriver::kBeaconCohort),
              cache),
        RewriteDriver::kDependenciesCohort,
        Stats(PropertyCache::GetStatsPrefix(RewriteDriver::kDependenciesCohort),
              cache),
        RewriteDriver::kDomCohort,
        Stats(PropertyCache::GetStatsPrefix(RewriteDriver::kDomCohort),
              cache));
  }

  GoogleString Compressed(StringPiece cache) {
    return CompressedCache::FormatName(cache);
  }

  SystemServerContext* PopulateCacheForPurgeTest() {
    options_->set_file_cache_path(kCachePath);
    SystemRewriteOptions* options = options_.get();
    PrepareWithConfig(options);
    system_server_context_.reset(SetupServerContext(options_.release()));
    HTTPCache* http_cache = system_server_context_->http_cache();
    MessageHandler* handler = message_handler();
    system_server_context_->set_message_handler(handler);
    ResponseHeaders headers;
    SetDefaultLongCacheHeaders(&kContentTypeText, &headers);
    headers.ComputeCaching();
    RequestHeaders::Properties req_properties;
    http_cache->Put(kUrl1, rewrite_driver_->CacheFragment(), req_properties,
                    ResponseHeaders::kRespectVaryOnResources,
                    &headers, "a value", handler);
    http_cache->Put(kUrl2, rewrite_driver_->CacheFragment(), req_properties,
                    ResponseHeaders::kRespectVaryOnResources,
                    &headers, "b value", handler);
    AdvanceTimeMs(1000);
    HTTPValue value;

    // As expected, both kUrl1 and kUrl2 are valid after Put.
    EXPECT_EQ(kFoundResult, HttpBlockingFindWithOptions(
        options, kUrl1, http_cache, &value, &headers));
    EXPECT_EQ(kFoundResult, HttpBlockingFindWithOptions(
        options, kUrl2, http_cache, &value, &headers));
    AdvanceTimeMs(1000);
    return system_server_context_.get();
  }

  scoped_ptr<ThreadSystem> thread_system_;
  scoped_ptr<AbstractSharedMem> shared_mem_;
  scoped_ptr<SystemCaches> system_caches_;
  scoped_ptr<SystemRewriteOptions> options_;
  scoped_ptr<SystemServerContext> system_server_context_;
  bool purge_done_;
  bool purge_success_;
};

TEST_F(SystemCachesTest, BasicFileAndLruCache) {
  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(100);
  options_->set_default_shared_memory_cache_kb(0);
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  EXPECT_STREQ(Compressed(WriteThrough(Stats("lru_cache", ThreadsafeLRU()),
                                       FileCacheWithStats())),
               server_context->metadata_cache()->Name());
  EXPECT_STREQ(
      HttpCache(
          WriteThrough(
              Stats("lru_cache", ThreadsafeLRU()),
              FileCacheWithStats())),
      server_context->http_cache()->Name());
  EXPECT_TRUE(server_context->filesystem_metadata_cache() == NULL);
}

TEST_F(SystemCachesTest, BasicFileOnlyCache) {
  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(0);
  options_->set_default_shared_memory_cache_kb(0);
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  EXPECT_STREQ(Compressed(FileCacheWithStats()),
               server_context->metadata_cache()->Name());
  EXPECT_STREQ(HttpCache(FileCacheWithStats()),
               server_context->http_cache()->Name());
  EXPECT_TRUE(server_context->filesystem_metadata_cache() == NULL);
}

TEST_F(SystemCachesTest, UnusableShmAndLru) {
  // Test that we properly fallback when we can't create the shm cache
  // due to too small a size given.
  GoogleString error_msg;
  EXPECT_FALSE(
      system_caches_->CreateShmMetadataCache(kCachePath, 10, &error_msg));
  EXPECT_STREQ("Shared memory cache unusably small.", error_msg);

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(100);
  options_->set_default_shared_memory_cache_kb(0);
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  EXPECT_STREQ(Compressed(WriteThrough(Stats("lru_cache", ThreadsafeLRU()),
                                       FileCacheWithStats())),
               server_context->metadata_cache()->Name());
  EXPECT_STREQ(
      HttpCache(WriteThrough(
          Stats("lru_cache", ThreadsafeLRU()),
          FileCacheWithStats())),
      server_context->http_cache()->Name());
  EXPECT_TRUE(server_context->filesystem_metadata_cache() == NULL);
}

TEST_F(SystemCachesTest, BasicShmAndLru) {
  GoogleString error_msg;
  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kCachePath, kUsableMetadataCacheSize, &error_msg));

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(100);
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  // We don't use the LRU when shm cache is on.
  EXPECT_STREQ(Compressed(Fallback(Stats("shm_cache", "SharedMemCache<64>"),
                                   FileCacheWithStats())),
               server_context->metadata_cache()->Name());
  // HTTP cache is unaffected.
  EXPECT_STREQ(
      HttpCache(WriteThrough(Stats("lru_cache", ThreadsafeLRU()),
                             FileCacheWithStats())),
      server_context->http_cache()->Name());
  EXPECT_TRUE(server_context->filesystem_metadata_cache() == NULL);
}

TEST_F(SystemCachesTest, BasicShmAndNoLru) {
  GoogleString error_msg;
  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kCachePath, kUsableMetadataCacheSize, &error_msg));

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(0);
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  // We don't use the LRU when shm cache is on.
  EXPECT_STREQ(Compressed(Fallback(Stats("shm_cache", "SharedMemCache<64>"),
                                   FileCacheWithStats())),
               server_context->metadata_cache()->Name());
  // HTTP cache is unaffected.
  EXPECT_STREQ(HttpCache(FileCacheWithStats()),
               server_context->http_cache()->Name());
  EXPECT_TRUE(server_context->filesystem_metadata_cache() == NULL);
}

TEST_F(SystemCachesTest, DoubleShmCreate) {
  // Proper error message on two creation attempts for the same name.
  GoogleString error_msg;
  EXPECT_TRUE(
      system_caches_->CreateShmMetadataCache(kCachePath,
                                             kUsableMetadataCacheSize,
                                             &error_msg));
  EXPECT_FALSE(
      system_caches_->CreateShmMetadataCache(kCachePath,
                                             kUsableMetadataCacheSize,
                                             &error_msg));
  EXPECT_STREQ(StrCat("Cache named ", kCachePath, " already exists."),
               error_msg);

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(100);
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  // We don't use the LRU when shm cache is on.
  EXPECT_STREQ(Compressed(Fallback(Stats("shm_cache", "SharedMemCache<64>"),
                                   FileCacheWithStats())),
               server_context->metadata_cache()->Name());
  // HTTP cache is unaffected.
  EXPECT_STREQ(
      HttpCache(WriteThrough(
          Stats("lru_cache", ThreadsafeLRU()),
          FileCacheWithStats())),
      server_context->http_cache()->Name());
  EXPECT_TRUE(server_context->filesystem_metadata_cache() == NULL);
}

// This class is not template for several reasons:
// 1. There is currently no need in having different types depending on what
//    external cache are we testing.
// 2. Templates add complexity and boilerplate when defining functions outside
//    of the class.
// 3. When you have a class derived from template class, you cannot access
//    member functions and fields without explicitly writing `this->` or
//    something similar. It's explained in Google Test documentation and here:
//    http://stackoverflow.com/questions/1120833/derived-template-class-access-to-base-class-member-data
class SystemCachesExternalCacheTestBase : public SystemCachesTest {
 protected:
  virtual bool SkipExternalCacheTests() = 0;

  virtual GoogleString AssembledAsyncCacheWithStats() = 0;

  virtual GoogleString AssembledBlockingCacheWithStats() = 0;

  virtual void SetUpExternalCache(SystemRewriteOptions* options) = 0;

  // Test helpers
  void TestBasicCacheAndLru();

  void TestBasicCacheLruShm();

  void TestBasicCacheShmNoLru();

  void StressTestHelper(bool do_deletes) {
    if (SkipExternalCacheTests()) {
      return;
    }

    options_->set_file_cache_path(kCachePath);
    options_->set_use_shared_mem_locking(false);
    options_->set_lru_cache_kb_per_process(0);
    options_->set_default_shared_memory_cache_kb(0);
    options_->set_compress_metadata_cache(false);
    SetUpExternalCache(options_.get());
    PrepareWithConfig(options_.get());
    scoped_ptr<ServerContext> server_context(
        SetupServerContext(options_.release()));
    CacheInterface* cache = server_context->metadata_cache();
    SimpleRandom random(new NullMutex);
    GoogleString value = random.GenerateHighEntropyString(20000);
    CacheSpammer::RunTests(4 /* num_threads */,
                           200 /* iters */,
                           200 /* inserts */,
                           false /* expecting_evictions */,
                           do_deletes,
                           value.c_str(),
                           cache,
                           thread_system_.get());
  }

  void TestCacheShare();

  void TestStatsStringMinimal();

  void TestBrokenShmFallbackCacheLruShm();

  void TestBrokenShmFallbackCacheShmNoLru();
};

#define ADD_EXTERNAL_CACHE_TESTS(CacheTestClass)                              \
  TEST_F(CacheTestClass, BasicCacheAndLru) { TestBasicCacheAndLru(); }        \
  TEST_F(CacheTestClass, BasicCacheLruShm) { TestBasicCacheLruShm(); }        \
  TEST_F(CacheTestClass, BasicCacheShmNoLru) { TestBasicCacheShmNoLru(); }    \
  TEST_F(CacheTestClass, CacheShare) { TestCacheShare(); }                    \
  TEST_F(CacheTestClass, StatsStringMinimal) { TestStatsStringMinimal(); }    \
  TEST_F(CacheTestClass, StressTest) { StressTestHelper(false); }             \
  TEST_F(CacheTestClass, StressTestWithDeletions) { StressTestHelper(true); } \
  TEST_F(CacheTestClass, BrokenShmFallbackCacheLruShm) {                      \
    TestBrokenShmFallbackCacheLruShm();                                       \
  }                                                                           \
  TEST_F(CacheTestClass, BrokenShmFallbackCacheShmNoLru) {                    \
    TestBrokenShmFallbackCacheShmNoLru();                                     \
  }

class SystemCachesMemCacheTest : public SystemCachesExternalCacheTestBase {
 protected:
  bool SkipExternalCacheTests() override {
    return ServerSpec().empty();
  }

  ExternalClusterSpec ServerSpec() {
    if (cluster_spec_.empty()) {
      // This matches the logic in apr_mem_cache_test.
      const char* port_string = getenv("MEMCACHED_PORT");
      int port;
      if (port_string == nullptr || !StringToInt(port_string, &port)) {
        LOG(ERROR) << "SystemCachesMemCacheTest is skipped because env var "
                   << "$MEMCACHED_PORT is not set to a valid integer. Set that "
                   << "to the port number where memcached is running to enable "
                   << "the tests.  See install/run_program_with_memcached.sh";
        return cluster_spec_;
      }
      cluster_spec_.servers.push_back(ExternalServerSpec("localhost", port));
    }
    return cluster_spec_;
  }

  GoogleString AssembledAsyncCacheWithStats() override {
    return Fallback(
        Batcher(Stats(SystemCaches::kMemcachedAsync,
                      AsyncCache::FormatName(AprMemCache::FormatName())),
                1, 1000),
        FileCacheWithStats());
  }

  GoogleString AssembledBlockingCacheWithStats() override {
    return Fallback(
        Stats(SystemCaches::kMemcachedBlocking, AprMemCache::FormatName()),
        FileCacheWithStats());
  }

  void SetUpExternalCache(SystemRewriteOptions* options) override {
    options->set_memcached_servers(ServerSpec());
  }

  void TestBasicMemCacheAndNoLru(int num_threads_specified,
                                 int num_threads_expected);

 private:
  ExternalClusterSpec cluster_spec_;
};

ADD_EXTERNAL_CACHE_TESTS(SystemCachesMemCacheTest)

void SystemCachesExternalCacheTestBase::TestBasicCacheAndLru() {
  if (SkipExternalCacheTests()) {
    return;
  }

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(100);
  options_->set_default_shared_memory_cache_kb(0);
  SetUpExternalCache(options_.get());
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  EXPECT_STREQ(Compressed(WriteThrough(Stats("lru_cache", ThreadsafeLRU()),
                                       AssembledAsyncCacheWithStats())),
               server_context->metadata_cache()->Name());
  EXPECT_STREQ(
      HttpCache(WriteThrough(
          Stats("lru_cache", ThreadsafeLRU()),
          AssembledAsyncCacheWithStats())),
      server_context->http_cache()->Name());
  ASSERT_TRUE(server_context->filesystem_metadata_cache() != NULL);
  EXPECT_TRUE(server_context->filesystem_metadata_cache()->IsBlocking());
  EXPECT_STREQ(
      AssembledBlockingCacheWithStats(),
      server_context->filesystem_metadata_cache()->Name());
}

void SystemCachesExternalCacheTestBase::TestBasicCacheLruShm() {
  if (SkipExternalCacheTests()) {
    return;
  }

  GoogleString error_msg;
  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kCachePath, kUsableMetadataCacheSize, &error_msg));

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(100);
  SetUpExternalCache(options_.get());
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  // For metadata, we fallback to external cache behind shmcache.
  EXPECT_STREQ(
      Compressed(WriteThrough(
          Stats("shm_cache", SharedMemCache<64>::FormatName()),
          AssembledAsyncCacheWithStats())),
      server_context->metadata_cache()->Name());
  EXPECT_STREQ(
      HttpCache(WriteThrough(
          Stats("lru_cache", ThreadsafeLRU()),
          AssembledAsyncCacheWithStats())),
      server_context->http_cache()->Name());
}

void SystemCachesExternalCacheTestBase::TestBasicCacheShmNoLru() {
  if (SkipExternalCacheTests()) {
    return;
  }

  GoogleString error_msg;
  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kCachePath, kUsableMetadataCacheSize, &error_msg));

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(0);
  SetUpExternalCache(options_.get());
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  EXPECT_STREQ(
      Compressed(
          WriteThrough(
              Stats("shm_cache", "SharedMemCache<64>"),
              AssembledAsyncCacheWithStats())),
      server_context->metadata_cache()->Name());
  EXPECT_STREQ(
      HttpCache(AssembledAsyncCacheWithStats()),
      server_context->http_cache()->Name());
  ASSERT_TRUE(server_context->filesystem_metadata_cache() != NULL);
  EXPECT_TRUE(server_context->filesystem_metadata_cache()->IsBlocking());
  EXPECT_STREQ(
      Stats("shm_cache", "SharedMemCache<64>"),
      server_context->filesystem_metadata_cache()->Name());
}

void SystemCachesMemCacheTest::TestBasicMemCacheAndNoLru(
    int num_threads_specified, int num_threads_expected) {
  if (ServerSpec().empty()) {
    return;
  }

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(0);
  options_->set_memcached_servers(ServerSpec());
  options_->set_memcached_threads(num_threads_specified);
  options_->set_default_shared_memory_cache_kb(0);
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));

  GoogleString mem_cache;
  if (num_threads_expected == 0) {
    mem_cache = Batcher(Stats(SystemCaches::kMemcachedAsync,
                              AprMemCache::FormatName()),
                        1, 1000);
  } else {
    mem_cache =
        Batcher(Stats(SystemCaches::kMemcachedAsync,
                      AsyncCache::FormatName(AprMemCache::FormatName())),
                num_threads_expected, 1000);
  }

  EXPECT_STREQ(
      Compressed(Fallback(mem_cache, Stats("file_cache", FileCacheName()))),
      server_context->metadata_cache()->Name());
  EXPECT_STREQ(
      HttpCache(
          Fallback(mem_cache, Stats("file_cache", FileCacheName()))),
      server_context->http_cache()->Name());
  ASSERT_TRUE(server_context->filesystem_metadata_cache() != NULL);

  // That the code that queries the FSMDC from the validator in RewriteContext
  // does a Get and needs the response to be available inline.
  EXPECT_TRUE(server_context->filesystem_metadata_cache()->IsBlocking());
  EXPECT_STREQ(AssembledBlockingCacheWithStats(),
               server_context->filesystem_metadata_cache()->Name());
}

TEST_F(SystemCachesMemCacheTest, BasicMemCachedAndNoLru_0_Threads) {
  TestBasicMemCacheAndNoLru(0, 0);
}

TEST_F(SystemCachesMemCacheTest, BasicMemCachedAndNoLru_1_Thread) {
  TestBasicMemCacheAndNoLru(1, 1);
}

TEST_F(SystemCachesMemCacheTest, BasicMemCachedAndNoLru_2_Threads) {
  TestBasicMemCacheAndNoLru(2, 1);  // Clamp to 1.
}

class SystemCachesRedisCacheTest : public SystemCachesExternalCacheTestBase {
 protected:
  // TODO(yeputons): share this code with SystemCachesMemCacheTest or move it to
  // the base class.
  bool SkipExternalCacheTests() override {
    return ServerSpec().empty();
  }

  // TODO(yeputons): share this code with SystemCachesMemCacheTest or move it to
  // the base class.
  ExternalServerSpec ServerSpec() {
    if (server_spec_.empty()) {
      // This matches the logic in apr_mem_cache_test.
      const char* port_string = getenv("REDIS_PORT");
      int port;
      if (port_string == NULL || !StringToInt(port_string, &port)) {
        LOG(ERROR) << "SystemCachesRedisCacheTest is skipped because env var "
                   << "$REDIS_PORT is not set to a valid integer. Set that to "
                   << "the port number where redis is running to enable the "
                   << "tests.  See install/run_program_with_redis.sh";
        return ExternalServerSpec();
      }
      server_spec_.host = "localhost";
      server_spec_.port = port;
    }
    return server_spec_;
  }

  GoogleString AssembledAsyncCacheWithStats() override {
    return Batcher(Stats(SystemCaches::kRedisAsync,
                         AsyncCache::FormatName(RedisCache::FormatName())),
                   1, 1000);
  }

  GoogleString AssembledBlockingCacheWithStats() override {
    return Stats(SystemCaches::kRedisBlocking, RedisCache::FormatName());
  }

  void SetUpExternalCache(SystemRewriteOptions* options) override {
    options->set_redis_server(ServerSpec());
  }

 private:
  ExternalServerSpec server_spec_;
};

ADD_EXTERNAL_CACHE_TESTS(SystemCachesRedisCacheTest)

TEST_F(SystemCachesTest, BasicFileLockManager) {
  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(100);
  PrepareWithConfig(options_.get());
  NamedLockManager* named_locks = system_caches_->GetLockManager(
      options_.get());
  EXPECT_TRUE(named_locks != NULL);
  EXPECT_TRUE(dynamic_cast<FileSystemLockManager*>(named_locks) != NULL);
}

TEST_F(SystemCachesTest, BasicShmLockManager) {
  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(true);
  options_->set_lru_cache_kb_per_process(100);
  PrepareWithConfig(options_.get());
  NamedLockManager* named_locks = system_caches_->GetLockManager(
      options_.get());
  EXPECT_TRUE(named_locks != NULL);
  EXPECT_TRUE(dynamic_cast<SharedMemLockManager*>(named_locks) != NULL);
}

TEST_F(SystemCachesTest, FileShare) {
  // [0], [1], share path, [2] doesn't.
  std::vector<SystemRewriteOptions*> configs;
  for (int i = 0; i < 3; ++i) {
    SystemRewriteOptions* config = options_->NewOptions();
    config->set_file_cache_path((i == 2) ? kCachePath : kAltCachePath);
    config->set_default_shared_memory_cache_kb(0);
    system_caches_->RegisterConfig(config);
    configs.push_back(config);
  }
  system_caches_->RootInit();
  // pretend we fork here.
  system_caches_->ChildInit();

  std::vector<ServerContext*> servers;
  for (int i = 0; i < 3; ++i) {
    servers.push_back(SetupServerContext(configs[i]));
  }

  TestPut(servers[0]->metadata_cache(), "b", "value");
  TestGet(servers[0]->metadata_cache(), "b",
          CacheInterface::kAvailable, "value");
  TestGet(servers[1]->metadata_cache(), "b",
          CacheInterface::kAvailable, "value");
  TestGet(servers[2]->metadata_cache(), "b", CacheInterface::kNotFound, "");

  TestHttpPut(servers[0]->http_cache(), "http://b.org", "fragment", "value");
  TestHttpGet(servers[0]->http_cache(), "http://b.org", "fragment",
              kFoundResult, "value");
  TestHttpGet(servers[1]->http_cache(), "http://b.org", "fragment",
              kFoundResult, "value");
  TestHttpGet(servers[2]->http_cache(), "http://b.org", "fragment",
              kNotFoundResult, "");

  // Lock managers have similar sharing semantics
  scoped_ptr<NamedLock> lock0(
      system_caches_->GetLockManager(configs[0])->CreateNamedLock("a"));
  scoped_ptr<NamedLock> lock1(
      system_caches_->GetLockManager(configs[1])->CreateNamedLock("a"));
  scoped_ptr<NamedLock> lock2(
      system_caches_->GetLockManager(configs[2])->CreateNamedLock("a"));
  NamedLockTester tester(thread_system_.get());
  EXPECT_TRUE(tester.TryLock(lock0.get()));
  EXPECT_FALSE(tester.TryLock(lock1.get()));
  EXPECT_TRUE(tester.TryLock(lock2.get()));
  lock0->Unlock();
  EXPECT_TRUE(tester.TryLock(lock1.get()));

  STLDeleteElements(&servers);
}

TEST_F(SystemCachesTest, ShmShare) {
  // For SHM metadata cache, sharing is based on explicit segment names/
  // [0], [1], share, [2] doesn't.
  GoogleString error_msg;
  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kCachePath, kUsableMetadataCacheSize, &error_msg));

  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kAltCachePath, kUsableMetadataCacheSize, &error_msg));

  std::vector<SystemRewriteOptions*> configs;
  for (int i = 0; i < 3; ++i) {
    SystemRewriteOptions* config = options_->NewOptions();
    config->set_file_cache_path((i == 2) ? kAltCachePath : kCachePath);
    system_caches_->RegisterConfig(config);
    configs.push_back(config);
  }

  system_caches_->RootInit();
  // pretend we fork here.
  system_caches_->ChildInit();

  std::vector<ServerContext*> servers;
  for (int i = 0; i < 3; ++i) {
    servers.push_back(SetupServerContext(configs[i]));
    EXPECT_STREQ(Compressed(Fallback(Stats("shm_cache", "SharedMemCache<64>"),
                                     FileCacheWithStats())),
                 servers[i]->metadata_cache()->Name());
  }

  // This is only about metadata cache.
  TestPut(servers[0]->metadata_cache(), "b", "value");
  TestGet(servers[0]->metadata_cache(), "b",
          CacheInterface::kAvailable, "value");
  TestGet(servers[1]->metadata_cache(), "b",
          CacheInterface::kAvailable, "value");
  TestGet(servers[2]->metadata_cache(), "b", CacheInterface::kNotFound, "");

  STLDeleteElements(&servers);
}

TEST_F(SystemCachesTest, ShmDefault) {
  // Unless a cache is explicitly defined or the default is disabled with
  // set_default_shared_memory_cache_kb(0), use the default.  Shared memory
  // caches only write through to memcache, which we're not using here.
  //
  // [0] and [1] share the default, [2] has one separately configured.  All
  // three have different file cache paths.
  GoogleString error_msg;
  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kAltCachePath2, kUsableMetadataCacheSize, &error_msg));

  std::vector<SystemRewriteOptions*> configs;
  const char* kPaths[] = {kCachePath, kAltCachePath, kAltCachePath2};
  for (int i = 0; i < 3; ++i) {
    SystemRewriteOptions* config = options_->NewOptions();
    config->set_file_cache_path(kPaths[i]);
    system_caches_->RegisterConfig(config);
    configs.push_back(config);
  }

  // No shm metadata cache was created for [0]'s kCachePath or [1]'s
  // kAltCachePath, only [2]'s kAltCachePath2.  So [0] and [1] will share the
  // default.

  system_caches_->RootInit();
  // pretend we fork here.
  system_caches_->ChildInit();

  std::vector<ServerContext*> servers;
  for (int i = 0; i < 3; ++i) {
    servers.push_back(SetupServerContext(configs[i]));
  }
  EXPECT_STREQ(Compressed(Fallback(Stats("shm_cache", "SharedMemCache<64>"),
                                   FileCacheWithStats())),
               servers[0]->metadata_cache()->Name());
  EXPECT_STREQ(Compressed(Fallback(Stats("shm_cache", "SharedMemCache<64>"),
                                   FileCacheWithStats())),
               servers[1]->metadata_cache()->Name());
  EXPECT_STREQ(Compressed(Fallback(Stats("shm_cache", "SharedMemCache<64>"),
                                   FileCacheWithStats())),
               servers[2]->metadata_cache()->Name());


  // This is only about metadata cache.
  TestPut(servers[0]->metadata_cache(), "b", "value");
  TestGet(servers[0]->metadata_cache(), "b",
          CacheInterface::kAvailable, "value");
  TestGet(servers[1]->metadata_cache(), "b",
          CacheInterface::kAvailable, "value");
  TestGet(servers[2]->metadata_cache(), "b", CacheInterface::kNotFound, "");

  STLDeleteElements(&servers);
}

void SystemCachesExternalCacheTestBase::TestCacheShare() {
  if (SkipExternalCacheTests()) {
    return;
  }

  // Just share 3 clients for the same server (so we don't
  // need 2 servers for the test)

  std::vector<SystemRewriteOptions*> configs;
  for (int i = 0; i < 3; ++i) {
    SystemRewriteOptions* config = options_->NewOptions();
    config->set_file_cache_path(kCachePath);
    config->set_default_shared_memory_cache_kb(0);
    SetUpExternalCache(config);
    system_caches_->RegisterConfig(config);
    configs.push_back(config);
  }

  system_caches_->RootInit();
  // pretend we fork here.
  system_caches_->ChildInit();

  std::vector<ServerContext*> servers;
  for (int i = 0; i < 3; ++i) {
    servers.push_back(SetupServerContext(configs[i]));
    EXPECT_STREQ(
        Compressed(AssembledAsyncCacheWithStats()),
        servers[i]->metadata_cache()->Name());

    EXPECT_STREQ(Pcache(Compressed(AssembledBlockingCacheWithStats())),
                 servers[i]->page_property_cache()->property_store()->Name());
  }

  // Metadata + HTTP cache will end up shared
  TestPut(servers[0]->metadata_cache(), "b", "value");
  TestGet(servers[0]->metadata_cache(), "b",
          CacheInterface::kAvailable, "value");
  TestGet(servers[1]->metadata_cache(), "b",
          CacheInterface::kAvailable, "value");
  TestGet(servers[2]->metadata_cache(), "b",
          CacheInterface::kAvailable, "value");

  TestHttpPut(servers[0]->http_cache(), "http://b.org", "fragment", "value");
  TestHttpGet(servers[0]->http_cache(), "http://b.org", "fragment",
              kFoundResult, "value");
  TestHttpGet(servers[1]->http_cache(), "http://b.org", "fragment",
              kFoundResult, "value");
  TestHttpGet(servers[2]->http_cache(), "http://b.org", "fragment",
              kFoundResult, "value");

  STLDeleteElements(&servers);
}

TEST_F(SystemCachesTest, FileCacheSettings) {
  // Make sure we apply the various file cache settings right.
  options_->set_file_cache_path(kCachePath);
  options_->set_file_cache_clean_interval_ms(3 * Timer::kHourMs);
  options_->set_file_cache_clean_size_kb(1024);
  options_->set_file_cache_clean_inode_limit(50000);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(0);
  options_->set_default_shared_memory_cache_kb(0);
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  EXPECT_STREQ(Compressed(FileCacheWithStats()),
               server_context->metadata_cache()->Name());
  EXPECT_STREQ(HttpCache(FileCacheWithStats()),
               server_context->http_cache()->Name());
  EXPECT_STREQ(Pcache(Compressed(FileCacheWithStats())),
               server_context->page_property_cache()->property_store()->Name());

  FileCache* file_cache = dynamic_cast<FileCache*>(
      SkipWrappers(server_context->metadata_cache()));
  ASSERT_TRUE(file_cache != NULL);
  EXPECT_EQ(kCachePath, file_cache->path());
  EXPECT_EQ(3 * Timer::kHourMs, file_cache->cache_policy()->clean_interval_ms);
  // Note: this is in bytes, the setting is in kb.
  EXPECT_EQ(1024*1024, file_cache->cache_policy()->target_size_bytes);
  EXPECT_EQ(50000, file_cache->cache_policy()->target_inode_count);
  EXPECT_TRUE(file_cache->worker() != NULL);
}

TEST_F(SystemCachesTest, LruCacheSettings) {
  // Test that we apply LRU cache settings right.
  options_->set_file_cache_path(kCachePath);
  options_->set_lru_cache_kb_per_process(1024);
  options_->set_lru_cache_byte_limit(500);
  options_->set_default_shared_memory_cache_kb(0);
  PrepareWithConfig(options_.get());
  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));

  WriteThroughCache* write_through = dynamic_cast<WriteThroughCache*>(
      SkipWrappers(server_context->metadata_cache()));
  ASSERT_TRUE(write_through != NULL);
  EXPECT_EQ(500, write_through->cache1_limit());

  LRUCache* lru_cache = dynamic_cast<LRUCache*>(
      SkipWrappers(write_through->cache1()));
  ASSERT_TRUE(lru_cache != NULL);
  EXPECT_EQ(1024*1024, lru_cache->max_bytes_in_cache());

  // Also on the HTTP cache
  WriteThroughCache* http_write_through =
      dynamic_cast<WriteThroughCache*>(server_context->http_cache()->cache());
  ASSERT_TRUE(http_write_through != NULL);
  EXPECT_EQ(500, http_write_through->cache1_limit());
}

void SystemCachesExternalCacheTestBase::TestStatsStringMinimal() {
  if (SkipExternalCacheTests()) {
    return;
  }

  // The format is rather dependent on the implementation so we don't check
  // it, but we do care that it at least doesn't crash.
  GoogleString out;

  GoogleString error_msg;
  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kCachePath, kUsableMetadataCacheSize, &error_msg));

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(0);
  SetUpExternalCache(options_.get());
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));

  system_caches_->PrintCacheStats(
      static_cast<SystemCaches::StatFlags>(SystemCaches::kGlobalView |
                                           SystemCaches::kIncludeMemcached |
                                           SystemCaches::kIncludeRedis),
      &out);
}

TEST_F(SystemCachesTest, ShareIdenticalNoPurge) {
  options_->set_file_cache_path(kCachePath);
  SystemCachePath* path1 = system_caches_->GetCache(options_.get());
  SystemRewriteOptions options2(thread_system_.get());
  options2.set_file_cache_path(kCachePath);
  SystemCachePath* path2 = system_caches_->GetCache(&options2);
  EXPECT_EQ(path1, path2);
}

TEST_F(SystemCachesTest, ShareIdenticalPurge) {
  options_->set_file_cache_path(kCachePath);
  options_->set_enable_cache_purge(true);
  SystemCachePath* path1 = system_caches_->GetCache(options_.get());
  SystemRewriteOptions options2(thread_system_.get());
  options2.set_file_cache_path(kCachePath);
  options2.set_enable_cache_purge(true);
  SystemCachePath* path2 = system_caches_->GetCache(&options2);
  EXPECT_EQ(path1, path2);
}

TEST_F(SystemCachesTest, NoSharePurgeFlush) {
  options_->set_file_cache_path(kCachePath);
  options_->set_enable_cache_purge(true);
  SystemCachePath* path1 = system_caches_->GetCache(options_.get());
  SystemRewriteOptions options2(thread_system_.get());
  options2.set_file_cache_path(kCachePath);
  SystemCachePath* path2 = system_caches_->GetCache(&options2);
  EXPECT_NE(path1, path2);
}

TEST_F(SystemCachesTest, ShareIdenticalPurgeCustomPath) {
  options_->set_file_cache_path(kCachePath);
  options_->set_cache_flush_filename("f1");
  SystemCachePath* path1 = system_caches_->GetCache(options_.get());
  SystemRewriteOptions options2(thread_system_.get());
  options2.set_file_cache_path(kCachePath);
  options2.set_cache_flush_filename("f1");
  SystemCachePath* path2 = system_caches_->GetCache(&options2);
  EXPECT_EQ(path1, path2);
}

TEST_F(SystemCachesTest, NoShareVaryingPurgeCustomPath) {
  options_->set_file_cache_path(kCachePath);
  options_->set_cache_flush_filename("f1");
  SystemCachePath* path1 = system_caches_->GetCache(options_.get());
  SystemRewriteOptions options2(thread_system_.get());
  options2.set_file_cache_path(kCachePath);
  options2.set_cache_flush_filename("f2");
  SystemCachePath* path2 = system_caches_->GetCache(&options2);
  EXPECT_NE(path1, path2);
}

TEST_F(SystemCachesTest, ShareOnOff) {
  options_->set_file_cache_path(kCachePath);
  SystemCachePath* path1 = system_caches_->GetCache(options_.get());
  SystemRewriteOptions options2(thread_system_.get());
  options2.set_file_cache_path(kCachePath);
  options2.set_enabled(RewriteOptions::kEnabledOff);
  SystemCachePath* path2 = system_caches_->GetCache(&options2);
  EXPECT_EQ(path1, path2);
}

TEST_F(SystemCachesTest, ShareOnStandby) {
  options_->set_file_cache_path(kCachePath);
  SystemCachePath* path1 = system_caches_->GetCache(options_.get());
  SystemRewriteOptions options2(thread_system_.get());
  options2.set_file_cache_path(kCachePath);
  options2.set_enabled(RewriteOptions::kEnabledStandby);
  SystemCachePath* path2 = system_caches_->GetCache(&options2);
  EXPECT_EQ(path1, path2);
}

TEST_F(SystemCachesTest, NoShareOnUnplugged) {
  options_->set_file_cache_path("/a");
  options_->set_enabled(RewriteOptions::kEnabledUnplugged);
  SystemCachePath* path1 = system_caches_->GetCache(options_.get());
  SystemRewriteOptions options2(thread_system_.get());
  options2.set_file_cache_path("/b");
  options2.set_enabled(RewriteOptions::kEnabledUnplugged);
  options2.set_cache_flush_filename("f2");
  SystemCachePath* path2 = system_caches_->GetCache(&options2);
  EXPECT_EQ(path1, path2);  // All 'unplugged' caches are the same.
}

TEST_F(SystemCachesTest, ShareUnpluggedWithOtherMismatches) {
  options_->set_file_cache_path(kCachePath);
  SystemCachePath* path1 = system_caches_->GetCache(options_.get());
  SystemRewriteOptions options2(thread_system_.get());
  options2.set_file_cache_path(kCachePath);
  options2.set_enabled(RewriteOptions::kEnabledUnplugged);
  SystemCachePath* path2 = system_caches_->GetCache(&options2);
  EXPECT_NE(path1, path2);
}

TEST_F(SystemCachesTest, FileCacheNoConflictTwoPaths) {
  options_->set_file_cache_path(kCachePath);
  SystemCachePath* path1 = system_caches_->GetCache(options_.get());
  SystemRewriteOptions options2(thread_system_.get());
  SystemCachePath* path2 = system_caches_->GetCache(&options2);
  EXPECT_NE(path1, path2);
  EXPECT_EQ(0, message_handler()->MessagesOfType(kWarning));
}

TEST_F(SystemCachesTest, FileCacheFullConflictTwoPaths) {
  options_->set_file_cache_path(kCachePath);
  options_->set_file_cache_clean_size_kb(10);
  options_->set_file_cache_clean_inode_limit(20);
  options_->set_file_cache_clean_interval_ms(1000);
  SystemCachePath* path1 = system_caches_->GetCache(options_.get());
  SystemRewriteOptions options2(thread_system_.get());
  options2.set_file_cache_path(kCachePath);
  options2.set_file_cache_clean_size_kb(11);        // wins
  options2.set_file_cache_clean_inode_limit(19);    // loses
  options2.set_file_cache_clean_interval_ms(999);   // wins
  SystemCachePath* path2 = system_caches_->GetCache(&options2);
  ASSERT_EQ(path1, path2);
  FileCache* file_cache = path1->file_cache_backend();
  const FileCache::CachePolicy* policy = file_cache->cache_policy();
  EXPECT_EQ(11*1024, policy->target_size_bytes);
  EXPECT_EQ(20, policy->target_inode_count);
  EXPECT_EQ(999, policy->clean_interval_ms);
  EXPECT_EQ(3, message_handler()->MessagesOfType(kWarning));
}

TEST_F(SystemCachesTest, FileCacheNoConflictOnDefaults) {
  options_->set_file_cache_path(kCachePath);
  options_->set_file_cache_clean_inode_limit(20);
  options_->set_file_cache_clean_interval_ms(1000);
  SystemCachePath* path1 = system_caches_->GetCache(options_.get());
  SystemRewriteOptions options2(thread_system_.get());
  options2.set_file_cache_path(kCachePath);
  options2.set_file_cache_clean_size_kb(11);        // wins
  SystemCachePath* path2 = system_caches_->GetCache(&options2);
  ASSERT_EQ(path1, path2);
  FileCache* file_cache = path1->file_cache_backend();
  const FileCache::CachePolicy* policy = file_cache->cache_policy();
  EXPECT_EQ(11*1024, policy->target_size_bytes);
  EXPECT_EQ(20, policy->target_inode_count);
  EXPECT_EQ(1000, policy->clean_interval_ms);
  EXPECT_EQ(0, message_handler()->MessagesOfType(kWarning));
}

TEST_F(SystemCachesTest, PurgeUrl) {
  options_->set_enable_cache_purge(true);
  SystemServerContext* server_context = PopulateCacheForPurgeTest();
  server_context->PostInitHook();
  SystemRewriteOptions* options =
      server_context->global_system_rewrite_options();
  RequestContextPtr request_context(
      RequestContext::NewTestRequestContext(thread_system_.get()));
  StringAsyncFetch fetch(request_context);

  // Invalidate kUrl1 but leave kUrl2 intact.
  AdminSite* admin_site = server_context->admin_site();
  admin_site->PurgeHandler(kUrl1, server_context->cache_path(), &fetch);
  ASSERT_TRUE(fetch.done());
  ASSERT_TRUE(fetch.success());
  server_context->FlushCacheIfNecessary();

  // Make sure we can no longer fetch kUrl1, but we can still fetch kUrl2.
  ResponseHeaders headers;
  HTTPValue value;
  EXPECT_EQ(kNotFoundResult, HttpBlockingFindWithOptions(
      options, kUrl1, server_context->http_cache(), &value, &headers));
  EXPECT_EQ(kFoundResult, HttpBlockingFindWithOptions(
      options, kUrl2, server_context->http_cache(), &value, &headers));

  // Now set the global invalidation timestamp, and kUrl2 will now be invalid
  // as well.
  AdvanceTimeMs(1);
  fetch.Reset();
  admin_site->PurgeHandler("http://example.com/*", server_context->cache_path(),
                           &fetch);
  server_context->FlushCacheIfNecessary();
  AdvanceTimeMs(1);
  EXPECT_EQ(kNotFoundResult, HttpBlockingFindWithOptions(
      options, kUrl2, server_context->http_cache(), &value, &headers));
}

TEST_F(SystemCachesTest, InvalidateWithPurgeDisabled) {
  options_->set_enable_cache_purge(false);
  SystemServerContext* server_context = PopulateCacheForPurgeTest();
  SystemRewriteOptions* options =
      server_context->global_system_rewrite_options();

  // touch cache.flush
  file_system()->WriteFile(StrCat(kCachePath, "/cache.flush").c_str(),
                           "", message_handler());
  AdvanceTimeMs(1000);
  server_context->FlushCacheIfNecessary();

  // Make sure both kUrl1 and kUrl2 are invalidated from touching the file.
  ResponseHeaders headers;
  HTTPValue value;
  EXPECT_EQ(kNotFoundResult, HttpBlockingFindWithOptions(
      options, kUrl1, server_context->http_cache(), &value, &headers));
  EXPECT_EQ(kNotFoundResult, HttpBlockingFindWithOptions(
      options, kUrl2, server_context->http_cache(), &value, &headers));
}

TEST_F(SystemCachesTest, BrokenShmFallbackShmLockManager) {
  BreakShm();
  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(true);
  options_->set_lru_cache_kb_per_process(100);
  PrepareWithConfig(options_.get());
  NamedLockManager* named_locks = system_caches_->GetLockManager(
      options_.get());
  EXPECT_TRUE(named_locks != NULL);

  // Actually file system based here, due to fallback.
  EXPECT_TRUE(dynamic_cast<FileSystemLockManager*>(named_locks) != NULL);
}

TEST_F(SystemCachesTest, BrokenShmFallbackShmAndLru) {
  BreakShm();
  GoogleString error_msg;
  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kCachePath, kUsableMetadataCacheSize, &error_msg));

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(100);
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  // We don't use the LRU when shm cache is on.
  EXPECT_STREQ(Compressed(WriteThrough(Stats("lru_cache", ThreadsafeLRU()),
                                       FileCacheWithStats())),
               server_context->metadata_cache()->Name());
  // HTTP cache is unaffected.
  EXPECT_STREQ(
      HttpCache(WriteThrough(
          Stats("lru_cache", ThreadsafeLRU()),
          FileCacheWithStats())),
      server_context->http_cache()->Name());
}

TEST_F(SystemCachesTest, BrokenShmFallbackShmAndNoLru) {
  BreakShm();
  GoogleString error_msg;
  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kCachePath, kUsableMetadataCacheSize, &error_msg));

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(0);
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  // We don't use the LRU when shm cache is on.
  EXPECT_STREQ(Compressed(FileCacheWithStats()),
               server_context->metadata_cache()->Name());
  // HTTP cache is unaffected.
  EXPECT_STREQ(HttpCache(FileCacheWithStats()),
               server_context->http_cache()->Name());
}

void SystemCachesExternalCacheTestBase::TestBrokenShmFallbackCacheLruShm() {
  if (SkipExternalCacheTests()) {
    return;
  }

  BreakShm();
  GoogleString error_msg;
  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kCachePath, kUsableMetadataCacheSize, &error_msg));

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(100);
  SetUpExternalCache(options_.get());
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  // For metadata, we fallback to external cache behind shmcache.
  EXPECT_STREQ(
      Compressed(
          WriteThrough(
              Stats("lru_cache", ThreadsafeLRU()),
              AssembledAsyncCacheWithStats())),
      server_context->metadata_cache()->Name());
  EXPECT_STREQ(
      HttpCache(WriteThrough(
          Stats("lru_cache", ThreadsafeLRU()),
          AssembledAsyncCacheWithStats())),
      server_context->http_cache()->Name());
  EXPECT_STREQ(Pcache(Compressed(AssembledBlockingCacheWithStats())),
               server_context->page_property_cache()->property_store()->Name());
}

void SystemCachesExternalCacheTestBase::TestBrokenShmFallbackCacheShmNoLru() {
  if (SkipExternalCacheTests()) {
    return;
  }

  BreakShm();
  GoogleString error_msg;
  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kCachePath, kUsableMetadataCacheSize, &error_msg));

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(0);
  SetUpExternalCache(options_.get());
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  EXPECT_STREQ(
      Compressed(AssembledAsyncCacheWithStats()),
      server_context->metadata_cache()->Name());
  EXPECT_STREQ(
      HttpCache(AssembledAsyncCacheWithStats()),
      server_context->http_cache()->Name());
}

}  // namespace

}  // namespace net_instaweb
