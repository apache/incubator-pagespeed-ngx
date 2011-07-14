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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SHARED_CIRCULAR_BUFFER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SHARED_CIRCULAR_BUFFER_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractSharedMem;
class AbstractSharedMemSegment;
class AbstractMutex;
class CircularBuffer;
class MessageHandler;
class Writer;

// Shared memory circular buffer, the content of its shared memory segment is a
// Mutex and a CircularBuffer.
// In parent process, we initialize a shared memory segment. Then we create a
// SharedCircularBuffer object in each process and attach it to the segment by
// calling InitSegment(true, handler) once in the parent process and calling
// InitSegment(false, handler) in each child.

class SharedCircularBuffer {
 public:
  // Construct with shared memory segment and buffer size.
  SharedCircularBuffer(int buffer_capacity_,
                       AbstractSharedMem* shm_runtime,
                       const GoogleString& filename_prefix);
  virtual ~SharedCircularBuffer();
  // Initialize the shared memory segment.
  // parent = true if this is invoked in root process -- initialize the shared
  // memory; parent = false if this is invoked in child process -- attach to
  // existing segment.
  bool InitSegment(bool parent, MessageHandler* handler);
  // Reset circular buffer.
  void Clear();
  // Write content to circular buffer.
  bool Write(const StringPiece& message);
  // Write content of data in buffer to writer, without clearing the buffer.
  bool Dump(Writer* writer, MessageHandler* handler);
  // Return data content as string. This is for test purposes.
  GoogleString ToString(MessageHandler* handler);
  // This should be called from the root process as it is about to exit, when no
  // future children are expected to start.
  void GlobalCleanup(MessageHandler* handler);

 private:
  bool InitMutex(MessageHandler* handler);
  GoogleString SegmentName() const;

  // Capacity of circular buffer.
  const int buffer_capacity_;
  // Size to allocate for circular buffer.
  int buffer_size_;
  AbstractSharedMem* shm_runtime_;
  // Circular buffer.
  CircularBuffer* buffer_;
  // Mutex for segment.
  const GoogleString filename_prefix_;
  scoped_ptr<AbstractMutex> mutex_;
  // Shared memory segment.
  scoped_ptr<AbstractSharedMemSegment> segment_;

  DISALLOW_COPY_AND_ASSIGN(SharedCircularBuffer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SHARED_CIRCULAR_BUFFER_H_
