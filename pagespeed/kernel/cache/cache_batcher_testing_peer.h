/*
 * Copyright 2016 Google Inc.
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

// Author: cfredric@google.com (Chris Fredrickson)

#ifndef PAGESPEED_KERNEL_CACHE_CACHE_BATCHER_TESTING_PEER_H_
#define PAGESPEED_KERNEL_CACHE_CACHE_BATCHER_TESTING_PEER_H_

#include "pagespeed/kernel/cache/cache_batcher.h"

#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

class CacheBatcherTestingPeer {
 public:
  CacheBatcherTestingPeer() { }

  static int last_batch_size(CacheBatcher* batcher) {
    return batcher->last_batch_size();
  }

  static int num_in_flight_keys(CacheBatcher* batcher) {
    return batcher->num_in_flight_keys();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheBatcherTestingPeer);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_CACHE_CACHE_BATCHER_TESTING_PEER_H_
