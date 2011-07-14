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

#include "net/instaweb/util/public/shared_circular_buffer.h"

#include <cstddef>
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/circular_buffer.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"

namespace {
  const char kSharedCircularBufferObjName[] = "SharedCircularBuffer";
}    // namespace

namespace net_instaweb {

SharedCircularBuffer::SharedCircularBuffer(int buffer_capacity,
                                           AbstractSharedMem* shm_runtime,
                                           const GoogleString& filename_prefix)
    : buffer_capacity_(buffer_capacity),
      shm_runtime_(shm_runtime), filename_prefix_(filename_prefix) {
}

SharedCircularBuffer::~SharedCircularBuffer() {
}

// Initialize shared mutex.
bool SharedCircularBuffer::InitMutex(MessageHandler* handler) {
  if (!segment_->InitializeSharedMutex(0, handler)) {
    handler->Message(
        kError, "Unable to create mutex for shared memory circular buffer");
    return false;
  }
  return true;
}

bool SharedCircularBuffer::InitSegment(bool parent,
                                       MessageHandler* handler) {
  // Size of segment includes mutex and circular buffer.
  int buffer_size = CircularBuffer::Sizeof(buffer_capacity_);
  size_t total = shm_runtime_->SharedMutexSize() + buffer_size;
  if (parent) {
    // In root process -> initialize the shared memory.
    segment_.reset(
        shm_runtime_->CreateSegment(SegmentName(), total, handler));
    // Initialize mutex.
    if (segment_.get() != NULL && !InitMutex(handler)) {
      segment_.reset(NULL);
      shm_runtime_->DestroySegment(SegmentName(), handler);
      return false;
    }
  } else {
    // In child process -> attach to exisiting segment.
    segment_.reset(
        shm_runtime_->AttachToSegment(SegmentName(), total, handler));
    if (segment_.get() == NULL) {
      return false;
    }
  }
  // Attach Mutex.
  mutex_.reset(segment_->AttachToSharedMutex(0));
  // Initialize the circular buffer.
  int pos = shm_runtime_->SharedMutexSize();
  buffer_ = CircularBuffer::Init(
                parent,
                static_cast<void*>(const_cast<char*>(segment_->Base() + pos)),
                buffer_size, buffer_capacity_);
  return true;
}

void SharedCircularBuffer::Clear() {
  ScopedMutex hold_lock(mutex_.get());
  buffer_->Clear();
}

bool SharedCircularBuffer::Write(const StringPiece& message) {
  ScopedMutex hold_lock(mutex_.get());
  return buffer_->Write(message);
}

bool SharedCircularBuffer::Dump(Writer* writer, MessageHandler* handler) {
  ScopedMutex hold_lock(mutex_.get());
  return (writer->Write(buffer_->ToString(handler), handler));
}

GoogleString SharedCircularBuffer::ToString(MessageHandler* handler) {
  ScopedMutex hold_lock(mutex_.get());
  return buffer_->ToString(handler);
}

void SharedCircularBuffer::GlobalCleanup(MessageHandler* handler) {
  if (segment_.get() != NULL) {
    shm_runtime_->DestroySegment(SegmentName(), handler);
  }
}

GoogleString SharedCircularBuffer::SegmentName() const {
  return StrCat(filename_prefix_, kSharedCircularBufferObjName);
}

}  // namespace net_instaweb
