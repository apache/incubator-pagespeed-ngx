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

#ifndef PAGESPEED_SYSTEM_APR_THREAD_COMPATIBLE_POOL_H_
#define PAGESPEED_SYSTEM_APR_THREAD_COMPATIBLE_POOL_H_

#include "apr_pools.h"

namespace net_instaweb {

// Creates a pool that can be used in any thread, even when run in
// Apache prefork.
//
// 1) This method must be called from startup phase only
// 2) Each pool must be accessed only from a single thread (or otherwise
//    have its access serialized)
// 3) Different pools returned by this function may be safely used concurrently.
// 4) It's OK to just use ap_pool_create to create child pools of this one from
//    multiple threads; those will be re-entrant too (but pools created merely
//    as children of Apache's pools will not be reentrant in prefork)
//
// In short, pools returned by this method are not fully threadsafe, but
// at least they are not thread-hostile, which is what you get with
// apr_pool_create in Prefork.
//
// Note: the above is all about the release version of the pool code, the
// checking one has some additional locking!
//
// WARNING: you must not call apr_pool_clear on the returned pool.  The
// returned pool can be used to create sub-pools that can be accessed
// in distinct threads, due to a mutex injected into the allocator.
// However, if you call apr_pool_clear on the returned pool, the allocator's
// mutex will be freed and the pointer to it will be dangling.  Subsequent
// allocations are likely to crash.
apr_pool_t* AprCreateThreadCompatiblePool(apr_pool_t* parent_pool);

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_APR_THREAD_COMPATIBLE_POOL_H_
