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
 *
 * Author: cheesy@google.com (Steve Hill)
 */

#include <vector>

#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/system/redis_cache_cluster_setup.h"

// Lint complains if I put 'using namespace net_instaweb' even in main(), so
// adding this instead.

namespace net_instaweb {
namespace {

int SetupRedisCluster() {
  StringVector node_ids;
  std::vector<int> ports;
  ConnectionList connections;

  // LOGs errors on failure.
  if (RedisCluster::LoadConfiguration(&node_ids, &ports, &connections)) {
    RedisCluster::ResetConfiguration(&node_ids, &ports, &connections);
  } else {
    return 1;
  }

  return 0;
}

}  // namespace
}  // namespace net_instaweb

int main(int argc, char** argv) {
  return net_instaweb::SetupRedisCluster();
}
