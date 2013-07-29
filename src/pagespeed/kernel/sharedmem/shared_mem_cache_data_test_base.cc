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

#include "pagespeed/kernel/sharedmem/shared_mem_cache_data_test_base.h"

#include <cstddef>                     // for size_t
#include <set>

#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

using SharedMemCacheData::BlockNum;
using SharedMemCacheData::BlockVector;
using SharedMemCacheData::CacheEntry;
using SharedMemCacheData::EntryNum;
using SharedMemCacheData::Sector;
using SharedMemCacheData::kInvalidEntry;

namespace {

const char kSegment[] = "cache";
const size_t kExtra = 8192;  // we only allocate one segment,
                             // so we prepend these many bytes to make
                             // sure the offset is actually honored.

const int kBlocks = 20;
const int kEntries = 25;

}  // namespace

SharedMemCacheDataTestBase::SharedMemCacheDataTestBase(SharedMemTestEnv* env)
    : test_env_(env),
      shmem_runtime_(env->CreateSharedMemRuntime()),
      thread_system_(Platform::CreateThreadSystem()),
      handler_(thread_system_->NewMutex()) {
}

bool SharedMemCacheDataTestBase::CreateChild(TestMethod method) {
  Function* callback =
      new MemberFunction0<SharedMemCacheDataTestBase>(method, this);
  return test_env_->CreateChild(callback);
}

void SharedMemCacheDataTestBase::SanityCheckBlockVector(
    const SharedMemCacheData::BlockVector& blocks, int min_valid,
    int max_valid) {
  // Make sure all numbers are in range, unique.
  std::set<BlockNum> distinct_count;
  for (size_t c = 0; c < blocks.size(); ++c) {
    EXPECT_LE(min_valid, blocks[c]);
    EXPECT_LE(blocks[c], max_valid);
    distinct_count.insert(c);
  }
  EXPECT_EQ(blocks.size(), distinct_count.size());
}

void SharedMemCacheDataTestBase::ExtractAndSanityCheckLRU(
    Sector<SharedMemCacheDataTestBase::kBlockSize>* sector,
    std::vector<EntryNum>* out_lru) {

  // collect list starting form oldest.
  std::vector<EntryNum> backwards_lru;
  for (EntryNum e = sector->OldestEntryNum(); e != kInvalidEntry;
       e = sector->EntryAt(e)->lru_prev) {
    backwards_lru.push_back(e);
  }

  size_t size = backwards_lru.size();

  // Flip it to be from newest.
  out_lru->clear();
  for (size_t p = 0; p < size; ++p) {
    size_t backwards_pos = size - 1 - p;
    out_lru->push_back(backwards_lru[backwards_pos]);
  }

  // Sanity check it.
  for (size_t p = 0; p < size; ++p) {
    CacheEntry* entry = sector->EntryAt(out_lru->at(p));

    if (p == 0) {
      EXPECT_EQ(kInvalidEntry, entry->lru_prev);
    } else {
      EXPECT_EQ(out_lru->at(p - 1), entry->lru_prev);
    }

    if (p == (size - 1)) {
      EXPECT_EQ(kInvalidEntry, entry->lru_next);
    } else {
      EXPECT_EQ(out_lru->at(p + 1), entry->lru_next);
    }
  }

  EXPECT_EQ(out_lru->size(),
            static_cast<size_t>(sector->sector_stats()->used_entries));
}

void SharedMemCacheDataTestBase::TestFreeList() {
  AbstractSharedMemSegment* seg_raw_ptr = NULL;
  Sector<kBlockSize>* sector_raw_ptr = NULL;
  ASSERT_TRUE(ParentInit(&seg_raw_ptr, &sector_raw_ptr));
  scoped_ptr<AbstractSharedMemSegment> seg(seg_raw_ptr);
  scoped_ptr<Sector<kBlockSize> > sector(sector_raw_ptr);

  // Ask for more than the sector has; get exactly as many as it has.
  EXPECT_EQ(0, sector->sector_stats()->used_blocks);
  BlockVector blocks;
  EXPECT_EQ(kBlocks, sector->AllocBlocksFromFreeList(kBlocks * 2, &blocks));
  EXPECT_EQ(static_cast<size_t>(kBlocks), blocks.size());
  SanityCheckBlockVector(blocks, 0, kBlocks - 1);
  EXPECT_EQ(kBlocks, sector->sector_stats()->used_blocks);

  // Now asking to allocate more should not return anything.
  EXPECT_EQ(0, sector->AllocBlocksFromFreeList(5, &blocks));
  // Should not damage previous blocks set, however.
  EXPECT_EQ(static_cast<size_t>(kBlocks), blocks.size());
  EXPECT_EQ(kBlocks, sector->sector_stats()->used_blocks);

  // Now free blocks 0, 1, 2
  blocks.clear();
  blocks.push_back(0);
  blocks.push_back(1);
  blocks.push_back(2);
  sector->ReturnBlocksToFreeList(blocks);
  EXPECT_EQ(kBlocks - 3, sector->sector_stats()->used_blocks);

  // Now allocate 2 blocks then 1 blocks. Should succeed, and return 3
  // blocks of valid range.
  blocks.clear();
  EXPECT_EQ(2, sector->AllocBlocksFromFreeList(2, &blocks));
  EXPECT_EQ(static_cast<size_t>(2), blocks.size());
  EXPECT_EQ(kBlocks - 1, sector->sector_stats()->used_blocks);
  SanityCheckBlockVector(blocks, 0, 2);
  EXPECT_EQ(1, sector->AllocBlocksFromFreeList(1, &blocks));
  EXPECT_EQ(static_cast<size_t>(3), blocks.size());
  SanityCheckBlockVector(blocks, 0, 2);
  EXPECT_EQ(kBlocks, sector->sector_stats()->used_blocks);

  // Run the kid, which will free things other than 0, 1, 2.
  CreateChild(&SharedMemCacheDataTestBase::TestFreeListChild);
  test_env_->WaitForChildren();
  EXPECT_EQ(3, sector->sector_stats()->used_blocks);

  // Now try allocating this rest, appending it into blocks.
  EXPECT_EQ(kBlocks - 3, sector->AllocBlocksFromFreeList(kBlocks - 3, &blocks));
  EXPECT_EQ(static_cast<size_t>(kBlocks), blocks.size());
  SanityCheckBlockVector(blocks, 0, kBlocks - 1);
  EXPECT_EQ(kBlocks, sector->sector_stats()->used_blocks);

  ParentCleanup();
}

void SharedMemCacheDataTestBase::TestFreeListChild() {
  AbstractSharedMemSegment* seg_raw_ptr = NULL;
  Sector<kBlockSize>* sector_raw_ptr = NULL;
  if (!ChildInit(&seg_raw_ptr, &sector_raw_ptr)) {
    test_env_->ChildFailed();
  }

  scoped_ptr<AbstractSharedMemSegment> seg(seg_raw_ptr);
  scoped_ptr<Sector<kBlockSize> > sector(sector_raw_ptr);

  // Free blocks [3, kBlocks)
  BlockVector to_free;
  for (BlockNum b = 3; b < kBlocks; ++b) {
    to_free.push_back(b);
  }

  sector->ReturnBlocksToFreeList(to_free);
}

void SharedMemCacheDataTestBase::TestLRU() {
  AbstractSharedMemSegment* seg_raw_ptr = NULL;
  Sector<kBlockSize>* sector_raw_ptr = NULL;
  ASSERT_TRUE(ParentInit(&seg_raw_ptr, &sector_raw_ptr));
  scoped_ptr<AbstractSharedMemSegment> seg(seg_raw_ptr);
  scoped_ptr<Sector<kBlockSize> > sector(sector_raw_ptr);

  // Initially, nothing should be in the LRU.
  std::vector<EntryNum> lru;
  ExtractAndSanityCheckLRU(sector.get(), &lru);
  EXPECT_TRUE(lru.empty());
  EXPECT_EQ(0, sector->sector_stats()->used_entries);

  // Trying to unlink things again is fine, too.
  sector->UnlinkEntryFromLRU(0);
  EXPECT_EQ(0, sector->sector_stats()->used_entries);
  sector->UnlinkEntryFromLRU(1);
  EXPECT_EQ(0, sector->sector_stats()->used_entries);

  // Now insert 5 things into the LRU manually, with entry 0 expected
  // to be in front.
  for (EntryNum cur = 4; cur >= 0; --cur) {
    sector->InsertEntryIntoLRU(cur);
  }
  EXPECT_EQ(5, sector->sector_stats()->used_entries);

  ExtractAndSanityCheckLRU(sector.get(), &lru);
  ASSERT_EQ(5u, lru.size());
  EXPECT_EQ(0, lru[0]);
  EXPECT_EQ(1, lru[1]);
  EXPECT_EQ(2, lru[2]);
  EXPECT_EQ(3, lru[3]);
  EXPECT_EQ(4, lru[4]);

  // Unlink the middle, and two endpoints.
  sector->UnlinkEntryFromLRU(2);
  sector->UnlinkEntryFromLRU(0);
  sector->UnlinkEntryFromLRU(4);
  EXPECT_EQ(2, sector->sector_stats()->used_entries);

  ExtractAndSanityCheckLRU(sector.get(), &lru);
  ASSERT_EQ(2u, lru.size());
  EXPECT_EQ(1, lru[0]);
  EXPECT_EQ(3, lru[1]);

  ParentCleanup();
}

void SharedMemCacheDataTestBase::TestBlockLists() {
  AbstractSharedMemSegment* seg_raw_ptr = NULL;
  Sector<kBlockSize>* sector_raw_ptr = NULL;
  ASSERT_TRUE(ParentInit(&seg_raw_ptr, &sector_raw_ptr));
  scoped_ptr<AbstractSharedMemSegment> seg(seg_raw_ptr);
  scoped_ptr<Sector<kBlockSize> > sector(sector_raw_ptr);

  // First, let's sanity-check the computation routines
  EXPECT_EQ(static_cast<size_t>(0),
            Sector<kBlockSize>::DataBlocksForSize(0));
  EXPECT_EQ(static_cast<size_t>(1),
            Sector<kBlockSize>::DataBlocksForSize(1));
  EXPECT_EQ(static_cast<size_t>(1),
            Sector<kBlockSize>::DataBlocksForSize(kBlockSize));
  EXPECT_EQ(static_cast<size_t>(2),
            Sector<kBlockSize>::DataBlocksForSize(kBlockSize + 1));
  EXPECT_EQ(static_cast<size_t>(2),
            Sector<kBlockSize>::DataBlocksForSize(kBlockSize * 2));
  EXPECT_EQ(static_cast<size_t>(3),
            Sector<kBlockSize>::DataBlocksForSize(kBlockSize * 2 + 1));

  EXPECT_EQ(static_cast<size_t>(1),
            Sector<kBlockSize>::BytesInPortion(1, 0, 1));
  EXPECT_EQ(static_cast<size_t>(kBlockSize - 1),
            Sector<kBlockSize>::BytesInPortion(kBlockSize - 1, 0, 1));
  EXPECT_EQ(static_cast<size_t>(kBlockSize),
            Sector<kBlockSize>::BytesInPortion(kBlockSize, 0, 1));

  EXPECT_EQ(static_cast<size_t>(kBlockSize),
            Sector<kBlockSize>::BytesInPortion(kBlockSize + 1, 0, 2));
  EXPECT_EQ(static_cast<size_t>(1),
            Sector<kBlockSize>::BytesInPortion(kBlockSize + 1, 1, 2));

  EXPECT_EQ(static_cast<size_t>(kBlockSize),
            Sector<kBlockSize>::BytesInPortion(2 * kBlockSize, 0, 2));
  EXPECT_EQ(static_cast<size_t>(kBlockSize),
            Sector<kBlockSize>::BytesInPortion(2 * kBlockSize, 1, 2));

  // Now, let's allocate some blocks.
  const int kTestBlocks = 10;
  BlockVector blocks;
  ASSERT_EQ(kTestBlocks,
            sector->AllocBlocksFromFreeList(kTestBlocks, &blocks));

  // Link them together, and add them to a test entry.
  sector->LinkBlockSuccessors(blocks);
  CacheEntry* entry = sector->EntryAt(0);
  entry->byte_size = kTestBlocks * kBlockSize;
  entry->first_block = blocks[0];

  // Hopefully BlockListForEntry will look things up right.
  BlockVector extracted_blocks;
  sector->BlockListForEntry(entry, &extracted_blocks);
  EXPECT_EQ(blocks, extracted_blocks);

  ParentCleanup();
}

bool SharedMemCacheDataTestBase::ParentInit(AbstractSharedMemSegment** out_seg,
                                            Sector<kBlockSize>** out_sector) {
  size_t bytes =
      Sector<kBlockSize>::RequiredSize(shmem_runtime_.get(), kEntries, kBlocks);
  AbstractSharedMemSegment* seg =
      shmem_runtime_->CreateSegment(kSegment, bytes + kExtra, &handler_);
  if (seg == NULL) {
    return false;
  }

  Sector<kBlockSize>* sector =
      new Sector<kBlockSize>(seg, kExtra, kEntries, kBlocks);
  *out_seg = seg;
  *out_sector = sector;

  return sector->Initialize(&handler_);
}

bool SharedMemCacheDataTestBase::ChildInit(AbstractSharedMemSegment** out_seg,
                                           Sector<kBlockSize>** out_sector) {
  size_t bytes =
      Sector<kBlockSize>::RequiredSize(shmem_runtime_.get(), kEntries, kBlocks);
  AbstractSharedMemSegment* seg =
      shmem_runtime_->AttachToSegment(kSegment, bytes + kExtra, &handler_);
  if (seg == NULL) {
    return false;
  }

  Sector<kBlockSize>* sector =
      new Sector<kBlockSize>(seg, kExtra, kEntries, kBlocks);
  *out_seg = seg;
  *out_sector = sector;

  return sector->Attach(&handler_);
}

void SharedMemCacheDataTestBase::ParentCleanup() {
  test_env_->WaitForChildren();
  shmem_runtime_->DestroySegment(kSegment, &handler_);
}

}  // namespace net_instaweb
