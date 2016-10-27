/*
 * Copyright 2010 Google Inc.
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

#ifndef PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_CACHE_H_
#define PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_CACHE_H_

#include <cstddef>
#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/cache/file_cache.h"
#include "pagespeed/kernel/sharedmem/shared_mem_cache_data.h"

namespace net_instaweb {

class AbstractSharedMem;
class AbstractSharedMemSegment;
class Hasher;
class MessageHandler;
class SharedMemCacheDump;
class Timer;

// Abstract interface for a cache.
template<size_t kBlockSize>
class SharedMemCache : public CacheInterface {
 public:
  static const int kAssociativity = 4;  // Note: changing this requires changing
                                        // code of ExtractPosition as well.

  // Initializes the cache's settings, but does not actually touch the shared
  // memory --- you must call Initialize or Attach (and handle them potentially
  // returning false) to do so. The filename parameter will be used to identify
  // the shared memory segment, so distinct caches should use distinct values.
  //
  // Callers who want checkpointing need to call RegisterSnapshotFileCache().
  //
  // Precondition: hasher's raw mode must produce 13 bytes or more.
  SharedMemCache(AbstractSharedMem* shm_runtime, const GoogleString& filename,
                 Timer* timer, const Hasher* hasher, int sectors,
                 int entries_per_sector, int blocks_per_sector,
                 MessageHandler* handler);

  virtual ~SharedMemCache();

  // Sets up our shared state for use of all child processes/threads. Returns
  // whether successful. This should be called exactly once for every cache
  // in the root process, before forking.
  //
  // If a file cache was set this restores any snapshotted sectors to shared
  // memory.
  bool Initialize();

  // Connects to already initialized state from a child process. It must be
  // called once for every cache in every child process (that is, post-fork).
  // Returns whether successful.
  bool Attach();

  // This should be called from the root process as it is about to exit, when
  // no further children are expected to start.
  static void GlobalCleanup(AbstractSharedMem* shm_runtime,
                            const GoogleString& filename,
                            MessageHandler* message_handler);

  // Computes how many entries and blocks per sector a cache with total size
  // 'size_kb' and 'sectors' should have if there are about 'block_entry_ratio'
  // worth of blocks of data per every entry. You probably want to underestimate
  // this ratio somewhat, since having extra entries can reduce conflicts.
  // Also outputs size_cap, which is the limit on object size for the resulting
  // cache.
  static void ComputeDimensions(int64 size_kb,
                                int block_entry_ratio,
                                int sectors,
                                int* entries_per_sector_out,
                                int* blocks_per_sector_out,
                                int64* size_cap_out);

  // Returns the largest size of an object this cache can store.
  size_t MaxValueSize() const {
    return (blocks_per_sector_ * kBlockSize) / 8;
  }

  // Returns some statistics as plaintext.
  // TODO(morlovich): Potentially periodically push these to the main
  // Statistics system (or pull to it from these).
  GoogleString DumpStats();

  // Tries to dump the contents the specified sector to *dest, aborts early if a
  // different thread is already working on it, and returns whether it was
  // successful.  To make sure only one thread ends up dumping the sector it
  // compares the last_checkpoint_ms provided to the one in the sector, and only
  // continues with the dump if they match.  After a successful dump, it updates
  // the last_checkpoint_ms in the sector to the current time.
  //
  // Note: other accesses to the sector will be locked out for the duration.
  bool AddSectorToSnapshot(int sector_num, int64 last_checkpoint_ms,
                           SharedMemCacheDump* dest);

  // Restores entries stored in the dump into this cache. The dump
  // may contain multiple sectors.
  void RestoreSnapshot(const SharedMemCacheDump& dump);

  // Encode/Decode SharedMemCacheDump objects.
  static void MarshalSnapshot(const SharedMemCacheDump& dump,
                              GoogleString* out);
  static void DemarshalSnapshot(const StringPiece& marshaled,
                                SharedMemCacheDump* out);

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, const SharedString& value);
  virtual void Delete(const GoogleString& key);
  static GoogleString FormatName();
  virtual GoogleString Name() const { return FormatName();}

  virtual bool IsBlocking() const { return true; }
  virtual bool IsHealthy() const { return true; }
  virtual void ShutDown() {
    // TODO(morlovich): Implement
  }

  // Sanity check the cache data structures.
  void SanityCheck();

  // Use the specified FileCache for loading and storing snapshots.  This may be
  // called multiple times with different FileCaches but we pick one.  If the
  // cache has the same path we were constructed with, use that.  Otherwise, to
  // handle the default shm cache case, use the cache with the path that comes
  // first alphabetically.
  void RegisterSnapshotFileCache(FileCache* potential_file_cache,
                                 int checkpoint_interval_sec);

  StringPiece snapshot_path() const { return snapshot_path_; }
  FileCache* file_cache() const { return file_cache_; }

  int64 GetLastWriteMsForTesting(int sector_num);
  void SetLastWriteMsForTesting(int sector_num, int64 last_checkpoint_ms);

  void WriteOutSnapshotForTesting(int sector_num, int64 last_checkpoint_ms) {
    WriteOutSnapshotFromWorkerThread(sector_num, last_checkpoint_ms);
  }

 private:
  class WriteOutSnapshotFunction;

  // Describes potential placements of a key
  struct Position {
    int sector;
    SharedMemCacheData::EntryNum keys[kAssociativity];
  };

  bool InitCache(bool parent);

  // PutRawHash can be used in either realtime mode or in restore mode.  In
  // realtime mode (checkpoint_ok=true) a put can trigger a checkpoint and
  // last_use_timestamp_ms should be the current time.  In restore mode, a put
  // shouldn't checkpointing because we're in the middle of restoring a
  // checkpoint and last_use_timestamp_ms should be the timestamp to restore for
  // the entry.
  void PutRawHash(const GoogleString& raw_hash, int64 last_use_timestamp_ms,
                  const SharedString& value, bool checkpoint_ok);

  // Finish a get, with the entry matching and sector lock held.  Releases lock
  // while performing the read, but takes it again before returning.
  CacheInterface::KeyState GetFromEntry(
      const GoogleString& key,
      SharedMemCacheData::Sector<kBlockSize>* sector,
      SharedMemCacheData::EntryNum entry_num,
      Callback* str) EXCLUSIVE_LOCKS_REQUIRED(sector->mutex());

  // Finish a put into the given entry. Lock is expected to be held at entry,
  // will still be held when done. The hash in the entry must also be already
  // correct at time of call.
  void PutIntoEntry(SharedMemCacheData::Sector<kBlockSize>* sector,
                    SharedMemCacheData::EntryNum entry_num,
                    int64 last_use_timestamp_ms, const SharedString& value)
      EXCLUSIVE_LOCKS_REQUIRED(sector->mutex());

  // Finish a delete, with the entry matching and sector lock held.
  void DeleteEntry(SharedMemCacheData::Sector<kBlockSize>* sector,
                   SharedMemCacheData::EntryNum entry_num)
      EXCLUSIVE_LOCKS_REQUIRED(sector->mutex());

  // Attempts to allocate at least the given number of blocks, and appends any
  // blocks it manages to allocate to *blocks. Returns whether successful.
  //
  // Note that in case of failure, some blocks may still have been allocated,
  // so the caller may have to clean them up. When successful, this method may
  // allocate more memory than is requested.
  bool TryAllocateBlocks(SharedMemCacheData::Sector<kBlockSize>* sector,
                         int goal, SharedMemCacheData::BlockVector* blocks)
      EXCLUSIVE_LOCKS_REQUIRED(sector->mutex());

  // Marks the given entry free in the directory, and unlinks it from the LRU.
  // Note that this does not touch the entry's blocks.
  void MarkEntryFree(SharedMemCacheData::Sector<kBlockSize>* sector,
                     SharedMemCacheData::EntryNum entry_num);

  // Marks entry as having been recently used, and updates timestamp.
  void TouchEntry(SharedMemCacheData::Sector<kBlockSize>* sector,
                  int64 last_use_timestamp_ms,
                  SharedMemCacheData::EntryNum entry_num);

  // Returns true if the entry can be written (in particular meaning it's not
  // opened by someone else)
  bool Writeable(const SharedMemCacheData::CacheEntry* entry);

  bool KeyMatch(SharedMemCacheData::CacheEntry* entry,
                const GoogleString& raw_hash);

  GoogleString ToRawHash(const GoogleString& key);

  // Given a hash, tells what sector and what entries in it to check.
  void ExtractPosition(const GoogleString& raw_hash, Position* out_pos);

  // Makes sure we have exclusive write access to the entry, with no concurrent
  // readers. Must be called with sector lock held.
  void EnsureReadyForWriting(SharedMemCacheData::Sector<kBlockSize>* sector,
                             SharedMemCacheData::CacheEntry* entry)
      EXCLUSIVE_LOCKS_REQUIRED(sector->mutex());

  // Restore snapshots from the file cache, if set.
  void RestoreFromDisk();

  // Helper for PutRawHash that decides whether to call ScheduleSnapshot and
  // then makes the call if appropriate.
  void ScheduleSnapshotIfNecessary(bool checkpoint_ok,
                                   int64 last_use_timestamp_ms,
                                   int64 last_checkpoint_ms,
                                   int sector_num);

  // Write out the specified sector to the file cache so it will survive process
  // restarts.  We pass along last_checkpoint_ms so that if another thread or
  // process succeeds at snapshotting before we do we can abort quickly.
  void ScheduleSnapshot(int sector_num, int64 last_checkpoint_ms);

  // ScheduleSnapshot asks the FileCache's slow worker thread to call this.
  void WriteOutSnapshotFromWorkerThread(int sector_num,
                                        int64 last_checkpoint_ms);

  // Key to store the snapshot of this sector under.  If two SharedMemCaches
  // have the same cache key it's safe to restore a snapshot dumped from one
  // into the other.
  GoogleString SnapshotCacheKey(int sector_num) const;

  AbstractSharedMem* shm_runtime_;
  const Hasher* hasher_;
  Timer* timer_;
  GoogleString filename_;
  int num_sectors_;
  int entries_per_sector_;
  int blocks_per_sector_;
  int checkpoint_interval_sec_;
  MessageHandler* handler_;
  GoogleString snapshot_path_;
  FileCache* file_cache_;

  scoped_ptr<AbstractSharedMemSegment> segment_;
  std::vector<SharedMemCacheData::Sector<kBlockSize>*> sectors_;

  GoogleString name_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemCache);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_CACHE_H_
