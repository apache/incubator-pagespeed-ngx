// Copyright 2012 Google Inc.
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
//         jmarantz@google.com (Joshua Marantz)  (refactoring only)

#include "pagespeed/system/apr_thread_compatible_pool.h"

#include <cstddef>

#include "apr_pools.h"
#include "apr_thread_mutex.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/stack_buffer.h"

namespace net_instaweb {

apr_pool_t* AprCreateThreadCompatiblePool(apr_pool_t* parent_pool) {
  // Creates a pool that can be used in any thread, even when run in
  // Apache prefork.
  //
  // 1) Concurrent allocations from the same pools are not (thread)safe.
  // 2) Concurrent allocations from different pools using the same allocator
  //    are not safe unless the allocator has a mutex set.
  // 3) prefork's pchild pool (which is our ancestor) has an allocator without
  //    a mutex set.
  //
  // Note: the above is all about the release version of the pool code, the
  // checking one has some additional locking!
  apr_pool_t* pool = NULL;
  apr_allocator_t* allocator = NULL;
  CHECK(apr_allocator_create(&allocator) == APR_SUCCESS);
  apr_status_t status =
      apr_pool_create_ex(&pool, parent_pool, NULL /*abortfn*/, allocator);
  if ((status != APR_SUCCESS) || (pool == NULL)) {
    char buf[kStackBufferSize];
    apr_strerror(status, buf, sizeof(buf));
    CHECK_EQ(APR_SUCCESS, status) << "apr_pool_create_ex failed: " << buf;
    CHECK(pool != NULL) << "apr_pool_create_ex failed: " << buf;
  }
  apr_allocator_owner_set(allocator, pool);
  apr_thread_mutex_t* mutex;
  CHECK(apr_thread_mutex_create(&mutex, APR_THREAD_MUTEX_DEFAULT, pool) ==
        APR_SUCCESS);
  apr_allocator_mutex_set(allocator, mutex);
  return pool;
}

}  // namespace net_instaweb
