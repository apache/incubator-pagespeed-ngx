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

#ifndef PAGESPEED_KERNEL_SHAREDMEM_SHARED_CIRCULAR_BUFFER_H_
#define PAGESPEED_KERNEL_SHAREDMEM_SHARED_CIRCULAR_BUFFER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/writer.h"

namespace net_instaweb {

class AbstractSharedMem;
class AbstractSharedMemSegment;
class AbstractMutex;
class CircularBuffer;
class MessageHandler;

// Shared memory circular buffer, the content of its shared memory segment is a
// Mutex and a CircularBuffer.
// In parent process, we initialize a shared memory segment. Then we create a
// SharedCircularBuffer object in each process and attach it to the segment by
// calling InitSegment(true, handler) once in the parent process and calling
// InitSegment(false, handler) in each child.

class SharedCircularBuffer : public Writer {
 public:
  // Construct with shared memory, data buffer capacity, filename_prefix and
  // filename_suffix. filename_prefix and filename_suffix are used to name
  // segment for the shared circular buffer.
  SharedCircularBuffer(AbstractSharedMem* shm_runtime,
                       const int buffer_capacity,
                       const GoogleString& filename_prefix,
                       const GoogleString& filename_suffix);
  virtual ~SharedCircularBuffer();
  // Initialize the shared memory segment.
  // parent = true if this is invoked in root process -- initialize the shared
  // memory; parent = false if this is invoked in child process -- attach to
  // existing segment.
  bool InitSegment(bool parent, MessageHandler* handler);
  // Reset circular buffer.
  void Clear();
  // Write content to circular buffer.
  virtual bool Write(const StringPiece& message, MessageHandler* handler);
  virtual bool Flush(MessageHandler* message_handler) { return true; }

  // Write content of data in buffer to writer, without clearing the buffer.
  virtual bool Dump(Writer* writer, MessageHandler* handler);
  // Return data content as string. This is for test purposes.
  GoogleString ToString(MessageHandler* handler);
  // This should be called from the root process as it is about to exit, when no
  // future children are expected to start.
  void GlobalCleanup(MessageHandler* handler);

 private:
  bool InitMutex(MessageHandler* handler);
  GoogleString SegmentName() const;

  // SegmentName looks like:
  // filename_prefix/SharedCircularBuffer.filename_suffix.
  AbstractSharedMem* shm_runtime_;
  // Capacity of circular buffer.
  const int buffer_capacity_;
  // Circular buffer.
  CircularBuffer* buffer_;
  const GoogleString filename_prefix_;
  // filename_suffix_ is used to distinguish SharedCircularBuffer.
  const GoogleString filename_suffix_;
  // Mutex for segment.
  scoped_ptr<AbstractMutex> mutex_;
  // Shared memory segment.
  scoped_ptr<AbstractSharedMemSegment> segment_;

  DISALLOW_COPY_AND_ASSIGN(SharedCircularBuffer);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_SHAREDMEM_SHARED_CIRCULAR_BUFFER_H_
