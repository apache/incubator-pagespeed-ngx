// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MOD_SPDY_COMMON_SPDY_FRAME_QUEUE_H_
#define MOD_SPDY_COMMON_SPDY_FRAME_QUEUE_H_

#include <list>

#include "base/basictypes.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"

namespace net { class SpdyFrame; }

namespace mod_spdy {

// A simple FIFO queue of SPDY frames, intended for sending input frames from
// the SPDY connection thread to a SPDY stream thread.  This class is
// thread-safe -- all methods may be called concurrently by multiple threads.
class SpdyFrameQueue {
 public:
  // Create an initially-empty queue.
  SpdyFrameQueue();
  ~SpdyFrameQueue();

  // Return true if this queue has been aborted.
  bool is_aborted() const;

  // Abort the queue.  All frames held by the queue will be deleted; future
  // frames passed to Insert() will be immediately deleted; future calls to
  // Pop() will fail immediately; and current blocking calls to Pop will
  // immediately unblock and fail.
  void Abort();

  // Insert a frame into the queue.  The queue takes ownership of the frame,
  // and will delete it if the queue is deleted or aborted before the frame is
  // removed from the queue by the Pop method.
  void Insert(net::SpdyFrame* frame);

  // Remove and provide a frame from the queue and return true, or return false
  // if the queue is empty or has been aborted.  If the block argument is true,
  // block until a frame becomes available (or the queue is aborted).  The
  // caller gains ownership of the provided frame object.
  bool Pop(bool block, net::SpdyFrame** frame);

 private:
  // This is a pretty naive implementation of a thread-safe queue, but it's
  // good enough for our purposes.  We could use an apr_queue_t instead of
  // rolling our own class, but it lacks the ownership semantics that we want.
  mutable base::Lock lock_;
  base::ConditionVariable condvar_;
  std::list<net::SpdyFrame*> queue_;
  bool is_aborted_;

  DISALLOW_COPY_AND_ASSIGN(SpdyFrameQueue);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_COMMON_SPDY_FRAME_QUEUE_H_
