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

// Author: yeputons@google.com (Egor Suvorov)

// Unit-test the redis interface in conjunction with Redis Cluster

#include "pagespeed/system/redis_cache_cluster_setup.h"

#include <cstddef>
#include <cstdlib>
#include <algorithm>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/posix_timer.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/system/tcp_connection_for_testing.h"

namespace net_instaweb {

namespace RedisCluster {

namespace {

static const int kReconfigurationPropagationTimeoutMs = 5000;

GoogleString ReadBulkString(TcpConnectionForTesting* conn) {
  GoogleString length_str_storage = conn->ReadLineCrLf();
  StringPiece length_str = length_str_storage;
  // Check that Redis answered with Bulk String
  CHECK(length_str.starts_with("$"));
  length_str.remove_prefix(1);
  CHECK(length_str.ends_with("\r\n"));
  length_str.remove_suffix(2);
  int length;
  CHECK(StringToInt(length_str, &length));
  GoogleString result =  conn->ReadBytes(length);
  CHECK_EQ("\r\n", conn->ReadLineCrLf());
  return result;
}

}  // namespace

// TODO(cheesy): Instead of CLUSTER NODES, CLUSTER SLOTS provides the same
// information in a machine readable format that is already parsed in
// RedisCache::FetchClusterSlotMapping. The CLUSTER NODES part of this code
// could be replaced with a call to the innards of FetchClusterSlotMapping.
// redisReaderCreate and redisReaderGetReply would likely be needed to turn the
// ReadBulkString result into a redisReply. See:
// https://github.com/redis/hiredis/issues/59
StringVector GetNodeConfig(TcpConnectionForTesting* conn) {
  conn->Send("CLUSTER INFO\r\n");
  GoogleString cluster_info = ReadBulkString(conn);
  if (cluster_info.find("cluster_state:ok\r\n") == GoogleString::npos) {
    return {};
  }

  conn->Send("CLUSTER NODES\r\n");
  GoogleString config_csv = ReadBulkString(conn);
  StringPieceVector lines;
  SplitStringPieceToVector(config_csv, "\r\n", &lines,
                            true /* omit_empty_strings */);
  StringVector current_config;
  for (StringPiece line : lines) {
    StringPieceVector fields;
    SplitStringPieceToVector(line, " ", &fields,
                              true /* omit_empty_strings */);
    CHECK_GE(fields.size(), 8);
    GoogleString node_descr;
    // See http://redis.io/commands/cluster-nodes. We take three fields
    // from node description (node id, ip:port, master/slave) plus
    // information about slots served.
    StrAppend(&node_descr, fields[0], " ", fields[1], " ", fields[3]);
    for (auto it = fields.begin() + 8; it != fields.end(); ++it) {
      StrAppend(&node_descr, " ", *it);
    }
    current_config.push_back(node_descr);
  }
  std::sort(current_config.begin(), current_config.end());
  return current_config;
}

void ResetConfiguration(StringVector* node_ids,
                        std::vector<int>* ports,
                        ConnectionList* connections) {
  // TODO(cheesy): These should be collapsed onto a single vector of
  // struct { conn, port, node_id }.
  CHECK_EQ(6, connections->size());
  CHECK_EQ(connections->size(), ports->size());
  CHECK_EQ(connections->size(), node_ids->size());

  LOG(INFO) << "Resetting Redis Cluster configuration back to default";

  // Flush the nodes which is required to re-configure the cluster.
  FlushAll(connections);

  // Reset all nodes.
  for (auto& conn : *connections) {
    conn->Send("CLUSTER RESET SOFT\r\n");
  }
  for (auto& conn : *connections) {
    CHECK_EQ("+OK\r\n", conn->ReadLineCrLf());
  }

  // Now make nodes know about each other.
  for (auto& conn : *connections) {
    for (int port : *ports) {
      conn->Send(
          StrCat("CLUSTER MEET 127.0.0.1 ", IntegerToString(port), "\r\n"));
    }
    for (int i = 0, n = ports->size(); i < n; ++i) {
      CHECK_EQ("+OK\r\n", conn->ReadLineCrLf());
    }
  }

  // And finally load slots configuration.
  // Some of these boundaries are explicitly probed in the SlotBoundaries
  // test. If you change the cluster layout, you must also change that test.
  static const int slot_ranges[] = { 0, 5500, 11000, 16384 };
  for (int i = 0; i < 3; i++) {
    GoogleString command = "CLUSTER ADDSLOTS";
    for (int slot = slot_ranges[i]; slot < slot_ranges[i + 1]; slot++) {
      StrAppend(&command, " ", IntegerToString(slot));
    }
    StrAppend(&command, "\r\n");

    auto& conn = (*connections)[i];
    conn->Send(command);
    CHECK_EQ("+OK\r\n", conn->ReadLineCrLf());
  }

  // Nodes learn about each other asynchronously in response to CLUSTER MEET
  // above, but if the system hasn't yet converged, REPLICATE will fail. We
  // poll the cluster config with GetNodeClusterConfig() until every node
  // knows about every other node.

  LOG(INFO) << "Reset Redis Cluster configuration back to default, "
                "waiting for node propagation...";

  PosixTimer timer;
  int64 timeout_at_ms = timer.NowMs() + kReconfigurationPropagationTimeoutMs;

  bool propagated = false;
  while (!propagated && timer.NowMs() < timeout_at_ms) {
    size_t num_complete = 0;
    // For every connection, pull the node config and verify that it sees
    // the right number of nodes.
    for (auto& conn : *connections) {
      StringVector config = GetNodeConfig(conn.get());
      if (config.size() == connections->size()) {
        ++num_complete;
      } else {
        break;
      }
    }
    if (num_complete == connections->size()) {
      propagated = true;
    } else {
      timer.SleepMs(50);
    }
  }

  CHECK(propagated) << "All nodes did not report in after CLUSTER MEET";

  for (int i = 3; i < 6; i++) {
    auto& conn = (*connections)[i];
    conn->Send(StrCat("CLUSTER REPLICATE ", (*node_ids)[i - 3], "\r\n"));
    CHECK_EQ("+OK\r\n", conn->ReadLineCrLf());
  }

  // Now wait until all nodes report cluster as healthy and report same
  // cluster configuration.
  LOG(INFO) << "Reset Redis Cluster configuration back to default, "
                "waiting for slot propagation...";
  timeout_at_ms = timer.NowMs() + kReconfigurationPropagationTimeoutMs;
  bool cluster_is_up = false;
  while (!cluster_is_up) {
    CHECK_LE(timer.NowMs(), timeout_at_ms)
        << "Redis Cluster configuration did not propagate in time";

    StringVector first_node_config;
    for (auto& conn : *connections) {
      StringVector current_config = GetNodeConfig(conn.get());
      if (current_config.empty() ||
          current_config.size() != connections->size()) {
        break;
      }
      // Check configs are the same on all nodes.
      cluster_is_up = true;
      if (first_node_config.empty()) {
        first_node_config = current_config;
      } else if (first_node_config != current_config) {
        cluster_is_up = false;
        break;
      }
    }
    if (!cluster_is_up) {
      timer.SleepMs(50);
    }
  }
  LOG(INFO) << "Redis Cluster is reset";
}

bool LoadConfiguration(StringVector* node_ids,
                       std::vector<int>* ports,
                        ConnectionList* connections) {
  // Parsing environment variables.
  // TODO(cheesy): We should discover the cluster IDs by querying the ports,
  // and not rely on the shell to set REDIS_CLUSTER_IDS for us.
  const char* ports_env = getenv("REDIS_CLUSTER_PORTS");
  const char* ids_env = getenv("REDIS_CLUSTER_IDS");
  if (!ports_env && !ids_env) {
    LOG(ERROR) << "Env variables REDIS_CLUSTER_* are not set. Use "
                << "install/run_program_with_redis_cluster.sh for running "
                << "these tests. Do not use real cluster; ALL DATA WILL "
                << "BE ERASED DURING TESTS!";
    return false;
  }
  CHECK(ports_env) << "Env variable REDIS_CLUSTER_PORTS is unspecified";
  CHECK(ids_env) << "Env variable REDIS_CLUSTER_IDS is unspecified";

  StringPieceVector port_strs;
  StringPieceVector id_strs;
  SplitStringPieceToVector(ports_env, " ", &port_strs,
                            /* omit_empty_strings */ true);
  SplitStringPieceToVector(ids_env, " ", &id_strs,
                            /* omit_empty_strings */ true);
  CHECK_EQ(port_strs.size(), id_strs.size()) << "REDIS_CLUSTER_PORTS and "
                                                "REDIS_CLUSTER_IDS have "
                                                "different amount of items";
  CHECK_EQ(port_strs.size(), 6) << "Six Redis Cluster nodes are expected";

  for (auto port_str : port_strs) {
    int port;
    CHECK(StringToInt(port_str, &port)) << "Invalid port: " << port_str;
    ports->push_back(port);
  }
  for (StringPiece id : id_strs) {
    node_ids->push_back(id.as_string());
  }

  for (int port : *ports) {
    connections->emplace_back(new TcpConnectionForTesting());
    CHECK(connections->back()->Connect("localhost", port))
        << "Cannot connect to Redis Cluster node";
  }
  return true;
}

void FlushAll(TcpConnectionForTesting* conn) {
  conn->Send("FLUSHALL\r\n");
  GoogleString flushall_reply = conn->ReadLineCrLf();
  // We'll get READONLY from slave nodes, which isn't a problem.
  CHECK(flushall_reply == "+OK\r\n" ||
        strings::StartsWith(flushall_reply, "-READONLY"));
}

void FlushAll(ConnectionList* connections) {
  for (auto& conn : *connections) {
    FlushAll(conn.get());
  }
}

}  // namespace RedisCluster

}  // namespace net_instaweb
