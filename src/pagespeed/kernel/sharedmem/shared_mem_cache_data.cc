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

// Author: morlovich@google.com (Maksim Orlovich)

// Data structure operation helpers for SharedMemCache. See the top of
// shared_mem_cache.cc for data format descriptions.

#include "pagespeed/kernel/sharedmem/shared_mem_cache_data.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/abstract_shared_mem.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

namespace SharedMemCacheData {

namespace {
// Assumes align is power-of-2
inline size_t AlignTo(size_t alignment, size_t in) {
  return (in + (alignment - 1)) & ~(alignment - 1);
}

double percent(int64 portion, int64 total) {
  return static_cast<double>(portion) / static_cast<double>(total) * 100.0;
}

}  // namespace

template<size_t kBlockSize>
struct Sector<kBlockSize>::MemLayout {
  MemLayout(size_t mutex_size, size_t cache_entries, size_t data_blocks) {
    // Check out alignment assumptions -- everything must be of a size
    // that's multiple of 8. The exact sizes don't matter too much, but
    // we check it anyway to avoid surprises.
    CHECK_EQ(96u, sizeof(SectorHeader));
    CHECK_EQ(48u, sizeof(CacheEntry));

    header_bytes = AlignTo(8, sizeof(SectorHeader) + mutex_size);
    block_successor_list_bytes =
        AlignTo(8, sizeof(BlockNum) * data_blocks);
    size_t directory_size = sizeof(CacheEntry) * cache_entries;
    metadata_bytes =
        AlignTo(kBlockSize,
                header_bytes + directory_size + block_successor_list_bytes);
  }

  size_t header_bytes;  // also offset to the block successor list.
  size_t block_successor_list_bytes;
  size_t metadata_bytes;  // e.g. offset to the blocks.
};

template<size_t kBlockSize>
Sector<kBlockSize>::Sector(AbstractSharedMemSegment* segment,
                           size_t sector_offset, size_t cache_entries,
                           size_t data_blocks)
    : cache_entries_(cache_entries),
      data_blocks_(data_blocks),
      segment_(segment),
      sector_offset_(sector_offset) {
  MemLayout layout(segment->SharedMutexSize(), cache_entries, data_blocks);
  char* base = const_cast<char*>(segment->Base()) + sector_offset;
  sector_header_ = reinterpret_cast<SectorHeader*>(base);
  block_successors_ = reinterpret_cast<BlockNum*>(base + layout.header_bytes);
  directory_base_ =
      base + layout.header_bytes + layout.block_successor_list_bytes;
  blocks_base_ = base + layout.metadata_bytes;
}

template<size_t kBlockSize>
Sector<kBlockSize>::~Sector() {
}

template<size_t kBlockSize>
bool Sector<kBlockSize>::Attach(MessageHandler* handler) {
  mutex_.reset(
      segment_->AttachToSharedMutex(sector_offset_ + sizeof(SectorHeader)));
  return (mutex_.get() != NULL);
}

template<size_t kBlockSize>
bool Sector<kBlockSize>::Initialize(MessageHandler* handler)
    NO_THREAD_SAFETY_ANALYSIS {
  if (!segment_->InitializeSharedMutex(sector_offset_ + sizeof(SectorHeader),
                                       handler)) {
    return false;
  }

  // Connect to the mutex, now it's created.
  if (!Attach(handler)) {
    return false;
  }

  // Initialize the LRU and the cache entry.
  sector_header_->lru_list_front = kInvalidEntry;
  sector_header_->lru_list_rear = kInvalidEntry;
  for (size_t c = 0; c < cache_entries_; ++c) {
    CacheEntry* entry = EntryAt(c);
    entry->lru_prev = kInvalidEntry;
    entry->lru_next = kInvalidEntry;
    entry->first_block = kInvalidBlock;
  }

  // Initialize the freelist and block successor list.
  sector_header_->free_list_front = kInvalidBlock;
  BlockVector all_blocks;
  for (size_t c = 0; c < data_blocks_; ++c) {
    all_blocks.push_back(static_cast<BlockNum>(c));
  }
  ReturnBlocksToFreeList(all_blocks);
  sector_header_->stats.used_blocks = 0;

  return true;
}

template<size_t kBlockSize>
size_t Sector<kBlockSize>::RequiredSize(AbstractSharedMem* shmem_runtime,
                                        size_t cache_entries,
                                        size_t data_blocks) {
  MemLayout layout(shmem_runtime->SharedMutexSize(), cache_entries,
                   data_blocks);
  return layout.metadata_bytes + data_blocks * kBlockSize;
}

template<size_t kBlockSize>
int Sector<kBlockSize>::AllocBlocksFromFreeList(int goal,
                                                BlockVector* blocks) {
  int allocated = 0;
  while ((allocated < goal) &&
          (sector_header_->free_list_front != kInvalidBlock)) {
    int block_num = sector_header_->free_list_front;
    sector_header_->free_list_front = GetBlockSuccessor(block_num);
    blocks->push_back(block_num);
    ++allocated;
  }
  sector_header_->stats.used_blocks += allocated;
  return allocated;
}

template<size_t kBlockSize>
void Sector<kBlockSize>::ReturnBlocksToFreeList(const BlockVector& blocks) {
  for (size_t c = 0; c < blocks.size(); ++c) {
    BlockNum block_num = blocks[c];
    SetBlockSuccessor(block_num, sector_header_->free_list_front);
    sector_header_->free_list_front = block_num;
  }
  sector_header_->stats.used_blocks -= blocks.size();
};

template<size_t kBlockSize>
void Sector<kBlockSize>::InsertEntryIntoLRU(EntryNum entry_num) {
  CacheEntry* entry = EntryAt(entry_num);
  CHECK((entry->lru_prev == kInvalidEntry) &&
        (entry->lru_next == kInvalidEntry));
  ++sector_header_->stats.used_entries;
  entry->lru_next = sector_header_->lru_list_front;
  if (entry->lru_next == kInvalidEntry) {
    sector_header_->lru_list_rear = entry_num;
  } else {
    EntryAt(entry->lru_next)->lru_prev = entry_num;
  }
  sector_header_->lru_list_front = entry_num;
}

template<size_t kBlockSize>
void Sector<kBlockSize>::UnlinkEntryFromLRU(int entry_num) {
  CacheEntry* entry = EntryAt(entry_num);

  // TODO(morlovich): again, perhaps a bit too much work for stats.
  if (entry->lru_next != kInvalidEntry ||
      entry->lru_prev != kInvalidEntry ||
      sector_header_->lru_list_front == entry_num) {
    --sector_header_->stats.used_entries;
  }

  // Update successor or rear pointer.
  if (entry->lru_next == kInvalidEntry) {
    // Either at end or not linked-in at all.
    if (entry_num == sector_header_->lru_list_rear) {
      sector_header_->lru_list_rear = entry->lru_prev;
    }
  } else {
    EntryAt(entry->lru_next)->lru_prev = entry->lru_prev;
  }

  // Update predecessor or front pointer.
  if (entry->lru_prev == kInvalidEntry) {
    // Front or not linked-in at all.
    if (entry_num == sector_header_->lru_list_front) {
      sector_header_->lru_list_front = entry->lru_next;
    }
  } else {
    EntryAt(entry->lru_prev)->lru_next = entry->lru_next;
  }

  // clear own links.
  entry->lru_prev = kInvalidEntry;
  entry->lru_next = kInvalidEntry;
}

template<size_t kBlockSize>
size_t Sector<kBlockSize>::BytesInPortion(size_t total_bytes, size_t b,
                                          size_t total) {
  if (b != (total -1)) {
    return kBlockSize;
  } else {
    size_t rem = total_bytes % kBlockSize;
    return (rem == 0) ? kBlockSize : rem;
  }
}

template<size_t kBlockSize>
int Sector<kBlockSize>::BlockListForEntry(CacheEntry* entry,
                                          BlockVector* out_blocks) {
  int data_blocks = DataBlocksForSize(entry->byte_size);

  BlockNum block = entry->first_block;
  for (int d = 0; d < data_blocks; ++d) {
    DCHECK_LE(0, block);
    DCHECK_LT(block, static_cast<BlockNum>(data_blocks_));
    out_blocks->push_back(block);
    block = GetBlockSuccessor(block);
  }
  DCHECK_EQ(block, kInvalidBlock);

  return data_blocks;
}

SectorStats::SectorStats()
    : num_put(0),
      num_put_update(0),
      num_put_replace(0),
      num_put_concurrent_create(0),
      num_put_concurrent_full_set(0),
      num_put_spins(0),
      num_get(0),
      num_get_hit(0),
      used_entries(0),
      used_blocks(0) {
}

void SectorStats::Add(const SectorStats& other) {
  num_put += other.num_put;
  num_put_update += other.num_put_update;
  num_put_replace += other.num_put_replace;
  num_put_concurrent_create += other.num_put_concurrent_create;
  num_put_concurrent_full_set += other.num_put_concurrent_full_set;
  num_put_spins += other.num_put_spins;
  num_get += other.num_get;
  num_get_hit += other.num_get_hit;
  used_entries += other.used_entries;
  used_blocks += other.used_blocks;
}

GoogleString SectorStats::Dump(size_t total_entries,
                               size_t total_blocks) const {
  GoogleString out;
  StringAppendF(&out, "Total put operations: %s\n",
                Integer64ToString(num_put).c_str());
  StringAppendF(&out, "  updating an existing key: %s\n",
                Integer64ToString(num_put_update).c_str());
  StringAppendF(&out, "  replace/conflict miss: %s\n",
                Integer64ToString(num_put_replace).c_str());
  StringAppendF(
      &out, "  simultaneous same-key insert: %s\n",
      Integer64ToString(num_put_concurrent_create).c_str());
  StringAppendF(
      &out, "  dropped since all of associativity set locked: %s\n",
      Integer64ToString(num_put_concurrent_full_set).c_str());
  StringAppendF(
      &out, "  spinning sleeps performed by writers: %s\n",
      Integer64ToString(num_put_spins).c_str());

  StringAppendF(&out, "Total get operations: %s\n",
                Integer64ToString(num_get).c_str());
  StringAppendF(&out, "  hits: %s (%.2f%%)\n",
                Integer64ToString(num_get_hit).c_str(),
                percent(num_get_hit, num_get));

  StringAppendF(&out, "Entries used: %s (%.2f%%)\n",
                Integer64ToString(used_entries).c_str(),
                percent(used_entries, total_entries));
  StringAppendF(&out, "Blocks used: %s (%.2f%%)\n",
                Integer64ToString(used_blocks).c_str(),
                percent(used_blocks, total_blocks));
  return out;
}

template<size_t kBlockSize>
void Sector<kBlockSize>::DumpStats(MessageHandler* handler) {
  mutex()->Lock();
  GoogleString dump = sector_stats()->Dump(cache_entries_, data_blocks_);
  mutex()->Unlock();
  handler->Message(kError, "%s", dump.c_str());
}

template class Sector<64>;
template class Sector<512>;
template class Sector<4096>;

}  // namespace SharedMemCacheData

}  // namespace net_instaweb
