/*
 * Copyright 2010 Google Inc.
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

// Author: abliss@google.com (Adam Bliss)

#ifndef NET_INSTAWEB_REWRITER_RESOURCE_MANAGER_TESTING_PEER_H_
#define NET_INSTAWEB_REWRITER_RESOURCE_MANAGER_TESTING_PEER_H_

#include "net/instaweb/rewriter/public/output_resource.h"

namespace net_instaweb {

class ResourceManagerTestingPeer {
 public:
  ResourceManagerTestingPeer() { }

  static bool HasHash(const OutputResource* resource) {
    return resource->has_hash();
  }
  static bool Outlined(const OutputResource* resource) {
    return resource->outlined();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceManagerTestingPeer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_RESOURCE_MANAGER_TESTING_PEER_H_
