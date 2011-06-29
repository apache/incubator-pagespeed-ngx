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

// Author: fangfei@google.com (Fangfei Zhou)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_CIRCULAR_BUFFER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_CIRCULAR_BUFFER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;

// CircularBuffer which can be instantiated using malloc or with a
// pre-allocated buffer.
class CircularBuffer {
 public:
  // Instantiate buffer with malloc.
  static CircularBuffer* Create(const int capacity);
  // Instantiate buffer with pre-allocated block.
  // parent = true if this is invoked in root process (ie, this call should
  // initialize the fields of the segment).
  // block is a pointer to a pre-allocated segment.
  // block_size is the size of the block, it must be equal to Sizeof(capacity).
  // capacity is the size of the data buffer (the maximum amount of data
  // which we can buffer at one time).
  static CircularBuffer* Init(bool parent, void* block,
                              const int block_size,
                              const int capacity);
  // Return the size in bytes of the memory block to allocate to hold a buffer
  // with size equal to capacity.
  // capacity is the size of data buffer.
  static int Sizeof(const int capacity) {
    // buffer[1] is double counted, so -1 here.
    int total = sizeof(CircularBuffer) + capacity - 1;
    return total;
  }
  // Reset offset of data segment and wrapped flag.
  // The old content is not cleared but shouldn't be print out again.
  void Clear();
  // Write message to buffer.
  bool Write(const StringPiece& message);
  // Return data content as string.
  GoogleString ToString(MessageHandler* handler);

 private:
  // Can't construct -- must call Create() or Init() from a pre-allocated
  // block of correct size.
  CircularBuffer();
  ~CircularBuffer();
  // Return the part from offset to the end of buffer.
  StringPiece FirstChunk();
  // Return the part from the beginning to the offset of buffer.
  StringPiece SecondChunk();

  // Capacity of buffer.
  int capacity_;
  // Flag of having wrapped message.
  bool wrapped_;
  // Position to write in buffer.
  int offset_;
  // Buffer.
  char buffer_[1];

  DISALLOW_COPY_AND_ASSIGN(CircularBuffer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CIRCULAR_BUFFER_H_
