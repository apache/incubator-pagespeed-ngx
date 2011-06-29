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

#include <cstdlib>         // for malloc
#include <algorithm>       // for min
#include "base/logging.h"  // for DCHECK
#include "net/instaweb/util/public/circular_buffer.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;

CircularBuffer* CircularBuffer::Create(const int capacity) {
  int total = Sizeof(capacity);
  CircularBuffer* cb = static_cast<CircularBuffer*>(malloc(total));
  cb->capacity_ = capacity;
  cb->wrapped_ = false;
  cb->offset_ = 0;
  return cb;
}

CircularBuffer* CircularBuffer::Init(bool parent, void* block,
                                     const int block_size,
                                     const int capacity) {
  // Check if the pre-allocated block has right size for CircularBuffer.
  DCHECK(block_size == Sizeof(capacity));
  CircularBuffer* cb = static_cast<CircularBuffer*>(block);
  if (parent) {
    // In root process, initialize the variables.
    cb->capacity_ = capacity;
    cb->wrapped_ = false;
    cb->offset_ = 0;
  }
  return cb;
}

void CircularBuffer::Clear() {
  offset_ = 0;
  wrapped_ = false;
}

bool CircularBuffer::Write(const StringPiece& message) {
  const char* data = message.data();
  int size = message.size();
  // Left-truncate the message if its size is larger than buffer.
  if (size > capacity_) {
    data += size - capacity_;
    size = capacity_;
    memcpy(buffer_, data, size);
    offset_ = 0;
    wrapped_ = true;
    return true;
  }
  // Otherwise, start to write the message at offset.
  if (offset_ == capacity_) {
    offset_ = 0;
    wrapped_ = true;
  }
  int rest = capacity_ - offset_;
  int len = std::min(rest, size);
  memcpy(buffer_ + offset_, data, len);
  offset_ += len;
  // If available size < message size < buffer capacity,
  // write the rest of the data at the beginning of the buffer.
  if (len < size) {
    memcpy(buffer_, data + len, size - len);
    offset_ = size - len;
    wrapped_ = true;
  }
  return true;
}

GoogleString CircularBuffer::ToString(MessageHandler* handler) {
  GoogleString result;
  StringWriter writer(&result);
  writer.Write(FirstChunk(), handler);
  writer.Write(SecondChunk(), handler);
  return result;
}

StringPiece CircularBuffer::FirstChunk() {
  if (!wrapped_) {
    return StringPiece();
  }
  return StringPiece(buffer_ + offset_, capacity_ - offset_);
}

StringPiece CircularBuffer::SecondChunk() {
  return StringPiece(buffer_, offset_);
}

}  // namespace net_instaweb
