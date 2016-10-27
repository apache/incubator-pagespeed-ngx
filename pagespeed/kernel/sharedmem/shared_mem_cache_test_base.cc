// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: morlovich@google.com (Maksim Orlovich)

#include "pagespeed/kernel/sharedmem/shared_mem_cache_test_base.h"

#include <unistd.h>
#include <cstddef>                     // for size_t
#include <map>
#include <utility>

#include "base/logging.h"               // for Check_EQImpl, CHECK_EQ
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/sharedmem/shared_mem_cache.h"
#include "pagespeed/kernel/sharedmem/shared_mem_cache_snapshot.pb.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

namespace {

const char kSegment[] = "cache";
const char kAltSegment[] = "alt_cache";
const int kSectors = 2;
const int kSectorBlocks = 2000;
const int kSectorEntries = 256;
const int kSpinRuns = 100;
// Tests don't actually rely on this value, it just needs to be >0.
const int kSnapshotIntervalMs = 1000;

// In some tests we have tight consumer/producer spinloops assuming they'll get
// preempted to let other end proceed. Valgrind does not actually do that
// sometimes.
void YieldToThread() {
  usleep(1);
}

}  // namespace

SharedMemCacheTestBase::SharedMemCacheTestBase(SharedMemTestEnv* env)
    : test_env_(env),
      shmem_runtime_(env->CreateSharedMemRuntime()),
      thread_system_(Platform::CreateThreadSystem()),
      handler_(thread_system_->NewMutex()),
      timer_(thread_system_->NewMutex(), 0),
      sanity_checks_enabled_(true) {
  cache_.reset(MakeCache());
  EXPECT_TRUE(cache_->Initialize());

  // Compute a large string for tests --- one large enough to take multiple
  // blocks (2 complete blocks, plus 43 bytes in a 3rd one, where 43
  // is a completely arbitrary small integer smaller than the block size)
  for (size_t c = 0; c < kBlockSize * 2 + 43; ++c) {
    large_.push_back('A' + (c % 26));
  }

  // Now a gigantic one, which goes close to the size limit, which is 1/32nd
  // of sector size.
  for (size_t c = 0; c < kBlockSize * kSectorBlocks / 40 + 43; ++c) {
    gigantic_.push_back('a' + (c % 26));
  }
}

SharedMemCache<SharedMemCacheTestBase::kBlockSize>*
SharedMemCacheTestBase::MakeCache() {
  return new SharedMemCache<kBlockSize>(shmem_runtime_.get(), kSegment, &timer_,
                                        &hasher_, kSectors, kSectorEntries,
                                        kSectorBlocks, &handler_);
}

void SharedMemCacheTestBase::TearDown() {
  cache_->GlobalCleanup(shmem_runtime_.get(), kSegment, &handler_);
  CacheTestBase::TearDown();
}

void SharedMemCacheTestBase::ResetCache() {
  cache_->GlobalCleanup(shmem_runtime_.get(), kSegment, &handler_);
  cache_.reset(MakeCache());
  EXPECT_TRUE(cache_->Initialize());
}

bool SharedMemCacheTestBase::CreateChild(TestMethod method) {
  Function* fn = new MemberFunction0<SharedMemCacheTestBase>(method, this);
  return test_env_->CreateChild(fn);
}

void SharedMemCacheTestBase::SanityCheck() {
  if (sanity_checks_enabled_) {
    cache_->SanityCheck();
  }
}

void SharedMemCacheTestBase::TestBasic() {
  CheckNotFound("404");
  CheckDelete("404");
  CheckNotFound("404");

  CheckPut("200", "OK");
  CheckGet("200", "OK");
  CheckNotFound("404");

  CheckPut("002", "KO!");
  CheckGet("002", "KO!");
  CheckGet("200", "OK");
  CheckNotFound("404");

  CheckDelete("002");
  CheckNotFound("002");
  CheckNotFound("404");
  CheckGet("200", "OK");

  CheckPut("big", large_);
  CheckGet("big", large_);

  // Make sure this at least doesn't blow up.
  cache_->DumpStats();
}

void SharedMemCacheTestBase::TestReinsert() {
  CheckPut("key", "val");
  CheckGet("key", "val");

  // Insert the same size.
  CheckPut("key", "alv");
  CheckGet("key", "alv");

  // Insert larger one..
  CheckPut("key", large_);
  CheckGet("key", large_);

  // Now shrink it down again.
  CheckPut("key", "small");
  CheckGet("key", "small");

  // ... And make it huge.
  CheckPut("key", gigantic_);
  CheckGet("key", gigantic_);

  // Now try with empty value
  CheckPut("key", "");
  CheckGet("key", "");
}

void SharedMemCacheTestBase::TestReplacement() {
  sanity_checks_enabled_ = false;  // To expensive for that much work.

  // Make sure we can allocate spec from replacement, too, but that it doesn't
  // affect very recent files (barring collisions). All 3 entries below should
  // fit into one sector.
  // Now throw in tons of small files, to make sure we load the
  // directory heavily.
  for (int n = 0; n < kSectorEntries * 4; ++n) {
    GoogleString key1 = IntegerToString(n);
    CheckPut(key1, key1);
    timer_.AdvanceMs(1);
    CheckPut(key1, key1);
    CheckGet(key1, key1);
  }

  cache_->SanityCheck();

  for (int n = 0; n < 100; n += 3) {
    GoogleString key1 = IntegerToString(n);
    GoogleString key2 = IntegerToString(n + 1);
    GoogleString key3 = IntegerToString(n + 2);

    CheckPut(key1, large_);
    timer_.AdvanceMs(1);
    CheckPut(key2, key2);
    timer_.AdvanceMs(1);
    CheckPut(key3, gigantic_);
    timer_.AdvanceMs(1);

    CheckGet(key1, large_);
    CheckGet(key2, key2);
    CheckGet(key3, gigantic_);
    CheckDelete(key2.c_str());
    timer_.AdvanceMs(1);
    CheckNotFound(key2.c_str());
  }

  cache_->SanityCheck();
}

void SharedMemCacheTestBase::TestReaderWriter() {
  CreateChild(&SharedMemCacheTestBase::TestReaderWriterChild);

  for (int i = 0; i < kSpinRuns; ++i) {
    // Wait until the child puts in a proper value for 'key'
    CacheTestBase::Callback callback;
    while (callback.state() != CacheInterface::kAvailable) {
      cache_->Get("key", callback.Reset());
      ASSERT_EQ(true, callback.called());
      YieldToThread();
    }
    EXPECT_EQ(large_, callback.value().Value());
    CheckDelete("key");

    CheckPut("key2", "val2");
  }

  test_env_->WaitForChildren();
}

void SharedMemCacheTestBase::TestReaderWriterChild() {
  scoped_ptr<SharedMemCache<kBlockSize> > child_cache(MakeCache());
  if (!child_cache->Attach()) {
    test_env_->ChildFailed();
  }
  SharedString val(large_);

  for (int i = 0; i < kSpinRuns; ++i) {
    child_cache->Put("key", val);

    // Now wait until the parent puts in what we expect for 'key2'
    CacheTestBase::Callback callback;
    while (callback.state() != CacheInterface::kAvailable) {
      child_cache->Get("key2", callback.Reset());
      ASSERT_EQ(true, callback.called());
      YieldToThread();
    }

    if (callback.value().Value() != "val2") {
      test_env_->ChildFailed();
    }
    child_cache->Delete("key2");
  }
}

void SharedMemCacheTestBase::TestConflict() {
  const int kAssociativity = SharedMemCache<kBlockSize>::kAssociativity;

  // We create a cache with 1 sector, and kAssociativity entries, since it
  // makes it easy to get a conflict and replacement.
  scoped_ptr<SharedMemCache<kBlockSize> > small_cache(
      new SharedMemCache<kBlockSize>(shmem_runtime_.get(), kAltSegment, &timer_,
                                     &hasher_, 1 /* sectors*/,
                                     kAssociativity /* entries / sector */,
                                     kSectorBlocks, &handler_));
  ASSERT_TRUE(small_cache->Initialize());

  // Insert kAssociativity + 1 entries.
  for (int c = 0; c <= kAssociativity; ++c) {
    GoogleString key = IntegerToString(c);
    CheckPut(small_cache.get(), key, key);
  }

  // Now make sure the final one is available.
  // It would seem like one could predict replacement order exactly, but
  // with us only having kAssoc possible key values, it's quite likely
  // that the constructed key set will not have full associativity.
  GoogleString last(IntegerToString(kAssociativity));
  CheckGet(small_cache.get(), last, last);
  small_cache->GlobalCleanup(shmem_runtime_.get(), kAltSegment, &handler_);
}

void SharedMemCacheTestBase::TestEvict() {
  // We create a cache with 1 sector as it makes it easier to reason
  // about how much room is left.
  scoped_ptr<SharedMemCache<kBlockSize> > small_cache(
      new SharedMemCache<kBlockSize>(shmem_runtime_.get(), kAltSegment, &timer_,
                                     &hasher_, 1 /* sectors*/,
                                     kSectorBlocks * 4 /* entries / sector */,
                                     kSectorBlocks, &handler_));
  ASSERT_TRUE(small_cache->Initialize());

  // Insert large_ kSectorBlocks times. Since large_ is ~3 blocks in size,
  // we will need to evict older entries eventually.
  for (int c = 0; c < kSectorBlocks; ++c) {
    GoogleString key = IntegerToString(c);
    CheckPut(small_cache.get(), key, large_);
    CheckGet(small_cache.get(), key, large_);
  }

  small_cache->GlobalCleanup(shmem_runtime_.get(), kAltSegment, &handler_);
}

void SharedMemCacheTestBase::CheckDumpsEqual(
    const SharedMemCacheDump& a, const SharedMemCacheDump& b,
    const char* test_label) {
  ASSERT_EQ(a.entry_size(), b.entry_size()) << test_label;

  for (int i = 0; i < a.entry_size(); ++i) {
    EXPECT_EQ(a.entry(i).value(), b.entry(i).value()) << test_label;
    EXPECT_EQ(a.entry(i).raw_key(), b.entry(i).raw_key()) << test_label;
    EXPECT_EQ(a.entry(i).last_use_timestamp_ms(),
              b.entry(i).last_use_timestamp_ms()) << test_label;
  }
}

void SharedMemCacheTestBase::TestSnapshot() {
  const int kEntries = 10;
  const int64 kLastWriteMs = 1234567;

  // Put in 10 values: key0 ... key9 set to val0 ... val9, each with timestamp
  // corresponding to their number.
  for (int i = 0; i < kEntries; ++i) {
    CheckPut(StrCat("key", IntegerToString(i)),
             StrCat("val", IntegerToString(i)));
    timer_.AdvanceMs(1);
  }

  SharedMemCacheDump dump;
  for (int i = 0; i < kSectors; ++i) {
    // We explicitly SetLastWriteMsForTesting so we can build a snapshot with
    // where every sector is included but entries all have different
    // timestamps.
    Cache()->SetLastWriteMsForTesting(i, kLastWriteMs);
    EXPECT_TRUE(Cache()->AddSectorToSnapshot(i, kLastWriteMs, &dump));
  }

  // Make sure we can still access the cache. Also move the time forward, so we
  // can check timestamps are using old values after restoring the snapshot.
  for (int i = 0; i < kEntries; ++i) {
    CheckGet(StrCat("key", IntegerToString(i)),
             StrCat("val", IntegerToString(i)));
    timer_.AdvanceMs(1);
  }

  // Now check the dump contents. We can't inspect the keys directly, but
  // we can at least check values and timestamps.
  std::map<GoogleString, int64> value_to_timestamp;
  EXPECT_EQ(kEntries, dump.entry_size());
  for (int i = 0; i < dump.entry_size(); ++i) {
    const SharedMemCacheDumpEntry& entry = dump.entry(i);
    value_to_timestamp[entry.value()] = entry.last_use_timestamp_ms();
  }

  // Make sure size is right (e.g. no dupes).
  EXPECT_EQ(static_cast<size_t>(kEntries), value_to_timestamp.size());

  // Now see that the correspondence is right.
  for (std::map<GoogleString, int64>::iterator i = value_to_timestamp.begin();
       i != value_to_timestamp.end(); ++i) {
    EXPECT_EQ(i->first, StrCat("val", Integer64ToString(i->second)));
  }

  // Now round-trip to new object via string serialization
  GoogleString encoded_dump;
  Cache()->MarshalSnapshot(dump, &encoded_dump);
  SharedMemCacheDump decoded_dump;
  Cache()->DemarshalSnapshot(encoded_dump, &decoded_dump);

  CheckDumpsEqual(dump, decoded_dump, "dump vs decoded_dump");

  // Now make a new cache, which should initially be empty.
  ResetCache();
  for (int i = 0; i < kEntries; ++i) {
    CheckNotFound(StrCat("key", IntegerToString(i)).c_str());
  }

  // Restore it from decoded_dump
  Cache()->RestoreSnapshot(decoded_dump);

  // Save yet another dump. This is basically the best we can do to make sure
  // that the timestamps got restored properly.
  SharedMemCacheDump roundtrip_dump;
  for (int i = 0; i < kSectors; ++i) {
    Cache()->SetLastWriteMsForTesting(i, kLastWriteMs);
    EXPECT_TRUE(
        Cache()->AddSectorToSnapshot(i, kLastWriteMs, &roundtrip_dump));
    EXPECT_EQ(timer_.NowMs(), Cache()->GetLastWriteMsForTesting(i));
  }

  CheckDumpsEqual(dump, roundtrip_dump, "dump vs. roundtrip_dump");

  // Check to make sure all values are OK.
  for (int i = 0; i < kEntries; ++i) {
    CheckGet(StrCat("key", IntegerToString(i)),
             StrCat("val", IntegerToString(i)));
  }

  // Test that if checkpoint timestamps don't match we don't make a dump or
  // update the sector's last_checkpoint_ms.
  SharedMemCacheDump dump_ts_mismatch;
  int sector_num = 0;
  Cache()->SetLastWriteMsForTesting(sector_num, kLastWriteMs);
  EXPECT_FALSE(Cache()->AddSectorToSnapshot(
      sector_num, kLastWriteMs - 1, &dump_ts_mismatch));
  EXPECT_EQ(kLastWriteMs, Cache()->GetLastWriteMsForTesting(sector_num));
  EXPECT_EQ(0, dump_ts_mismatch.entry_size());
}

void SharedMemCacheTestBase::CheckDelete(const char* key) {
  cache_->Delete(key);
  SanityCheck();
}

void SharedMemCacheTestBase::TestRegisterSnapshotFileCache() {
  // Test that we handle setting the file cache to multiple paths by picking the
  // one that's first alphabetically.
  scoped_ptr<FileCacheTestWrapper> file_cache_wrapper_abc(
      new FileCacheTestWrapper(
          "/abc", thread_system_.get(), &timer_, &handler_));
  cache_->RegisterSnapshotFileCache(file_cache_wrapper_abc->file_cache(),
                                    kSnapshotIntervalMs);
  CHECK_EQ(cache_->snapshot_path(), "/abc");
  CHECK_EQ(cache_->file_cache(), file_cache_wrapper_abc->file_cache());

  // Alphabetically before /abc, so replaces it.
  scoped_ptr<FileCacheTestWrapper> file_cache_wrapper_abb(
      new FileCacheTestWrapper(
          "/abb", thread_system_.get(), &timer_, &handler_));
  cache_->RegisterSnapshotFileCache(file_cache_wrapper_abb->file_cache(),
                                    kSnapshotIntervalMs);
  CHECK_EQ(cache_->snapshot_path(), "/abb");
  CHECK_EQ(cache_->file_cache(), file_cache_wrapper_abb->file_cache());

  // Not before /abb, so doesn't replace it.
  scoped_ptr<FileCacheTestWrapper> file_cache_wrapper_acb(
      new FileCacheTestWrapper(
          "/acb", thread_system_.get(), &timer_, &handler_));
  cache_->RegisterSnapshotFileCache(file_cache_wrapper_acb->file_cache(),
                                    kSnapshotIntervalMs);
  CHECK_EQ(cache_->snapshot_path(), "/abb");
  CHECK_EQ(cache_->file_cache(), file_cache_wrapper_abb->file_cache());

  // Before /abb, so does replace it.
  scoped_ptr<FileCacheTestWrapper> file_cache_wrapper_aab(
      new FileCacheTestWrapper(
          "/aab", thread_system_.get(), &timer_, &handler_));
  cache_->RegisterSnapshotFileCache(file_cache_wrapper_aab->file_cache(),
                                    kSnapshotIntervalMs);
  CHECK_EQ(cache_->snapshot_path(), "/aab");
  CHECK_EQ(cache_->file_cache(), file_cache_wrapper_aab->file_cache());

  // The cache was constructed with a filename of kSegment, and a match on
  // filename should always win here.
  scoped_ptr<FileCacheTestWrapper> file_cache_wrapper_ksegment(
      new FileCacheTestWrapper(
          kSegment, thread_system_.get(), &timer_, &handler_));
  cache_->RegisterSnapshotFileCache(file_cache_wrapper_ksegment->file_cache(),
                                    kSnapshotIntervalMs);
  CHECK_EQ(cache_->snapshot_path(), kSegment);
  CHECK_EQ(cache_->file_cache(), file_cache_wrapper_ksegment->file_cache());

  // Before kSegment, but doesn't replace it because kSegment was a filename
  // match.
  scoped_ptr<FileCacheTestWrapper> file_cache_wrapper_aaa(
      new FileCacheTestWrapper(
          "/aaa", thread_system_.get(), &timer_, &handler_));
  cache_->RegisterSnapshotFileCache(file_cache_wrapper_aaa->file_cache(),
                                    kSnapshotIntervalMs);
  CHECK_EQ(cache_->snapshot_path(), kSegment);
  CHECK_EQ(cache_->file_cache(), file_cache_wrapper_ksegment->file_cache());
}

void SharedMemCacheTestBase::TestCheckpointAndRestore() {
  const GoogleString kPath = "/a-path";

  // Setup.
  cache_.reset(new SharedMemCache<kBlockSize>(
      shmem_runtime_.get(), kPath, &timer_, &hasher_, kSectors,
      kSectorEntries, kSectorBlocks, &handler_));
  scoped_ptr<FileCacheTestWrapper> file_cache_wrapper(
      new FileCacheTestWrapper(
          kPath, thread_system_.get(), &timer_, &handler_));
  cache_->RegisterSnapshotFileCache(file_cache_wrapper->file_cache(),
                                    kSnapshotIntervalMs);
  EXPECT_EQ(cache_->file_cache(), file_cache_wrapper->file_cache());
  EXPECT_TRUE(cache_->Initialize());

  // Now we're set up and can start testing.

  // Put something in the cache.
  CheckPut("200", "OK");
  CheckGet("200", "OK");

  const int64 kLastWriteMs = 1234567;
  for (int sector_num = 0; sector_num < kSectors; ++sector_num) {
    // Explicitly set kLastWriteMs so we don't have to worry about it.
    cache_->SetLastWriteMsForTesting(sector_num, kLastWriteMs);
    cache_->WriteOutSnapshotForTesting(sector_num, kLastWriteMs);
  }

  // Check that it did get written out.
  for (int sector_num = 0; sector_num < kSectors; ++sector_num) {
    EXPECT_EQ(timer_.NowMs(), cache_->GetLastWriteMsForTesting(sector_num));
  }

  // Reset the cache, but don't set a file system.  We expect not to load
  // anything.
  cache_.reset(new SharedMemCache<kBlockSize>(
      shmem_runtime_.get(), kPath, &timer_, &hasher_, kSectors,
      kSectorEntries, kSectorBlocks, &handler_));
  EXPECT_TRUE(cache_->Initialize());
  CheckNotFound("200");

  // Reset the cache, set a file system, but change the shmcache's path.  This
  // is similar to the case where a default shm cache and an explicitly
  // configured one share the same file cache path.  We expect not to load
  // anything, because the path is part of the key.
  cache_.reset(new SharedMemCache<kBlockSize>(
      shmem_runtime_.get(), "default-shm-cache", &timer_, &hasher_, kSectors,
      kSectorEntries, kSectorBlocks, &handler_));
  cache_->RegisterSnapshotFileCache(file_cache_wrapper->file_cache(),
                                    kSnapshotIntervalMs);
  EXPECT_TRUE(cache_->Initialize());
  CheckNotFound("200");

  // Now reset the cache, but do set the file system.  Everything should be
  // loaded back in.
  cache_.reset(new SharedMemCache<kBlockSize>(
      shmem_runtime_.get(), kPath, &timer_, &hasher_, kSectors,
      kSectorEntries, kSectorBlocks, &handler_));
  cache_->RegisterSnapshotFileCache(file_cache_wrapper->file_cache(),
                                    kSnapshotIntervalMs);
  EXPECT_TRUE(cache_->Initialize());
  CheckGet("200", "OK");

  // If the files are deleted the cache is still fine.
  file_cache_wrapper->filesystem()->Clear();
  CheckGet("200", "OK");

  // But you can't reload from an empty filesystem.
  cache_.reset(new SharedMemCache<kBlockSize>(
      shmem_runtime_.get(), kPath, &timer_, &hasher_, kSectors,
      kSectorEntries, kSectorBlocks, &handler_));
  cache_->RegisterSnapshotFileCache(file_cache_wrapper->file_cache(),
                                    kSnapshotIntervalMs);
  EXPECT_TRUE(cache_->Initialize());
  CheckNotFound("200");
}

}  // namespace net_instaweb
