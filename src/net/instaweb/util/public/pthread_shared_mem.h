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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_PTHREAD_SHARED_MEM_H_
#define NET_INSTAWEB_UTIL_PUBLIC_PTHREAD_SHARED_MEM_H_

#include <map>

#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

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

 private:
  typedef std::map<GoogleString, char*> SegmentBaseMap;

  // Accessor for below. Note that the segment_bases_lock will be held at exit.
  static SegmentBaseMap* AcquireSegmentBases();

  static void UnlockSegmentBases();

  // The root process stores segment locations here. Child processes will
  // inherit a readonly copy of this map after the fork. Note that this is
  // initialized in a thread-unsafe manner, given the above assumptions.
  static SegmentBaseMap* segment_bases_;

  DISALLOW_COPY_AND_ASSIGN(PthreadSharedMem);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_PTHREAD_SHARED_MEM_H_
