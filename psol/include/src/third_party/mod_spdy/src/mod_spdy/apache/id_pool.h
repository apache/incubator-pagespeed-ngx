/* Copyright 2012 Google Inc.
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

#ifndef MOD_SPDY_APACHE_ID_POOL_H_
#define MOD_SPDY_APACHE_ID_POOL_H_

#include <vector>
#include <set>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"

namespace mod_spdy {

// A class for managing non-zero 16-bit process-global IDs.
class IdPool {
 public:
  static const uint16 kOverFlowId = 0xFFFF;

  // Returns the one and only instance of the IdPool. Note that one must
  // be created with CreateInstance().
  static IdPool* Instance() { return g_instance; }

  // Call this before threading starts to initialize the instance pointer.
  static void CreateInstance();

  // Call this once you're done with the pool object to delete it.
  static void DestroyInstance();

  // Allocates a new, distinct, non-zero ID. 2^16-2 possible values may be
  // returned; if more than that are needed simultaneously (without being
  // Free()d) kOverFlowId will always be returned.
  uint16 Alloc();

  // Release an ID that's no longer in use, making it available for further
  // calls to Alloc().
  void Free(uint16 id);

 private:
  IdPool();
  ~IdPool();

  static IdPool* g_instance;

  base::Lock mutex_;
  std::vector<uint16> free_list_;  // IDs known to be free
  std::set<uint16> alloc_set_;  // IDs currently in use
  uint16 next_never_used_;  // Next ID we have never returned from Alloc,
                            // for use when the free list is empty.

  DISALLOW_COPY_AND_ASSIGN(IdPool);
};

}  // namespace mod_spdy

#endif  /* MOD_SPDY_APACHE_ID_POOL_H_ */
