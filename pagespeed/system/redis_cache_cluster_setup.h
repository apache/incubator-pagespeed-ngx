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
 * Author: yeputons@google.com (Egor Suvorov)
 */

#ifndef PAGESPEED_SYSTEM_REDIS_CACHE_CLUSTER_SETUP_H_
#define PAGESPEED_SYSTEM_REDIS_CACHE_CLUSTER_SETUP_H_

#include <memory>
#include <vector>

#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/system/tcp_connection_for_testing.h"

namespace net_instaweb {

typedef std::vector<std::unique_ptr<TcpConnectionForTesting>> ConnectionList;

namespace RedisCluster {

// This function checks that node reports cluster as healthy and returns its
// knowledge about cluster configuration. Returns empty vector in case of
// failure.
StringVector GetNodeConfig(TcpConnectionForTesting* conn);

// Reset cluster configuration to our testing default.
//
// TODO(cheesy): node_ids, ports and connections should be collapsed onto a
// single vector of struct { conn, port, node_id }.
void ResetConfiguration(StringVector* node_ids,
                        std::vector<int>* ports,
                        ConnectionList* connections);

// Populate node_ids, ports and connections with values suitable to be passed
// into ResetClusterConfiguration. Config is loaded through environment
// variables REDIS_CLUSTER_PORTS and REDIS_CLUSTER_IDS.
bool LoadConfiguration(StringVector* node_ids,
                       std::vector<int>* ports,
                       ConnectionList* connections);

// Send redis FLUSHALL command, which removes all stored data.
void FlushAll(TcpConnectionForTesting* connection);
void FlushAll(ConnectionList* connections);

}  // namespace RedisCluster

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_REDIS_CACHE_CLUSTER_SETUP_H_
