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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_NULL_SHARED_MEM_H_
#define NET_INSTAWEB_UTIL_PUBLIC_NULL_SHARED_MEM_H_

#include <cstddef>
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class MessageHandler;

// A stub implementation of shared memory for systems where we do not
// have a real one. Fails all the operations.
class NullSharedMem : public AbstractSharedMem {
 public:
  NullSharedMem();
  virtual ~NullSharedMem();

  virtual size_t SharedMutexSize() const;

  virtual AbstractSharedMemSegment* CreateSegment(
      const GoogleString& name, size_t size, MessageHandler* handler);

  // Attaches to an existing segment, which must have been created already.
  // May return NULL on failure
  virtual AbstractSharedMemSegment* AttachToSegment(
      const GoogleString& name, size_t size, MessageHandler* handler);

  virtual void DestroySegment(const GoogleString& name,
                              MessageHandler* handler);

 private:
  DISALLOW_COPY_AND_ASSIGN(NullSharedMem);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_NULL_SHARED_MEM_H_
