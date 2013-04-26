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

#include "net/instaweb/system/public/system_caches.h"

#include <cstdlib>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/write_through_http_cache.h"
#include "net/instaweb/rewriter/public/custom_rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/system/public/apr_mem_cache.h"
#include "net/instaweb/system/public/system_rewrite_options.h"
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/async_cache.h"
#include "net/instaweb/util/public/cache_batcher.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/cache_stats.h"
#include "net/instaweb/util/public/fallback_cache.h"
#include "net/instaweb/util/public/file_cache.h"
#include "net/instaweb/util/public/file_system_lock_manager.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/inprocess_shared_mem.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/null_shared_mem.h"
#include "net/instaweb/util/public/platform.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/shared_mem_lock_manager.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/threadsafe_cache.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/write_through_cache.h"
#include "net/instaweb/util/worker_test_base.h"

namespace net_instaweb {

namespace {

const char kCachePath[] = "/mem/path/";
const char kAltCachePath[] = "/mem/path_alt/";

class SystemCachesTest : public CustomRewriteTestBase<SystemRewriteOptions> {
 protected:
  static const int kThreadLimit = 3;
  static const int kUsableMetadataCacheSize = 8 * 1024;

  // Helper that blocks for async cache lookups.
  class BlockingCallback : public CacheInterface::Callback {
   public:
    explicit BlockingCallback(ThreadSystem* threads)
        : sync_(threads), result_(CacheInterface::kNotFound) {}

    CacheInterface::KeyState result() const { return result_; }
    GoogleString value() const { return value_; }

    void Block() {
      sync_.Wait();
    }

   protected:
    virtual void Done(CacheInterface::KeyState state) {
      result_ = state;
      CacheInterface::Callback::value()->Value().CopyToString(&value_);
      sync_.Notify();
    }

   private:
    WorkerTestBase::SyncPoint sync_;
    CacheInterface::KeyState result_;
    GoogleString value_;
  };

  // Helper that blocks for async HTTP cache lookups.
  class HTTPBlockingCallback : public HTTPCache::Callback {
   public:
    explicit HTTPBlockingCallback(ThreadSystem* threads)
        : Callback(RequestContext::NewTestRequestContext(threads)),
          sync_(threads),
          result_(HTTPCache::kNotFound) {}

    HTTPCache::FindResult result() const { return result_; }
    GoogleString value() const { return value_; }

    void Block() {
      sync_.Wait();
    }

   protected:
    virtual void Done(HTTPCache::FindResult state) {
      result_ = state;
      if (state == HTTPCache::kFound) {
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
        options_(new SystemRewriteOptions(thread_system_.get())) {
    shared_mem_.reset(new InProcessSharedMem(thread_system_.get()));
    factory_->set_hasher(new MD5Hasher());
    SystemCaches::InitStats(factory()->statistics());
  }

  virtual void SetUp() {
    system_caches_.reset(
        new SystemCaches(factory(), shared_mem_.get(), kThreadLimit));
    CustomRewriteTestBase<SystemRewriteOptions>::SetUp();
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
  ServerContext* SetupServerContext(SystemRewriteOptions* config) {
    scoped_ptr<ServerContext> server_context(factory()->NewServerContext());
    server_context->reset_global_options(config);
    server_context->set_statistics(factory()->statistics());
    system_caches_->SetupCaches(server_context.get());

    // Sanity-check that the two caches work.
    TestPut(server_context->metadata_cache(), "a", "b");
    TestGet(server_context->metadata_cache(), "a",
            CacheInterface::kAvailable, "b");

    TestHttpPut(server_context->http_cache(), "http://www.example.com", "a");
    TestHttpGet(server_context->http_cache(), "http://www.example.com",
                HTTPCache::kFound, "a");
    return server_context.release();
  }

  void TestPut(CacheInterface* cache, StringPiece key, StringPiece value) {
    GoogleString value_copy;
    value.CopyToString(&value_copy);
    SharedString shared_value;
    shared_value.SwapWithString(&value_copy);
    cache->Put(key.as_string(), &shared_value);
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

  void TestHttpPut(HTTPCache* cache, StringPiece key, StringPiece value) {
    ResponseHeaders headers;
    SetDefaultLongCacheHeaders(&kContentTypeText, &headers);
    cache->Put(key.as_string(), &headers, value, factory()->message_handler());
  }

  void TestHttpGet(HTTPCache* cache, StringPiece key,
                   HTTPCache::FindResult expected_state,
                   StringPiece expected_value) {
    HTTPBlockingCallback callback(thread_system_.get());
    cache->Find(key.as_string(), factory()->message_handler(), &callback);
    callback.Block();
    EXPECT_EQ(expected_state, callback.result());
    EXPECT_EQ(expected_value, callback.value());
  }

  // Returns empty string if not enabled. Tests should exit in that case.
  GoogleString MemCachedServerSpec() {
    if (server_spec_.empty()) {
      // This matches the logic in apr_mem_cache_test.
      const char* port_string = getenv("MEMCACHED_PORT");
      if (port_string == NULL) {
        LOG(ERROR) << "AprMemCache tests are skipped because env var "
                   << "$MEMCACHED_PORT is not set.  Set that to the port "
                   << "number where memcached is running to enable the "
                   << "tests.  See install/run_program_with_memcached.sh";
        // Does not fail the test.
        return "";
      }
      server_spec_ = StrCat("localhost:", port_string);
    }
    return server_spec_;
  }

  // Unwraps any wrapper cache objects.
  CacheInterface* SkipWrappers(CacheInterface* in) {
    CacheInterface* backend = in->Backend();
    if (backend != in) {
      return SkipWrappers(backend);
    }
    return in;
  }

  void TestBasicMemCacheAndNoLru(int num_threads_specified,
                                 int num_threads_expected) {
    if (MemCachedServerSpec().empty()) {
      return;
    }

    options_->set_file_cache_path(kCachePath);
    options_->set_use_shared_mem_locking(false);
    options_->set_lru_cache_kb_per_process(0);
    options_->set_memcached_servers(MemCachedServerSpec());
    options_->set_memcached_threads(num_threads_specified);
    PrepareWithConfig(options_.get());

    scoped_ptr<ServerContext> server_context(
        SetupServerContext(options_.release()));

    GoogleString mem_cache;
    if (num_threads_expected == 0) {
      mem_cache = Batcher(Stats("memcached", AprMemCache::FormatName()),
                          1, 1000);
    } else {
      mem_cache = Batcher(Stats("memcached", AsyncMemCache()),
                          num_threads_expected, 1000);
    }

    EXPECT_STREQ(
        Fallback(mem_cache, Stats("file_cache", FileCacheName())),
        server_context->metadata_cache()->Name());
    EXPECT_STREQ(
        HttpCache(
            Fallback(mem_cache, Stats("file_cache", FileCacheName()))),
        server_context->http_cache()->Name());
  }

  // Wrapper functions to format expected cache descriptor strings with
  // concise function calls exposing the cache structure via normal code
  // indentation.
  GoogleString WriteThrough(StringPiece l1, StringPiece l2) {
    return WriteThroughCache::FormatName(l1, l2);
  }

  GoogleString WriteThroughHTTP(StringPiece l1, StringPiece l2) {
    return WriteThroughHTTPCache::FormatName(l1, l2);
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

  GoogleString AsyncMemCache() {
    return AsyncCache::FormatName(AprMemCache::FormatName());
  }

  scoped_ptr<ThreadSystem> thread_system_;
  scoped_ptr<AbstractSharedMem> shared_mem_;
  scoped_ptr<SystemCaches> system_caches_;
  scoped_ptr<SystemRewriteOptions> options_;

 private:
  GoogleString server_spec_;  // Set lazily by MemCachedServerSpec()
};

TEST_F(SystemCachesTest, BasicFileAndLruCache) {
  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(100);
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  EXPECT_STREQ(WriteThrough(Stats("lru_cache", ThreadsafeLRU()),
                            Stats("file_cache", FileCacheName())),
               server_context->metadata_cache()->Name());
  EXPECT_STREQ(
      WriteThroughHTTP(
          HttpCache(Stats("lru_cache", ThreadsafeLRU())),
          HttpCache(Stats("file_cache", FileCacheName()))),
      server_context->http_cache()->Name());
}

TEST_F(SystemCachesTest, BasicFileOnlyCache) {
  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(0);
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  EXPECT_STREQ(Stats("file_cache", FileCacheName()),
               server_context->metadata_cache()->Name());
  EXPECT_STREQ(HttpCache(Stats("file_cache", FileCacheName())),
               server_context->http_cache()->Name());
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
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  EXPECT_STREQ(WriteThrough(Stats("lru_cache", ThreadsafeLRU()),
                            Stats("file_cache", FileCacheName())),
               server_context->metadata_cache()->Name());
  EXPECT_STREQ(
      WriteThroughHTTP(
          HttpCache(Stats("lru_cache", ThreadsafeLRU())),
          HttpCache(Stats("file_cache", FileCacheName()))),
      server_context->http_cache()->Name());
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
  EXPECT_STREQ(Stats("shm_cache", "SharedMemCache<64>"),
               server_context->metadata_cache()->Name());
  // HTTP cache is unaffected.
  EXPECT_STREQ(
      WriteThroughHTTP(HttpCache(Stats("lru_cache", ThreadsafeLRU())),
                       HttpCache(Stats("file_cache", FileCacheName()))),
      server_context->http_cache()->Name());
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
  EXPECT_STREQ(Stats("shm_cache", "SharedMemCache<64>"),
               server_context->metadata_cache()->Name());
  // HTTP cache is unaffected.
  EXPECT_STREQ(HttpCache(Stats("file_cache", FileCacheName())),
               server_context->http_cache()->Name());
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
  EXPECT_STREQ(Stats("shm_cache", "SharedMemCache<64>"),
               server_context->metadata_cache()->Name());
  // HTTP cache is unaffected.
  EXPECT_STREQ(
      WriteThroughHTTP(
          HttpCache(Stats("lru_cache", ThreadsafeLRU())),
          HttpCache(Stats("file_cache", FileCacheName()))),
      server_context->http_cache()->Name());
}

TEST_F(SystemCachesTest, BasicMemCachedAndLru) {
  if (MemCachedServerSpec().empty()) {
    return;
  }

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(100);
  options_->set_memcached_servers(MemCachedServerSpec());
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  EXPECT_STREQ(WriteThrough(Stats("lru_cache", ThreadsafeLRU()),
                            Fallback(Batcher(Stats("memcached",
                                                   AsyncMemCache()),
                                             1, 1000),
                                     Stats("file_cache", FileCacheName()))),
               server_context->metadata_cache()->Name());
  EXPECT_STREQ(
      WriteThroughHTTP(
          HttpCache(Stats("lru_cache", ThreadsafeLRU())),
          HttpCache(Fallback(Batcher(Stats("memcached", AsyncMemCache()),
                                     1, 1000),
                             Stats("file_cache", FileCacheName())))),
      server_context->http_cache()->Name());
}

TEST_F(SystemCachesTest, BasicMemCachedAndNoLru_0_Threads) {
  TestBasicMemCacheAndNoLru(0, 0);
}

TEST_F(SystemCachesTest, BasicMemCachedAndNoLru_1_Thread) {
  TestBasicMemCacheAndNoLru(1, 1);
}

TEST_F(SystemCachesTest, BasicMemCachedAndNoLru_2_Threads) {
  TestBasicMemCacheAndNoLru(2, 1);  // Clamp to 1.
}

TEST_F(SystemCachesTest, BasicMemCachedLruShm) {
  if (MemCachedServerSpec().empty()) {
    return;
  }

  GoogleString error_msg;
  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kCachePath, kUsableMetadataCacheSize, &error_msg));

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(100);
  options_->set_memcached_servers(MemCachedServerSpec());
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  // For metadata, we fallback to memcached behind shmcache.
  EXPECT_STREQ(
      WriteThrough(Stats("shm_cache", SharedMemCache<64>::FormatName()),
                   Fallback(Batcher(Stats("memcached", AsyncMemCache()),
                                    1, 1000),
                            Stats("file_cache", FileCacheName()))),
      server_context->metadata_cache()->Name());
  EXPECT_STREQ(
      WriteThroughHTTP(
          HttpCache(Stats("lru_cache", ThreadsafeLRU())),
          HttpCache(Fallback(Batcher(Stats("memcached", AsyncMemCache()),
                                     1, 1000),
                             Stats("file_cache", FileCacheName())))),
      server_context->http_cache()->Name());
}

TEST_F(SystemCachesTest, BasicMemCachedShmNoLru) {
  if (MemCachedServerSpec().empty()) {
    return;
  }

  GoogleString error_msg;
  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kCachePath, kUsableMetadataCacheSize, &error_msg));

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(0);
  options_->set_memcached_servers(MemCachedServerSpec());
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  EXPECT_STREQ(
      WriteThrough(
          Stats("shm_cache", "SharedMemCache<64>"),
          Fallback(Batcher(Stats("memcached", AsyncMemCache()), 1, 1000),
                   Stats("file_cache", FileCacheName()))),
      server_context->metadata_cache()->Name());
  EXPECT_STREQ(
      HttpCache(
          Fallback(Batcher(Stats("memcached", AsyncMemCache()), 1, 1000),
                   Stats("file_cache", FileCacheName()))),
      server_context->http_cache()->Name());
}

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

  TestHttpPut(servers[0]->http_cache(), "http://b.org", "value");
  TestHttpGet(servers[0]->http_cache(), "http://b.org",
              HTTPCache::kFound, "value");
  TestHttpGet(servers[1]->http_cache(), "http://b.org",
              HTTPCache::kFound, "value");
  TestHttpGet(servers[2]->http_cache(), "http://b.org",
              HTTPCache::kNotFound, "");

  // Lock managers have similar sharing semantics
  scoped_ptr<NamedLock> lock0(
      system_caches_->GetLockManager(configs[0])->CreateNamedLock("a"));
  scoped_ptr<NamedLock> lock1(
      system_caches_->GetLockManager(configs[1])->CreateNamedLock("a"));
  scoped_ptr<NamedLock> lock2(
      system_caches_->GetLockManager(configs[2])->CreateNamedLock("a"));
  EXPECT_TRUE(lock0->TryLock());
  EXPECT_FALSE(lock1->TryLock());
  EXPECT_TRUE(lock2->TryLock());
  lock0->Unlock();
  EXPECT_TRUE(lock1->TryLock());

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
    config->set_file_cache_path((i== 2) ? kAltCachePath : kCachePath);
    system_caches_->RegisterConfig(config);
    configs.push_back(config);
  }

  system_caches_->RootInit();
  // pretend we fork here.
  system_caches_->ChildInit();

  std::vector<ServerContext*> servers;
  for (int i = 0; i < 3; ++i) {
    servers.push_back(SetupServerContext(configs[i]));
    EXPECT_STREQ(Stats("shm_cache", "SharedMemCache<64>"),
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

TEST_F(SystemCachesTest, MemCachedShare) {
  if (MemCachedServerSpec().empty()) {
    return;
  }

  // Just share 3 memcached clients for the same server (so we don't
  // need 2 servers for the test)

  std::vector<SystemRewriteOptions*> configs;
  for (int i = 0; i < 3; ++i) {
    SystemRewriteOptions* config = options_->NewOptions();
    config->set_file_cache_path(kCachePath);
    config->set_memcached_servers(MemCachedServerSpec());
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
        Fallback(Batcher(Stats("memcached", AsyncMemCache()), 1, 1000),
                 Stats("file_cache", FileCacheName())),
        servers[i]->metadata_cache()->Name());
  }

  // Metadata + HTTP cache will end up shared
  TestPut(servers[0]->metadata_cache(), "b", "value");
  TestGet(servers[0]->metadata_cache(), "b",
          CacheInterface::kAvailable, "value");
  TestGet(servers[1]->metadata_cache(), "b",
          CacheInterface::kAvailable, "value");
  TestGet(servers[2]->metadata_cache(), "b",
          CacheInterface::kAvailable, "value");

  TestHttpPut(servers[0]->http_cache(), "http://b.org", "value");
  TestHttpGet(servers[0]->http_cache(), "http://b.org",
              HTTPCache::kFound, "value");
  TestHttpGet(servers[1]->http_cache(), "http://b.org",
              HTTPCache::kFound, "value");
  TestHttpGet(servers[2]->http_cache(), "http://b.org",
              HTTPCache::kFound, "value");

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
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  EXPECT_STREQ(Stats("file_cache", FileCacheName()),
               server_context->metadata_cache()->Name());
  EXPECT_STREQ(HttpCache(Stats("file_cache", FileCacheName())),
               server_context->http_cache()->Name());

  FileCache* file_cache = dynamic_cast<FileCache*>(
      SkipWrappers(server_context->metadata_cache()));
  ASSERT_TRUE(file_cache != NULL);
  EXPECT_EQ(kCachePath, file_cache->path());
  EXPECT_EQ(3 * Timer::kHourMs, file_cache->cache_policy()->clean_interval_ms);
  // Note: this is in bytes, the setting is in kb.
  EXPECT_EQ(1024*1024, file_cache->cache_policy()->target_size);
  EXPECT_EQ(50000, file_cache->cache_policy()->target_inode_count);
  EXPECT_TRUE(file_cache->worker() != NULL);
}

TEST_F(SystemCachesTest, LruCacheSettings) {
  // Test that we apply LRU cache settings right.
  options_->set_file_cache_path(kCachePath);
  options_->set_lru_cache_kb_per_process(1024);
  options_->set_lru_cache_byte_limit(500);
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

  // Also on the HTTP cache (which has a separate write through class.
  WriteThroughHTTPCache* http_write_through =
      dynamic_cast<WriteThroughHTTPCache*>(server_context->http_cache());
  ASSERT_TRUE(http_write_through != NULL);
  EXPECT_EQ(500, http_write_through->cache1_limit());
}

TEST_F(SystemCachesTest, StatsStringMinimal) {
  // The format is rather dependent on the implementation so we don't check it,
  // but we do care that it at least doesn't crash.
  GoogleString out;

  if (MemCachedServerSpec().empty()) {
    return;
  }

  GoogleString error_msg;
  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kCachePath, kUsableMetadataCacheSize, &error_msg));

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(0);
  options_->set_memcached_servers(MemCachedServerSpec());
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));

  system_caches_->PrintCacheStats(
      static_cast<SystemCaches::StatFlags>(SystemCaches::kGlobalView |
                                           SystemCaches::kIncludeMemcached),
      &out);
}

// Tests for how we fallback when SHM setup ops fail.
class BrokenShmSystemCachesTest : public SystemCachesTest {
 protected:
  BrokenShmSystemCachesTest() {
    shared_mem_.reset(new NullSharedMem());
  }
};

TEST_F(BrokenShmSystemCachesTest, FallbackShmLockManager) {
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

TEST_F(BrokenShmSystemCachesTest, FallbackShmAndLru) {
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
  EXPECT_STREQ(WriteThrough(Stats("lru_cache", ThreadsafeLRU()),
                            Stats("file_cache", FileCacheName())),
               server_context->metadata_cache()->Name());
  // HTTP cache is unaffected.
  EXPECT_STREQ(
      WriteThroughHTTP(
          HttpCache(Stats("lru_cache", ThreadsafeLRU())),
          HttpCache(Stats("file_cache", FileCacheName()))),
      server_context->http_cache()->Name());
}

TEST_F(BrokenShmSystemCachesTest, FallbackShmAndNoLru) {
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
  EXPECT_STREQ(Stats("file_cache", FileCacheName()),
               server_context->metadata_cache()->Name());
  // HTTP cache is unaffected.
  EXPECT_STREQ(HttpCache(Stats("file_cache", FileCacheName())),
               server_context->http_cache()->Name());
}

TEST_F(BrokenShmSystemCachesTest, FallbackMemCachedLruShm) {
  if (MemCachedServerSpec().empty()) {
    return;
  }

  GoogleString error_msg;
  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kCachePath, kUsableMetadataCacheSize, &error_msg));

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(100);
  options_->set_memcached_servers(MemCachedServerSpec());
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  // For metadata, we fallback to memcached behind shmcache.
  EXPECT_STREQ(
      WriteThrough(
          Stats("lru_cache", ThreadsafeLRU()),
          Fallback(Batcher(Stats("memcached", AsyncMemCache()), 1, 1000),
                   Stats("file_cache", FileCacheName()))),
      server_context->metadata_cache()->Name());
  EXPECT_STREQ(
      WriteThroughHTTP(
          HttpCache(Stats("lru_cache", ThreadsafeLRU())),
          HttpCache(Fallback(Batcher(Stats("memcached", AsyncMemCache()),
                                     1, 1000),
                             Stats("file_cache", FileCacheName())))),
      server_context->http_cache()->Name());
}

TEST_F(BrokenShmSystemCachesTest, FallbackMemCachedShmNoLru) {
  if (MemCachedServerSpec().empty()) {
    return;
  }

  GoogleString error_msg;
  EXPECT_TRUE(system_caches_->CreateShmMetadataCache(
      kCachePath, kUsableMetadataCacheSize, &error_msg));

  options_->set_file_cache_path(kCachePath);
  options_->set_use_shared_mem_locking(false);
  options_->set_lru_cache_kb_per_process(0);
  options_->set_memcached_servers(MemCachedServerSpec());
  PrepareWithConfig(options_.get());

  scoped_ptr<ServerContext> server_context(
      SetupServerContext(options_.release()));
  EXPECT_STREQ(
      Fallback(Batcher(Stats("memcached", AsyncMemCache()), 1, 1000),
               Stats("file_cache", FileCacheName())),
      server_context->metadata_cache()->Name());
  EXPECT_STREQ(
      HttpCache(
          Fallback(Batcher(Stats("memcached", AsyncMemCache()), 1, 1000),
                   Stats("file_cache", FileCacheName()))),
      server_context->http_cache()->Name());
}

}  // namespace

}  // namespace net_instaweb
