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
#include "net/instaweb/util/public/shared_mem_lock_manager.h"

#include <cstddef>
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/timer_based_abstract_lock.h"

namespace net_instaweb {
class AbstractLock;

namespace SharedMemLockData {

// Memory structure:
//
// Bucket 0:
//  Slot 0
//     lock name hash (64-bit)
//     acquire timestamp (64-bit)
//  Slot 1
//  ...
//  Slot 15
//  Mutex
//  (pad to 64-byte alignment)
// Bucket 1:
//  ..
// Bucket 63:
//  ..
//
// Each key is statically assigned to a bucket based on its hash.
// When we're trying to lock or unlock the given named lock, we lock
// the corresponding bucket.
//
// Whenever a lock is held, some slot in the corresponding bucket has its hash
// and the time of acquisition. When a slot is free (or unlocked), its timestamp
// is set to kNotAcquired.
//
// Very old locks can be stolen by new clients, in which case the timestamp gets
// updated. This serves multiple purposes:
// 1) It means only one extra process will grab it for each timeout period,
// as all others will see the new timestamp.
// 2) It makes it possible for the last grabber to be the one to unlock the
// lock, as we check the grabber's acquisition timestamp versus the lock's.
//
// A further issue is what happens when a bucket is overflowed. In that case,
// however, we simply state that lock acquisition failed. This is because the
// purpose of this service is to limit the load on the system, and the table
// getting filled suggests it's under heavy load as it is, in which case
// blocking further operations is desirable.
//
const size_t kBuckets = 64;   // assumed to be <= 256
const size_t kSlotsPerBucket = 32;

struct Slot {
  uint64 hash;
  int64 acquired_at_ms;  // kNotAcquired if free.
};

const int64 kNotAcquired = 0;

struct Bucket {
  Slot slots[kSlotsPerBucket];
  char mutex_base[1];
};

inline size_t Align64(size_t in) {
  return (in + 63) & ~63;
}

inline size_t BucketSize(size_t lock_size) {
  return Align64(offsetof(Bucket, mutex_base) + lock_size);
}

inline size_t SegmentSize(size_t lock_size) {
  return kBuckets * BucketSize(lock_size);
}

}  // namespace SharedMemLockData

namespace Data = SharedMemLockData;

class SharedMemLock : public TimerBasedAbstractLock {
 public:
  virtual ~SharedMemLock() {
    Unlock();
  }

  virtual bool TryLock() {
    return TryLockImpl(false, 0);
  }

  virtual bool TryLockStealOld(int64 timeout_ms) {
    return TryLockImpl(true, timeout_ms);
  }

  virtual void Unlock() {
    if (acquisition_time_ == Data::kNotAcquired) {
      return;
    }

    // Protect the bucket.
    scoped_ptr<AbstractMutex> lock(AttachMutex());
    ScopedMutex hold_lock(lock.get());

    // Search for this lock.
    // note: we permit empty slots in the middle, and start search at different
    // positions depending on the hash to increase chance of quick hit.
    // TODO(morlovich): Consider remembering which bucket we locked to avoid
    // the search. (Could potentially be made lock-free, too).
    size_t base = hash_ % Data::kSlotsPerBucket;
    for (size_t offset = 0; offset < Data::kSlotsPerBucket; ++offset) {
      size_t s = (base + offset) % Data::kSlotsPerBucket;
      Data::Slot& slot = bucket_->slots[s];
      if (slot.hash == hash_ && slot.acquired_at_ms == acquisition_time_) {
        slot.acquired_at_ms = Data::kNotAcquired;
        break;
      }
    }

    acquisition_time_ = Data::kNotAcquired;
  }

  virtual GoogleString name() {
    return name_;
  }

 protected:
  virtual Timer* timer() const {
    return manager_->timer_;
  }

 private:
  friend class SharedMemLockManager;

  // ctor should only be called by CreateNamedLock below.
  SharedMemLock(SharedMemLockManager* manager, const StringPiece& name)
      : manager_(manager),
        name_(name.data(), name.size()),
        acquisition_time_(Data::kNotAcquired) {
    size_t bucket_num;
    GetHashAndBucket(name_, &hash_, &bucket_num);
    bucket_ = manager_->Bucket(bucket_num);
  }

  // Compute hash and bucket used to store the lock for a given lock name.
  void GetHashAndBucket(const StringPiece& name, uint64* hash_out,
                        size_t* bucket_out) {
    GoogleString raw_hash = manager_->hasher_->RawHash(name);

    // We use separate hash bits to determine the hash and the bucket.
    *bucket_out = static_cast<unsigned char>(raw_hash[8]) % Data::kBuckets;

    uint64 hash = 0;
    for (int c = 0; c < 8; ++c) {
      hash = (hash << 8) | static_cast<unsigned char>(raw_hash[c]);
    }
    *hash_out = hash;
  }

  AbstractMutex* AttachMutex() const {
    return manager_->seg_->AttachToSharedMutex(
        manager_->MutexOffset(bucket_));
  }

  bool TryLockImpl(bool steal, int64 steal_timeout_ms) {
    // Protect the bucket.
    scoped_ptr<AbstractMutex> lock(AttachMutex());
    ScopedMutex hold_lock(lock.get());

    int64 now_ms = manager_->timer_->NowMs();
    if (now_ms == Data::kNotAcquired) {
      ++now_ms;
    }

    // Search for existing lock or empty slot. We need to check everything
    // for existing lock, of course.
    size_t empty_slot = Data::kSlotsPerBucket;
    size_t base = hash_ % Data::kSlotsPerBucket;
    for (size_t offset = 0; offset < Data::kSlotsPerBucket; ++offset) {
      size_t s = (base + offset) % Data::kSlotsPerBucket;
      Data::Slot& slot = bucket_->slots[s];
      if (slot.hash == hash_) {
        if (slot.acquired_at_ms == Data::kNotAcquired ||
            (steal && ((now_ms - slot.acquired_at_ms) >= steal_timeout_ms))) {
          // Stealing lock, or re-using a free slot we ourselves unlocked.
          //
          // We know we don't have an actual locked entry with our key elsewhere
          // because:
          // 1) After our last unlock of it no one else has ever locked it (or
          // our key would have been overwritten), so if we ever performed an
          // another lock operation we would have done it with this slot in
          // present state.
          //
          // 2) We always chose the first candidate.
          DoLockSlot(s, now_ms);
          return true;
        } else {
          // Not permitted to steal or not stale enough to steal.
          return false;
        }
      } else if (slot.acquired_at_ms == Data::kNotAcquired) {
        if (empty_slot == Data::kSlotsPerBucket) {
          empty_slot = s;
        }
      }
    }

    if (empty_slot != Data::kSlotsPerBucket) {
      DoLockSlot(empty_slot, now_ms);
      return true;
    }

    manager_->handler_->Message(kInfo,
                                "Overflowed bucket trying to grab lock.");
    return false;
  }

  // Writes out our ID and current timestamp into the slot, and marks the
  // fact of our acquisition.
  void DoLockSlot(size_t s, int64 now_ms) {
    Data::Slot& slot = bucket_->slots[s];
    slot.hash = hash_;
    slot.acquired_at_ms = now_ms;
    acquisition_time_ = now_ms;
  }

  SharedMemLockManager* manager_;
  GoogleString name_;

  uint64 hash_;

  // Time at which we acquired the lock...
  int64 acquisition_time_;

  // base pointer for the bucket we are in.
  Data::Bucket* bucket_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemLock);
};

SharedMemLockManager::SharedMemLockManager(
    AbstractSharedMem* shm, const GoogleString& path, Timer* timer,
    Hasher* hasher, MessageHandler* handler)
    : shm_runtime_(shm),
      path_(path),
      timer_(timer),
      hasher_(hasher),
      handler_(handler),
      lock_size_(shm->SharedMutexSize()) {
  CHECK_GE(hasher_->RawHashSizeInBytes(), 9) << "Need >= 9 byte hashes";
}

SharedMemLockManager::~SharedMemLockManager() {
}

bool SharedMemLockManager::Initialize() {
  seg_.reset(shm_runtime_->CreateSegment(path_, Data::SegmentSize(lock_size_),
                                         handler_));
  if (seg_.get() == NULL) {
    handler_->Message(kError, "Unable to create memory segment for locks.");
    return false;
  }

  // Create the mutexes for each bucket
  for (size_t bucket = 0; bucket < Data::kBuckets; ++bucket) {
    if (!seg_->InitializeSharedMutex(MutexOffset(Bucket(bucket)), handler_)) {
      handler_->Message(kError, "%s",
                        StrCat("Unable to create lock service mutex #",
                               Integer64ToString(bucket)).c_str());
      return false;
    }
  }
  return true;
}

bool SharedMemLockManager::Attach() {
  size_t size = Data::SegmentSize(shm_runtime_->SharedMutexSize());
  seg_.reset(shm_runtime_->AttachToSegment(path_, size, handler_));
  if (seg_.get() == NULL) {
    handler_->Message(kWarning, "Unable to attach to lock service SHM segment");
    return false;
  }

  return true;
}

void SharedMemLockManager::GlobalCleanup(
  AbstractSharedMem* shm, const GoogleString& path, MessageHandler* handler) {
  shm->DestroySegment(path, handler);
}

AbstractLock* SharedMemLockManager::CreateNamedLock(const StringPiece& name) {
  return new SharedMemLock(this, name);
}

Data::Bucket* SharedMemLockManager::Bucket(size_t bucket) {
  return reinterpret_cast<Data::Bucket*>(
      const_cast<char*>(seg_->Base()) + bucket * Data::BucketSize(lock_size_));
}

size_t SharedMemLockManager::MutexOffset(SharedMemLockData::Bucket* bucket) {
  return &bucket->mutex_base[0] - seg_->Base();
}

}  // namespace net_instaweb
