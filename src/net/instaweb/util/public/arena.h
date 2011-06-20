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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_ARENA_H_
#define NET_INSTAWEB_UTIL_PUBLIC_ARENA_H_

#include <vector>
#include <cstddef>

#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

// This template keeps a packed set of objects inheriting from the same base
// type (which must have a virtual destructor) where all of the
// objects in the same arena are expected to be destroyed at once.
template<typename T>
class Arena {
 public:
  // All allocations we make will be aligned to this. We will also reserve
  // this much room for our work area, as it keeps things simple.
  static const size_t kAlign = 8;

  Arena() {
    InitEmpty();
  }

  ~Arena() {
    CHECK(chunks_.empty());
  }

  void* Allocate(size_t size) {
    size += kAlign;  // Need room to link the next object.
    size = ExpandToAlign(size);

    DCHECK(sizeof(void*) <= kAlign);
    DCHECK(size < Chunk::kSize);

    if (next_alloc_ + size > chunk_end_) {
      AddChunk();
    }

    char* base = next_alloc_;

    // Update the links -- the previous object should point to our
    // chunk's base, our base should point to NULL, and last_link_
    // should point to our base
    char** our_last_link_field = reinterpret_cast<char**>(base);
    *last_link_ = base;
    *our_last_link_field = NULL;
    last_link_ = our_last_link_field;

    next_alloc_ += size;

    char* out = base + kAlign;
    DCHECK((reinterpret_cast<uintptr_t>(out) & (kAlign - 1)) == 0);
    return out;
  }

  // Cleans up all the objects in the arena. You must call this explicitly.
  void DestroyObjects();

  // Rounds block size up to 8; we always align to it, even on 32-bit.
  static size_t ExpandToAlign(size_t in) {
    return (in + kAlign - 1) & ~(kAlign - 1);
  }

 private:
  struct Chunk {
    // Representation: the arena is a vector of fixed-size (kSize, 8k) chunks;
    // we allocate objects from the end of the most recently created chunk until
    // an allocation doesn't fit (in which case we make a fresh chunk)
    //
    // Each chunk is independently organized into a singly-linked list, where
    // we precede each object with a pointer to the next allocated block, e.g.:
    //
    //                  /-----------------|
    //                  |                \|/
    // -----------------|---------------|----|-----------|
    // |   | object 1 | | | object 2    | NU | object 3  |
    // | | |          |   |             | LL |           |
    // |_|_|----------|---|-------------|----|-----------|
    //   |              ^
    //   \--------------/
    // We need this because objects may have different sizes, and we'll need
    // to find everyone to call their destructor
    static const size_t kSize = 8192;
    char buf[kSize];
  };

  // Adds in a new chunk and initializes all the fields below to refer to it
  void AddChunk();

  // Sets up all the pointers below to denote us being empty.
  void InitEmpty();

  // First free byte of the current chunk
  char* next_alloc_;

  // The point where to link in a new object we allocate.
  // We need this so that the last object in a chunk has
  // a null 'next' link.
  char** last_link_;

  // First address after the last byte of the currently active chunk
  char* chunk_end_;

  // Scratch location in case we want a write to go nowhere.
  // Does not point anywhere, just bitbuckets writes
  char* scratch_;

  std::vector<Chunk*> chunks_;
};

template<typename T>
void Arena<T>::AddChunk() {
  Chunk* chunk = new Chunk();
  chunks_.push_back(chunk);
  next_alloc_ = chunk->buf;
  chunk_end_ = next_alloc_ + Chunk::kSize;
  last_link_ = &scratch_;
}

template<typename T>
void Arena<T>::DestroyObjects() {
  for (int i = 0; i < static_cast<int>(chunks_.size()); ++i) {
    // Walk through objects in this chunk.
    char* base = chunks_[i]->buf;
    while (base != NULL) {
      reinterpret_cast<T*>(base + kAlign)->~T();
      base = *reinterpret_cast<char**>(base);
    }
    delete chunks_[i];
  }
  chunks_.clear();
  InitEmpty();
}

template<typename T>
void Arena<T>::InitEmpty() {
  // The way this is initialized ensures that the next call to allocate
  // will call AddChunk(). Doing it this way rather than calling NewChunk()
  // from here establishes the invariant that all the chunks are non-empty,
  // which helps in DestroyObjects()
  next_alloc_ = NULL;
  last_link_ = NULL;
  chunk_end_ = NULL;
}

}  // namespace  net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_ARENA_H_
