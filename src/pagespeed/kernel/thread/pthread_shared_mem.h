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
//
// Author: morlovich@google.com (Maksim Orlovich)

#ifndef PAGESPEED_KERNEL_THREAD_PTHREAD_SHARED_MEM_H_
#define PAGESPEED_KERNEL_THREAD_PTHREAD_SHARED_MEM_H_

#include <cstddef>
#include <map>
#include <utility>

#include "pagespeed/kernel/base/abstract_shared_mem.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {
class MessageHandler;

// POSIX shared memory support, using mmap/pthread_mutexattr_setpshared
// Supports both processes and threads, but processes that want to access it
// must be results of just fork (without exec), and all the CreateSegment
// calls must occur before the fork.
//
// This implementation is also not capable of deallocating segments except
// at exit, so it should not be used when the set of segments may be dynamic.
class PthreadSharedMem : public AbstractSharedMem {
 public:
  PthreadSharedMem();
  virtual ~PthreadSharedMem();

  virtual size_t SharedMutexSize() const;

  virtual AbstractSharedMemSegment* CreateSegment(
      const GoogleString& name, size_t size, MessageHandler* handler);

  virtual AbstractSharedMemSegment* AttachToSegment(
      const GoogleString& name, size_t size, MessageHandler* handler);

  virtual void DestroySegment(const GoogleString& name,
                              MessageHandler* handler);

  // Frees all lazy-initialized memory used to track shared-memory segments.
  static void Terminate();

 private:
  typedef std::map<GoogleString, std::pair<char*, size_t> > SegmentBaseMap;

  // Accessor for below. Note that the segment_bases_lock will be held at exit.
  static SegmentBaseMap* AcquireSegmentBases();

  static void UnlockSegmentBases();

  // Prefixes the passed in segment name with the current instance number.
  GoogleString PrefixSegmentName(const GoogleString& name);

  // The root process stores segment locations here. Child processes will
  // inherit a readonly copy of this map after the fork. Note that this is
  // initialized in a thread-unsafe manner, given the above assumptions.
  static SegmentBaseMap* segment_bases_;

  // Holds the number of times a PthreadSharedMem has been created.
  static size_t s_instance_count_;
  // Used to prefix segment names, so that when two runtimes are active at the
  // same moment they will not have overlapping segment names. This occurs in
  // ngx_pagespeed during a configuration reload, where first a new factory is
  // created, before destroying the old one.
  size_t instance_number_;

  DISALLOW_COPY_AND_ASSIGN(PthreadSharedMem);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_THREAD_PTHREAD_SHARED_MEM_H_
