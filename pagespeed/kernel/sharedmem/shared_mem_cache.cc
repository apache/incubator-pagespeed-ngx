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

// This module provides a shared memory cache implementation.
//
// ----------------------------------------------------------------------------
// Cache data structures and memory layout.
// ----------------------------------------------------------------------------
//
// We first re-key everything via cryptographic (or other 'very very good')
// hashes by using a Hasher object, so the key bits are very random.
//
// The cache is partitioned into sectors. The sectors are completely independent
// --- no operation ever touches more than one, and keys are statically
// partitioned between them.
//
// When we access an entry, we first select a sector number based off its key,
// and then within the sector we choose kAssociativity (4) possible
// directory entries storing it, and the appropriate directory entry then
// points to some number of blocks containing the object's payload.
//
// In each sector we store:
//
// 1) Freelist --- block number of a free block, or -1 (kInvalidBlock) if there
//    are none. Further free blocks are linked via the block successor list.
//
// 2) LRU front/rear links into the cache directory.
//
// 3) Various statistics (see struct SectorStats in shared_mem_cache_data.h
//    for the list)
//
// Padding to align to 8.
//
// 4) Sector mutex that is used to protect the metadata (but is released while
//    copying the payload).
//
// Padding to align to 8.
//
// 5) Block successor list. This is used to both link the blocks in each file
//    and to link together the freelist. This is encoded as an array, indexed
//    by block number, with values being successors, or -1 (kInvalidBlock)
//    for end of list.
//
// Padding to align to 8.
//
// 6) The cache directory. This is an array of CacheEntry structures.
//    (But note that the size of the hash portion is dependent on the Hasher;
//     and the struct is padded to be 8-aligned).
//
// Padding to align to block size.
//
// 7) The data blocks. These contain the actual payload.
//
// ----------------------------------------------------------------------------
// Cache directory usage
// ----------------------------------------------------------------------------
//
// Presently we operate in a 4-way skew associative fashion:
// each key determines 4 (very rarely identical) positions in the directory
// that may be used to store it. We check both for lookup/overwrite, and use
// timestamps to determine replacement candidates. (Experiments have shown that
// 2-way produced way too many extra conflicts).
//
// ----------------------------------------------------------------------------
// Cache entry format
// ----------------------------------------------------------------------------
//
// The hash_bytes field contains the object key. As noted above, its
// length may vary with the hasher in use.
//
// last_use_timestamp_ms denotes when the entry was last touched, for
// associativity replacement.
//
// byte_size is the size of the actual payload in bytes (not counting
// internal fragmentation or our bookkeeping overhead).
//
// lru_next/lru_prev are used to form an inline doubly-linked LRU chain
// of non-free entries in case we need to free up some blocks on insertion
// because the freelist doesn't have enough.
//
// first_block is the block number of the first block (kInvalidBlock if the
// entry is 0-byte or not used for data). Later blocks can be found by
// following the block successor list.
//
// TODO(morlovich): What if we try to outline a 500MiB file, which would make
// ::Put() fail, but the filter would proceeds anyway as it has no way of
// knowing?
//
// creating and open_count are used to lock the particular entry for
// reading or writing while the main sector lock is released.
//
// The following are the possible combinations:
// Creating?  Open_count
// False      0           Entry unlocked --- can read, write, etc. freely
// False      n > 0       n processes reading. More readers can freely join.
// True       n > 0       n processes reading. Writer waiting for them to
//                          finish. Readers can't join.
// True       0           Writer working.
//
// For now, writers wait in sleep loop, while readers simply fail/miss.
//
// TODO(morlovich): Evaluate using chaining and one more layer of indirection
// instead, as it should hopefully produce much better utilization and avoid
// conflict misses entirely.

#include "pagespeed/kernel/sharedmem/shared_mem_cache.h"

#include <cstddef>                     // for size_t
#include <cstring>
#include <map>
#include <vector>
#include <utility>                      // for pair

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/abstract_shared_mem.h"
#include "pagespeed/kernel/base/base64_util.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/proto_util.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/sharedmem/shared_mem_cache_data.h"
#include "pagespeed/kernel/sharedmem/shared_mem_cache_snapshot.pb.h"
#include "pagespeed/kernel/thread/slow_worker.h"

namespace net_instaweb {

using SharedMemCacheData::BlockNum;
using SharedMemCacheData::BlockVector;
using SharedMemCacheData::CacheEntry;
using SharedMemCacheData::EntryNum;
using SharedMemCacheData::Sector;
using SharedMemCacheData::SectorStats;
using SharedMemCacheData::kInvalidBlock;
using SharedMemCacheData::kInvalidEntry;
using SharedMemCacheData::kHashSize;

namespace {

// Increase this number if making backwards incompatible changes to the dump
// format.
const int kSnapshotVersion = 1;

bool IsAllNil(const StringPiece& raw_hash) {
  bool all_nil = true;
  for (size_t c = 0; c < raw_hash.length(); ++c) {
    if (raw_hash[c] != '\0') {
      all_nil = false;
      break;
    }
  }
  return all_nil;
}

GoogleString FormatSize(size_t size) {
  return Integer64ToString(static_cast<int64>(size));
}

// A couple of debug helpers.
#ifndef NDEBUG

GoogleString DebugTextHash(const GoogleString& raw_hash) {
  GoogleString out;
  Web64Encode(raw_hash, &out);
  return out;
}

GoogleString DebugTextHash(CacheEntry* entry, size_t size) {
  return DebugTextHash(StringPiece(entry->hash_bytes, size).as_string());
}

#endif  // NDEBUG

}  // namespace

// If you add any new parameters also include them in SnapshotCacheKey() or else
// people will restore invalid snapshots and have a corrupt cache.
template<size_t kBlockSize>
SharedMemCache<kBlockSize>::SharedMemCache(
    AbstractSharedMem* shm_runtime, const GoogleString& filename,
    Timer* timer, const Hasher* hasher, int sectors, int entries_per_sector,
    int blocks_per_sector, MessageHandler* handler)
    : shm_runtime_(shm_runtime),
      hasher_(hasher),
      timer_(timer),
      filename_(filename),
      num_sectors_(sectors),
      entries_per_sector_(entries_per_sector),
      blocks_per_sector_(blocks_per_sector),
      checkpoint_interval_sec_(-1),
      handler_(handler),
      snapshot_path_(""),
      file_cache_(NULL) {
}

template<size_t kBlockSize>
GoogleString SharedMemCache<kBlockSize>::FormatName() {
  return StringPrintf("SharedMemCache<%d>", static_cast<int>(kBlockSize));
}

template<size_t kBlockSize>
SharedMemCache<kBlockSize>::~SharedMemCache() {
  STLDeleteElements(&sectors_);
}

template<size_t kBlockSize>
bool SharedMemCache<kBlockSize>::InitCache(bool parent) {
  size_t sector_size =
      Sector<kBlockSize>::RequiredSize(shm_runtime_, entries_per_sector_,
                                       blocks_per_sector_);
  size_t size = num_sectors_ * sector_size;

  if (parent) {
    segment_.reset(shm_runtime_->CreateSegment(filename_, size, handler_));
  } else {
    segment_.reset(shm_runtime_->AttachToSegment(filename_, size, handler_));
  }

  if (segment_.get() == NULL) {
    handler_->Message(
        kError, "SharedMemCache: can't %s segment %s of size %s",
        parent ? "create" : "attach",
        filename_.c_str(), FormatSize(size).c_str());
    return false;
  }

  STLDeleteElements(&sectors_);
  sectors_.clear();
  for (int s = 0; s < num_sectors_; ++s) {
    scoped_ptr<Sector<kBlockSize> > sec(
        new Sector<kBlockSize>(segment_.get(), s * sector_size,
                               entries_per_sector_, blocks_per_sector_));
    bool ok;
    if (parent) {
      ok = sec->Initialize(handler_);
    } else {
      ok = sec->Attach(handler_);
    }

    if (!ok) {
      handler_->Message(
          kError, "SharedMemCache: can't %s sector %d of cache %s",
          parent ? "create" : "attach", s, filename_.c_str());
      return false;
    }
    sectors_.push_back(sec.release());
  }

  if (parent) {
    handler_->Message(
      kInfo, "SharedMemCache: %s, sectors = %d, entries/sector = %d, "
      " %d-byte blocks/sector = %d, total footprint: %s", filename_.c_str(),
      num_sectors_, entries_per_sector_, static_cast<int>(kBlockSize),
      blocks_per_sector_, FormatSize(size).c_str());
  }
  return true;
}

template<size_t kBlockSize>
bool SharedMemCache<kBlockSize>::Initialize() {
  bool ok = InitCache(true);
  if (ok) {
    RestoreFromDisk();
  }
  return ok;
}

template<size_t kBlockSize>
bool SharedMemCache<kBlockSize>::Attach() {
  return InitCache(false);
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::GlobalCleanup(
    AbstractSharedMem* shm_runtime, const GoogleString& filename,
    MessageHandler* message_handler) {
  shm_runtime->DestroySegment(filename, message_handler);
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::ComputeDimensions(
    int64 size_kb,
    int block_entry_ratio,
    int sectors,
    int* entries_per_sector_out,
    int* blocks_per_sector_out,
    int64* size_cap_out) {
  int64 size = size_kb * 1024;
  const int kEntrySize = sizeof(CacheEntry);
  // Footprint of an entry is kEntrySize bytes. Block is kBlockSize + 4
  // bytes for successor list. We ignore sector headers for the math since
  // negligible. So:
  //
  // Size = (kBlockSize + 4) * Blocks + kEntrySize * Entries
  //      = (kBlockSize + 4) * (Blocks/Entries) * Entries + kEntrySize * Entries
  // Entries = Size / ((kBlockSize + 4) * (Blocks/Entries) + kEntrySize)
  // Blocks  = Entries * (Blocks/Entries)
  //
  // We also divide things up into some number of sectors to lower contention,
  // which reduces 'size' proportionally.
  size /= sectors;
  *entries_per_sector_out =
      size / ((kBlockSize + 4) * block_entry_ratio + kEntrySize);
  *blocks_per_sector_out = *entries_per_sector_out * block_entry_ratio;

  // The cache sets a size cap of 1/8'th of a sector for object size; let our
  // client know.
  *size_cap_out = *blocks_per_sector_out * kBlockSize / 8;
}

template<size_t kBlockSize>
GoogleString SharedMemCache<kBlockSize>::DumpStats() {
  SectorStats aggregate;
  for (size_t c = 0; c < sectors_.size(); ++c) {
    ScopedMutex lock(sectors_[c]->mutex());
    aggregate.Add(*sectors_[c]->sector_stats());
  }

  return aggregate.Dump(entries_per_sector_* num_sectors_,
                        blocks_per_sector_ * num_sectors_);
}

template<size_t kBlockSize>
bool SharedMemCache<kBlockSize>::AddSectorToSnapshot(
    int sector_num, int64 last_checkpoint_ms, SharedMemCacheDump* dest) {
  CHECK_LE(0, sector_num);
  CHECK_LT(sector_num, num_sectors_);

  Sector<kBlockSize>* sector = sectors_[sector_num];
  SectorStats* stats = sector->sector_stats();
  ScopedMutex lock(sector->mutex());
  DCHECK(!(last_checkpoint_ms > stats->last_checkpoint_ms));
  if (last_checkpoint_ms < stats->last_checkpoint_ms) {
    // Another thread already snapshotted this sector; do nothing.
    return false;
  }

  EntryNum cur = sector->OldestEntryNum();
  while (cur != kInvalidEntry) {
    CacheEntry* cur_entry = sector->EntryAt(cur);

    // It's possible that the sector got unlocked while a Put is
    // updating the payload for an entry. In that case, the entry will
    // have its creating bit set (but the metadata will be valid).
    // We skip those.
    if (!cur_entry->creating) {
      SharedMemCacheDumpEntry* dump_entry = dest->add_entry();
      dump_entry->set_raw_key(cur_entry->hash_bytes, kHashSize);
      dump_entry->set_last_use_timestamp_ms(cur_entry->last_use_timestamp_ms);

      // Gather value.
      BlockVector blocks;
      sector->BlockListForEntry(cur_entry, &blocks);

      size_t total_blocks = blocks.size();
      for (size_t b = 0; b < total_blocks; ++b) {
        int bytes = sector->BytesInPortion(cur_entry->byte_size, b,
                                           total_blocks);
        dump_entry->mutable_value()->append(
            sector->BlockBytes(blocks[b]), bytes);
      }
    }
    cur = cur_entry->lru_prev;
  }

  stats->last_checkpoint_ms = timer_->NowMs();
  return true;
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::RestoreSnapshot(
    const SharedMemCacheDump& dump) {
  for (int i = 0; i < dump.entry_size(); ++i) {
    const SharedMemCacheDumpEntry& entry = dump.entry(i);

    // The code below assumes that the raw hash is the right size, so make sure
    // to detect this particular corruption to avoid crashing.
    if (entry.raw_key().size() != kHashSize) {
      return;
    }

    SharedString value(entry.value());
    PutRawHash(entry.raw_key(), entry.last_use_timestamp_ms(), value,
               false /* don't trigger checkpointing */);
  }
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::MarshalSnapshot(
    const SharedMemCacheDump& dump, GoogleString* out) {
  out->clear();
  StringOutputStream sstream(out);  // finalizes *out in destructor
  dump.SerializeToZeroCopyStream(&sstream);
}


template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::DemarshalSnapshot(
    const StringPiece& marshaled, SharedMemCacheDump* out) {
  ArrayInputStream input(marshaled.data(), marshaled.size());
  out->ParseFromZeroCopyStream(&input);
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::Put(const GoogleString& key,
                                     const SharedString& value) {
  int64 now_ms = timer_->NowMs();
  GoogleString raw_hash = ToRawHash(key);
  PutRawHash(raw_hash, now_ms, value, true /* may trigger checkpointing */);
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>:: ScheduleSnapshotIfNecessary(
    bool checkpoint_ok, int64 last_use_timestamp_ms, int64 last_checkpoint_ms,
    int sector_num) {
  if (checkpoint_ok /* not restoring, ok to checkpoint */ &&
      checkpoint_interval_sec_ > 0 /* checkpointing enabled */) {
    int64 now_ms = last_use_timestamp_ms;

    if (now_ms - last_checkpoint_ms >
        (checkpoint_interval_sec_ * Timer::kSecondMs)) {
      ScheduleSnapshot(sector_num, last_checkpoint_ms);
    }
  }
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::PutRawHash(const GoogleString& raw_hash,
                                            int64 last_use_timestamp_ms,
                                            const SharedString& value,
                                            bool checkpoint_ok) {
  // See also ::ComputeDimensions
  const size_t kMaxSize = MaxValueSize();

  size_t value_size = static_cast<size_t>(value.size());
  if (value_size > kMaxSize) {
    handler_->Message(
      kInfo, "Unable to insert object of size: %s, cache limit is: %s",
      FormatSize(value.size()).c_str(),
      FormatSize(kMaxSize).c_str());
    return;
  }

  Position pos;
  ExtractPosition(raw_hash, &pos);

  Sector<kBlockSize>* sector = sectors_[pos.sector];
  SectorStats* stats = sector->sector_stats();

  ScopedMutex lock(sector->mutex());
  ++stats->num_put;
  int64 last_checkpoint_ms = stats->last_checkpoint_ms;

  // See if our key already exists. Note that if it does, we will attempt to
  // write even if there are readers (we will wait for them to finish);
  // but not if there is another writer, in which case we just give up.
  // It is important, however, that we always exit if the key matches,
  // so we don't end up creating a second copy!
  for (int p = 0; p < kAssociativity; ++p) {
    EntryNum cand_key = pos.keys[p];
    CacheEntry* cand = sector->EntryAt(cand_key);
    if (KeyMatch(cand, raw_hash)) {
      if (!cand->creating) {
        ++stats->num_put_update;
        EnsureReadyForWriting(sector, cand);
        PutIntoEntry(sector, cand_key, last_use_timestamp_ms, value);
        ScheduleSnapshotIfNecessary(checkpoint_ok, last_use_timestamp_ms,
                                    last_checkpoint_ms, pos.sector);
      } else {
        ++stats->num_put_concurrent_create;
      }
      return;
    }
  }

  // We don't have a current entry with our key, but see if we can overwrite
  // something  unrelated. In this case, we even give up if there are only
  // readers, as it's unclear that they are any less important than us.
  EntryNum best_key = kInvalidEntry;
  CacheEntry* best = NULL;
  for (int p = 0; p < kAssociativity; ++p) {
    EntryNum cand_key = pos.keys[p];
    CacheEntry* cand = sector->EntryAt(cand_key);
    if (Writeable(cand)) {
      if ((best_key == kInvalidEntry) ||
          (cand->last_use_timestamp_ms < best->last_use_timestamp_ms)) {
        best = cand;
        best_key = cand_key;
      }
    }
  }

  if (best_key == kInvalidEntry) {
    // All slots busy. Giving up.
    ++stats->num_put_concurrent_full_set;
    return;
  }

  if (best->byte_size != 0 ||
      !IsAllNil(StringPiece(best->hash_bytes, kHashSize))) {
    ++stats->num_put_replace;
  }

  // Wait for readers before touching the key.
  EnsureReadyForWriting(sector, best);
  std::memcpy(best->hash_bytes, raw_hash.data(), kHashSize);
  PutIntoEntry(sector, best_key, last_use_timestamp_ms, value);

  ScheduleSnapshotIfNecessary(checkpoint_ok, last_use_timestamp_ms,
                              last_checkpoint_ms, pos.sector);
}

template<size_t kBlockSize>
class SharedMemCache<kBlockSize>::WriteOutSnapshotFunction : public Function {
 public:
  WriteOutSnapshotFunction(SharedMemCache<kBlockSize>* cache,
                           int sector_num, int64 last_checkpoint_ms)
      : cache_(cache),
        sector_num_(sector_num),
        last_checkpoint_ms_(last_checkpoint_ms) {}
  ~WriteOutSnapshotFunction() override {}
  void Run() override {
    cache_->WriteOutSnapshotFromWorkerThread(sector_num_, last_checkpoint_ms_);
  }

 private:
  SharedMemCache<kBlockSize>* cache_;
  int sector_num_;
  int64 last_checkpoint_ms_;
  DISALLOW_COPY_AND_ASSIGN(WriteOutSnapshotFunction);
};

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::ScheduleSnapshot(int sector_num,
                                                  int64 last_checkpoint_ms) {
  // We're being called from whatever thread called Put() but snapshotting can
  // take a while so we need to move to the slow worker thread.  We use whatever
  // worker the file cache uses.
  CHECK(file_cache_ != NULL);
  SlowWorker* worker = file_cache_->worker();
  CHECK(worker != NULL);
  worker->Start();
  worker->RunIfNotBusy(
      new WriteOutSnapshotFunction(this, sector_num, last_checkpoint_ms));
  // If the worker chose not to run the snapshotter, because it was busy, we'll
  // try again after the next Put() for this sector.
}

template<size_t kBlockSize>
GoogleString SharedMemCache<kBlockSize>::SnapshotCacheKey(
    int sector_num) const {
  // Important: everything that determines whether it is legitimate to restore a
  // shared memory cache needs to be included in the key here.
  return StrCat("shm_metadata_cache/snapshot/",
                filename_, "/",
                IntegerToString(kSnapshotVersion), "/",
                StrCat(IntegerToString(kBlockSize), "/",
                       IntegerToString(blocks_per_sector_), "/",
                       IntegerToString(num_sectors_), "/",
                       IntegerToString(sector_num)));
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::WriteOutSnapshotFromWorkerThread(
    int sector_num, int64 last_checkpoint_ms) {
  SharedMemCacheDump snapshot;
  bool updated = AddSectorToSnapshot(sector_num, last_checkpoint_ms, &snapshot);
  if (!updated) {
    return;  // Another thread updated it first.  Nothing needs doing.
  }
  GoogleString snapshot_s;
  MarshalSnapshot(snapshot, &snapshot_s);
  SharedString snapshot_s_shared(snapshot_s);

  CHECK(file_cache_ != NULL);
  // It's safe for us to use the file cache from an arbitrary thread because
  // the file cache is thread-agnostic, having no writable member variables.
  file_cache_->Put(SnapshotCacheKey(sector_num), snapshot_s_shared);
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::RestoreFromDisk() {
  if (file_cache_ == NULL) {
    // RegisterSnapshotFileCache was never called, which should only happen in
    // test code.
    handler_->Message(
        kWarning,
        "SharedMemCache: RegisterSnapshotFileCache() not called for %s",
        filename_.c_str());
    return;  // Don't try to restore.
  }

  // We want to delay forking until these snapshots are all loaded, so we rely
  // on the file cache being a synchronous cache.
  CHECK(file_cache_->IsBlocking());
  for (int sector_num = 0; sector_num < num_sectors_; ++sector_num) {
    CacheInterface::SynchronousCallback callback;
    file_cache_->Get(SnapshotCacheKey(sector_num), &callback);
    CHECK(callback.called());
    if (callback.state() == CacheInterface::kAvailable) {
      SharedMemCacheDump snapshot;
      DemarshalSnapshot(callback.value().Value(), &snapshot);
      RestoreSnapshot(snapshot);
    }
  }
  // Some of these may have failed, or there may not have been any in the file
  // cache at all.  This is fine; restoring the snapshots is best-effort.
}

// Expects sector->mutex() held on entry, leaves it held on exit.
template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::PutIntoEntry(
    Sector<kBlockSize>* sector, EntryNum entry_num,
    int64 last_use_timestamp_ms, const SharedString& value) {
  const char* data = value.data();

  CacheEntry* entry = sector->EntryAt(entry_num);
  DCHECK(entry->creating);
  DCHECK_EQ(0u, entry->open_count);

  // Adjust space allocation....
  size_t want_blocks = sector->DataBlocksForSize(value.size());
  BlockVector blocks;
  sector->BlockListForEntry(entry, &blocks);

  // Grab more room if needed.
  if (blocks.size() < want_blocks) {
    if (!TryAllocateBlocks(sector, want_blocks - blocks.size(), &blocks)) {
      // Allocation failed. We torpedo the entry, free all the blocks
      // (both those it has originally and any the above call picked up),
      // and fail the insertion. This should be pretty much impossible.
      // TODO(morlovich): log warning?
      sector->ReturnBlocksToFreeList(blocks);
      entry->creating = false;
      MarkEntryFree(sector, entry_num);
      return;
    }
  }

  // Free up any room we don't need.
  if (blocks.size() > want_blocks) {
    BlockVector extras;
    while (blocks.size() > want_blocks) {
      extras.push_back(blocks.back());
      blocks.pop_back();
    }
    sector->ReturnBlocksToFreeList(extras);
  }

  entry->byte_size = value.size();
  TouchEntry(sector, last_use_timestamp_ms, entry_num);

  // Write out successor list for the blocks we use, and point the entry to it.
  sector->LinkBlockSuccessors(blocks);

  if (!blocks.empty()) {
    entry->first_block = blocks[0];
  } else {
    entry->first_block = kInvalidBlock;
  }

  // Now we can write out the data. We can release the lock while we do that,
  // since we've already removed them from the freelist, and the LRU/directory
  // entry is locked, so can't be concurrently freed.
  sector->mutex()->Unlock();
  for (size_t b = 0; b < want_blocks; ++b) {
    size_t bytes = sector->BytesInPortion(entry->byte_size, b, want_blocks);
    std::memcpy(sector->BlockBytes(blocks[b]), data + b * kBlockSize, bytes);
  }
  sector->mutex()->Lock();

  // We're done, clear creating bit.
  entry->creating = false;
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::Get(const GoogleString& key,
                                     Callback* callback) {
  GoogleString raw_hash = ToRawHash(key);
  Position pos;
  ExtractPosition(raw_hash, &pos);
  CacheInterface::KeyState key_state = kNotFound;
  Sector<kBlockSize>* sector = sectors_[pos.sector];
  {
    ScopedMutex lock(sector->mutex());
    SectorStats* stats = sector->sector_stats();
    ++stats->num_get;

    for (int p = 0; p < kAssociativity; ++p) {
      EntryNum cand_key = pos.keys[p];
      CacheEntry* cand = sector->EntryAt(cand_key);
      if (KeyMatch(cand, raw_hash)) {
        ++stats->num_get_hit;
        key_state = GetFromEntry(key, sector, cand_key, callback);
        break;
      }
    }
  }

  ValidateAndReportResult(key, key_state, callback);
}

// Expects sector->mutex() held on entry, leaves it held on exit.
template<size_t kBlockSize>
CacheInterface::KeyState SharedMemCache<kBlockSize>::GetFromEntry(
    const GoogleString& key,
    Sector<kBlockSize>* sector,
    EntryNum entry_num,
    Callback* callback) {
  CacheEntry* entry = sector->EntryAt(entry_num);
  if (entry->creating) {
    // For now, consider concurrent creation a miss.
    return kNotFound;
  }
  ++entry->open_count;

  TouchEntry(sector, timer_->NowMs(), entry_num);

  BlockVector blocks;
  sector->BlockListForEntry(entry, &blocks);

  // We can release the lock while we do the read, as the entry is now open for
  // reading.
  sector->mutex()->Unlock();

  SharedString str;
  str.Extend(entry->byte_size);

  size_t total_blocks = blocks.size();
  int pos = 0;
  for (size_t b = 0; b < total_blocks; ++b) {
    int bytes = sector->BytesInPortion(entry->byte_size, b, total_blocks);
    str.WriteAt(pos, sector->BlockBytes(blocks[b]), bytes);
    pos += bytes;
  }
  sector->mutex()->Lock();

  // Now reduce the reference count.
  --entry->open_count;

  callback->set_value(str);

  return kAvailable;
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::Delete(const GoogleString& key) {
  GoogleString raw_hash = ToRawHash(key);
  Position pos;
  ExtractPosition(raw_hash, &pos);

  Sector<kBlockSize>* sector = sectors_[pos.sector];
  ScopedMutex lock(sector->mutex());

  for (int p = 0; p < kAssociativity; ++p) {
    EntryNum cand_key = pos.keys[p];
    if (KeyMatch(sector->EntryAt(cand_key), raw_hash)) {
      DeleteEntry(sector, cand_key);
      return;
    }
  }
}

// Called with lock held.
template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::DeleteEntry(Sector<kBlockSize>* sector,
                                             EntryNum entry_num) {
  CacheEntry* entry = sector->EntryAt(entry_num);
  if (entry->creating) {
    // A multiple writers (Put or Delete) race. Let the other one proceed,
    // drop this one. (Call to EnsureReadyForWriting below will deal with any
    // outstanding readers).
    return;
  }
  EnsureReadyForWriting(sector, entry);
  BlockVector blocks;
  sector->BlockListForEntry(entry, &blocks);
  sector->ReturnBlocksToFreeList(blocks);
  entry->creating = false;
  MarkEntryFree(sector, entry_num);
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::SanityCheck() {
  for (int i = 0; i < num_sectors_; ++i) {
    Sector<kBlockSize>* sector = sectors_[i];
    ScopedMutex lock(sector->mutex());

    // Make sure that all blocks are accounted for exactly once.

    // First collect all blocks referred to from entries
    std::map<BlockNum, int> block_occur;
    for (EntryNum e = 0; e < entries_per_sector_; ++e) {
      CacheEntry* entry = sector->EntryAt(e);
      BlockVector blocks;
      sector->BlockListForEntry(entry, &blocks);
      for (size_t i = 0; i < blocks.size(); ++i) {
        ++block_occur[blocks[i]];
      }
    }

    // Now from freelist. We re-use the API for convenience.
    BlockVector freelist_blocks;
    sector->AllocBlocksFromFreeList(blocks_per_sector_, &freelist_blocks);
    for (size_t i = 0; i < freelist_blocks.size(); ++i) {
      ++block_occur[freelist_blocks[i]];
    }
    sector->ReturnBlocksToFreeList(freelist_blocks);

    CHECK(block_occur.size() == static_cast<size_t>(blocks_per_sector_));
    for (std::map<BlockNum, int>::iterator i = block_occur.begin();
         i != block_occur.end(); ++i) {
      CHECK_EQ(1, i->second);
    }
  }
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::RegisterSnapshotFileCache(
    FileCache* potential_file_cache, int checkpoint_interval_sec) {
  if (snapshot_path_ == filename_) {
    return;  // Already set to the best choice.
  }
  StringPiece potential_snapshot_path = potential_file_cache->path();
  if (potential_snapshot_path.empty()) {
    // We get empty paths when some vhosts have set us to unplugged.  That's
    // not a place we can store a snapshot, so don't consider these.
    return;
  }

  if (snapshot_path_.empty() ||
      potential_snapshot_path.compare(snapshot_path_) < 0 ||
      potential_snapshot_path == filename_) {
    // The path given is an improvement, because either no path had been set,
    // this path comes alphabetically earlier, or, if this is an explicitly
    // configured shared memory cache, this is the file cache that was chosen
    // in the config to go with this shared memory cache.
    potential_snapshot_path.CopyToString(&snapshot_path_);
    file_cache_ = potential_file_cache;
    checkpoint_interval_sec_ = checkpoint_interval_sec;
  }
}

template<size_t kBlockSize>
bool SharedMemCache<kBlockSize>::TryAllocateBlocks(
    Sector<kBlockSize>* sector, int goal, BlockVector* blocks) {
  // See how much we have in freelist.
  int got = sector->AllocBlocksFromFreeList(goal, blocks);

  // If not enough, start walking back in LRU and take blocks from those files.
  EntryNum entry_num = sector->OldestEntryNum();
  while ((entry_num != kInvalidEntry) && (got < goal)) {
    CacheEntry* entry = sector->EntryAt(entry_num);
    if (Writeable(entry)) {
      got += sector->BlockListForEntry(entry, blocks);
      MarkEntryFree(sector, entry_num);
      entry_num = sector->OldestEntryNum();
    } else {
      entry_num = entry->lru_prev;
    }
  }

  return (got >= goal);
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::MarkEntryFree(Sector<kBlockSize>* sector,
                                               EntryNum entry_num) {
  sector->UnlinkEntryFromLRU(entry_num);
  CacheEntry* entry = sector->EntryAt(entry_num);
  CHECK(Writeable(entry));
  std::memset(entry->hash_bytes, 0, kHashSize);
  entry->last_use_timestamp_ms = 0;
  entry->byte_size = 0;
  entry->first_block = kInvalidBlock;
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::TouchEntry(Sector<kBlockSize>* sector,
                                            int64 last_use_timestamp_ms,
                                            EntryNum entry_num) {
  CacheEntry* entry = sector->EntryAt(entry_num);
  sector->UnlinkEntryFromLRU(entry_num);
  sector->InsertEntryIntoLRU(entry_num);
  entry->last_use_timestamp_ms = last_use_timestamp_ms;
}

template<size_t kBlockSize>
bool SharedMemCache<kBlockSize>::Writeable(const CacheEntry* entry) {
  return (entry->open_count == 0) && !entry->creating;
}

template<size_t kBlockSize>
bool SharedMemCache<kBlockSize>::KeyMatch(CacheEntry* entry,
                                          const GoogleString& raw_hash) {
  DCHECK_EQ(kHashSize, raw_hash.size());
  return 0 == std::memcmp(entry->hash_bytes, raw_hash.data(), kHashSize);
}

template<size_t kBlockSize>
GoogleString SharedMemCache<kBlockSize>::ToRawHash(const GoogleString& key) {
  GoogleString raw_hash = hasher_->RawHash(key);
  DCHECK(raw_hash.size() >= kHashSize);
  if (raw_hash.size() > kHashSize) {
    raw_hash.resize(kHashSize);
  }

  // Avoid all 0x00, that's special
  if (IsAllNil(raw_hash)) {
    raw_hash[0] = ' ';
  }
  return raw_hash;
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::ExtractPosition(
    const GoogleString& raw_hash,
    SharedMemCache<kBlockSize>::Position* out_pos) {
  // We need at least 13 bytes of hash in code below, as we split it as follows:
  // keys[0] from hash[0..3]
  // keys[1] from hash[4..7]
  // keys[2] from hash[8..11]
  // sector number (hash[12])
  DCHECK_GE(raw_hash.length(), 13u);

  // Should also be consistent with out config
  DCHECK_EQ(raw_hash.length(), kHashSize);

  // This implementation only supports associativity 4, so it will need to be
  // readjusted if we decide to use an another setting.
  COMPILE_ASSERT(kAssociativity == 4, need_different_code_for_other_assoc);

  // Get the sector # from the [12]th byte, being careful not to sign-extend;
  // we have to watch out for negatives for %
  int raw_sector = static_cast<int>(static_cast<unsigned char>(raw_hash[12]));
  out_pos->sector = (raw_sector % sectors_.size());

  const uint32* keys = reinterpret_cast<const uint32*>(raw_hash.data());
  out_pos->keys[0] = static_cast<EntryNum>(keys[0] % entries_per_sector_);
  out_pos->keys[1] = static_cast<EntryNum>(keys[1] % entries_per_sector_);
  out_pos->keys[2] = static_cast<EntryNum>(keys[2] % entries_per_sector_);

  // For entry 3, we potentially already used lower bits of key[3] word for
  // sector, so instead use higher-bits from keys[0] as lower ones.
  uint32 key3 = (keys[0] >> 16) | (keys[1] << 16);
  out_pos->keys[3] = static_cast<EntryNum>(key3 % entries_per_sector_);
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::EnsureReadyForWriting(
    Sector<kBlockSize>* sector, CacheEntry* entry) {
  // It is possible that as we are starting to write, some other processes
  // are still in the middle of copying in read data for this entry, so we have
  // to make sure they finish up first.
  //
  // First, make sure no other readers or writers can join. With ->creating set
  // to true they will both avoid this entry. (And there are no other writers
  // as if there were, we would have given up ourselves).
  //
  entry->creating = true;

  // Now just wait for previous readers to leave.
  while (entry->open_count > 0) {
    ++sector->sector_stats()->num_put_spins;
    sector->mutex()->Unlock();
    timer_->SleepUs(50);
    sector->mutex()->Lock();
  }
}

template<size_t kBlockSize>
int64 SharedMemCache<kBlockSize>::GetLastWriteMsForTesting(int sector_num) {
  Sector<kBlockSize>* sector = sectors_[sector_num];
  SectorStats* stats = sector->sector_stats();
  sector->mutex()->Lock();
  int64 last_checkpoint_ms = stats->last_checkpoint_ms;
  sector->mutex()->Unlock();
  return last_checkpoint_ms;
}

template<size_t kBlockSize>
void SharedMemCache<kBlockSize>::SetLastWriteMsForTesting(
    int sector_num, int64 last_checkpoint_ms) {
  Sector<kBlockSize>* sector = sectors_[sector_num];
  SectorStats* stats = sector->sector_stats();
  sector->mutex()->Lock();
  stats->last_checkpoint_ms = last_checkpoint_ms;
  sector->mutex()->Unlock();
}

template class SharedMemCache<64>;  // metadata ("rname") cache
template class SharedMemCache<512>;  // testing
template class SharedMemCache<4096>;  // HTTP cache

}  // namespace net_instaweb
