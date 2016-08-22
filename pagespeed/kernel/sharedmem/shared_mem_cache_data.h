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

#ifndef PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_CACHE_DATA_H_
#define PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_CACHE_DATA_H_

#include <cstddef>                     // for size_t
#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread_annotations.h"

namespace net_instaweb {

class AbstractMutex;
class AbstractSharedMem;
class AbstractSharedMemSegment;
class MessageHandler;

namespace SharedMemCacheData {

typedef int32 EntryNum;
typedef int32 BlockNum;
typedef std::vector<BlockNum> BlockVector;

const BlockNum kInvalidBlock = -1;
const EntryNum kInvalidEntry = -1;
const size_t kHashSize = 16;

struct SectorStats {
  SectorStats();

  // FS operation stats --- updated by SharedMemCache. We do it
  // this way rather than using the normal Statistics interface
  // to avoid having to worry about extra synchronization inside
  // critical sections --- since we already hold sector locks
  // when doing this stuff, it's easy to update per-sector data.
  // TODO(morlovich): Consider periodically pushing these to
  // normal Statistics.
  int64 num_put;
  int64 num_put_update;  // update of the same key
  int64 num_put_replace;  // replacement of different key
  int64 num_put_concurrent_create;
  int64 num_put_concurrent_full_set;
  int64 num_put_spins;  // # of times writers had to sleep behind readers
  int64 num_get;    // # of calls to get
  int64 num_get_hit;
  int64 last_checkpoint_ms;  // When this sector was last checkpointed to disk.

  // Current state stats --- updated by SharedMemCacheData
  int64 used_entries;
  int64 used_blocks;

  // Adds number to this object's. No concurrency control is done.
  void Add(const SectorStats& other);

  // Text dump of the statistics. No concurrency control is done.
  GoogleString Dump(size_t total_entries, size_t total_blocks) const;
};

struct SectorHeader {
  BlockNum free_list_front;
  EntryNum lru_list_front;
  EntryNum lru_list_rear;
  int32 padding;

  SectorStats stats;

  // mutex goes here.
};

struct CacheEntry {
  char hash_bytes[kHashSize];
  int64 last_use_timestamp_ms;
  int32 byte_size;

  // For LRU list, prev/next are kInvalidEntry to denote 'none', which
  // can apply both at endpoints and for entries not in the LRU at all,
  // due to being free.
  EntryNum lru_prev;
  EntryNum lru_next;

  BlockNum first_block;

  // When this is true, someone is trying to overwrite this entry.
  bool creating : 1;

  // Number of readers currently accessing the data.
  uint32 open_count : 31;

  uint32 padding;  // ensures we're 8-aligned.
};

// Helper for operating on a given sector's data structures; helping
// access them, lay them out in memory, and initialize them. It does not
// implement the actual cache operations, however. In particular, its
// methods affect only a single data structure at the time and do not
// do anything to preserve cross-structure invariants.
template<size_t kBlockSize>
class Sector {
 public:
  // Creates a wrapper to help operate on cache sectors in a given region of
  // memory with given geometry.  The sector should have had as much memory
  // allocated for it as returned by a call to RequiredSize with the same
  // arguments.
  //
  // Note that this doesn't do any imperative initialization; you must
  // call Initialize() in the parent process, and Attach() in child processes,
  // and check their results as well. Also, segment is assumed to be owned
  // separately, with lifetime longer than ours.
  Sector(AbstractSharedMemSegment* segment, size_t sector_offset,
         size_t cache_entries, size_t data_blocks);
  ~Sector();

  // This should be called from child processes to initialize client
  // state for the cache already formatted by a call to Initialize() in
  // the parent.
  //
  // Returns if successful (which it should be if the parent process
  // successfully create the memory and Initialize()'d it).
  bool Attach(MessageHandler* handler);

  // This should be called from the initial/parent process before the children
  // start. It initializes the data structures in this sector, including
  // mutexes. Returns true on success.
  bool Initialize(MessageHandler* handler);

  // Computes how much memory a sector will need for given number of entries.
  // Also makes sure it's padded to proper alignment.
  static size_t RequiredSize(AbstractSharedMem* shmem_runtime,
                             size_t cache_entries, size_t data_blocks);

  // Mutex ops.

  // The sector lock should be held while doing any metadata accesses.
  AbstractMutex* mutex() const LOCK_RETURNED(mutex_) { return mutex_.get(); }

  // Block successor list ops.
  // ------------------------------------------------------------
  BlockNum GetBlockSuccessor(BlockNum block) EXCLUSIVE_LOCKS_REQUIRED(mutex()) {
    DCHECK_GE(block, 0);
    DCHECK_LT(block, static_cast<BlockNum>(data_blocks_));
    return block_successors_[block];
  }

  void SetBlockSuccessor(BlockNum block, BlockNum next)
      EXCLUSIVE_LOCKS_REQUIRED(mutex()) {
    DCHECK_GE(block, 0);
    DCHECK_LT(block, static_cast<BlockNum>(data_blocks_));

    DCHECK_GE(next, kInvalidBlock);
    DCHECK_LT(next, static_cast<BlockNum>(data_blocks_));

    block_successors_[block] = next;
  }

  // Links blocks in the vector in order, with later blocks being
  // marked as successors of later ones.
  void LinkBlockSuccessors(const BlockVector& blocks)
      EXCLUSIVE_LOCKS_REQUIRED(mutex()) {
    for (size_t pos = 0; pos < blocks.size(); ++pos) {
      if (pos == (blocks.size() - 1)) {
        SetBlockSuccessor(blocks[pos], kInvalidBlock);
      } else {
        SetBlockSuccessor(blocks[pos], blocks[pos + 1]);
      }
    }
  }

  // Freelist ops.
  // ------------------------------------------------------------

  // Allocates as close to the goal blocks from freelist as it can, and
  // appends their numbers to blocks. Returns how much it allocated.
  // Does not adjust block successor lists.
  //
  // Note that this doesn't attempt to free blocks that are in use by some
  // entries.
  int AllocBlocksFromFreeList(int goal, BlockVector* blocks)
      EXCLUSIVE_LOCKS_REQUIRED(mutex());

  // Puts all the passed in blocks onto this sector's freelist.
  // Does not read successors for passed in blocks, but does set them
  // for freelist membership.
  void ReturnBlocksToFreeList(const BlockVector& blocks)
      EXCLUSIVE_LOCKS_REQUIRED(mutex());

  // Cache directory ops.
  // ------------------------------------------------------------

  // Returns the given # entry.
  CacheEntry* EntryAt(EntryNum slot) {
    return reinterpret_cast<CacheEntry*>(directory_base_) + slot;
  }

  // Inserts the given entry into the LRU, at front.
  // Precondition: must not be in LRU.
  void InsertEntryIntoLRU(EntryNum entry_num);

  // Removes from the LRU. Safe to call if not in the LRU already.
  void UnlinkEntryFromLRU(EntryNum entry_num);

  EntryNum OldestEntryNum() {
    return sector_header_->lru_list_rear;
  }

  // Block ops.
  // ------------------------------------------------------------

  char* BlockBytes(BlockNum block_num) {
    return blocks_base_ + kBlockSize * block_num;
  }

  // Ops for lists of blocks corresponding to each directory entry,
  // and related size computations
  // ------------------------------------------------------------

  // Number of blocks of data needed for size blocks.
  static size_t DataBlocksForSize(size_t size) {
    return NeededPieces(size, kBlockSize);
  }

  // The # of bytes stored in block 'b' out of 'total' blocks for file of size
  // 'total_bytes'.
  // Precondition: 'total' is appropriate for 'total_bytes'.
  static size_t BytesInPortion(size_t total_bytes, size_t b, size_t total);

  // Appends the list of blocks used by the entry to 'blocks'.
  // Returns the number of items appended.
  int BlockListForEntry(CacheEntry* entry, BlockVector* out_blocks)
      EXCLUSIVE_LOCKS_REQUIRED(mutex());

  // Statistics stuff
  // ------------------------------------------------------------

  SectorStats* sector_stats() { return &sector_header_->stats; }

  // Prints out all statistics in the header (some of which are maintained
  // by the higher-level)
  void DumpStats(MessageHandler* handler);

 private:
  // Helper for doing sizing/memory layout computations.
  struct MemLayout;

  // How many piece_size pieces suffice to fit total
  static size_t NeededPieces(size_t total, size_t piece_size) {
    return (total + piece_size - 1) / piece_size;
  }

  // Configured geometry
  size_t cache_entries_;
  size_t data_blocks_;

  // Pointers to where various things are, and our sizes
  AbstractSharedMemSegment* segment_;
  scoped_ptr<AbstractMutex> mutex_;
  SectorHeader* sector_header_;
  BlockNum* block_successors_ PT_GUARDED_BY(mutex());
  char* directory_base_;
  char* blocks_base_;
  size_t sector_offset_;  // offset of the sector within the SHM segment

  DISALLOW_COPY_AND_ASSIGN(Sector);
};

}  // namespace SharedMemCacheData

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_CACHE_DATA_H_
