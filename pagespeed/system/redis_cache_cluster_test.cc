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

#include "pagespeed/system/redis_cache.h"

#include <cstdlib>
#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/posix_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/cache/cache_test_base.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/util/simple_stats.h"
#include "pagespeed/system/tcp_connection_for_testing.h"

namespace net_instaweb {

namespace {
  static const int kReconnectionDelayMs = 10;
  static const int kTimeoutUs = 100 * Timer::kMsUs;
  static const int kSlaveNodesFlushingTimeoutMs = 1000;
  static const int kReconfigurationPropagationTimeoutMs = 5000;

  // One can check following constants with CLUSTER KEYSLOT command.
  // For testing purposes, both KEY and {}KEY should be in the same slot range.
  // Implementation may or may not prepend {} to all keys processed to avoid
  // keys distribution due to hash tags. We want tests to work in both
  // situations. See http://redis.io/topics/cluster-spec#keys-hash-tags.
  //
  // TODO(yeputons): add static assertion that these keys really belong to
  // corresponding slots.
  static const char kKeyOnNode1[] = "Foobar";        // Slots 0-5499
  static const char kKeyOnNode1b[] = "Coolkey";      // Slots 0-5499
  static const char kKeyOnNode2[] = "SomeOtherKey";  // Slots 5500-10999
  static const char kKeyOnNode3[] = "Key";           // Slots 11000-16383
  static const char kValue1[] = "Value1";
  static const char kValue2[] = "Value2";
  static const char kValue3[] = "Value3";
  static const char kValue4[] = "Value4";
}  // namespace

typedef std::vector<std::unique_ptr<TcpConnectionForTesting>> ConnectionList;

class RedisCacheClusterTest : public CacheTestBase {
 protected:
  RedisCacheClusterTest()
      : thread_system_(Platform::CreateThreadSystem()),
        statistics_(thread_system_.get()),
        timer_(new NullMutex, 0) {
    RedisCache::InitStats(&statistics_);
  }

  static void SetUpTestCase() {
    StringVector node_ids;
    std::vector<int> ports;
    ConnectionList connections;

    if (LoadClusterConfiguration(&node_ids, &ports, &connections)) {
      ResetClusterConfiguration(&node_ids, &ports, &connections);
    }
  }

  // This function checks that node reports cluster as healthy and returns its
  // knowledge about cluster configuration. Returns empty vector in case of
  // failure.
  // TODO(yeputons): there is alternative command CLUSTER SLOTS which will
  // produces better machine-readable result which we will have to parse inside
  // RedisCache anyway to load cluster map in some C++ structure in advance.
  // We can probably re-use that parser here. You may find hiredis' functions
  // redisReaderCreate, redisReaderFree and redisReaderGetReply useful.
  static StringVector GetNodeClusterConfig(TcpConnectionForTesting* conn) {
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
      for (int i = 8; i < fields.size(); i++) {
        StrAppend(&node_descr, " ", fields[i]);
      }
      current_config.push_back(node_descr);
    }
    std::sort(current_config.begin(), current_config.end());
    return current_config;
  }

  // Reset cluster configuration to our testing default.  This is duplicated in
  // run_program_with_redis_cluster.sh for manual testing, and any changes here
  // should be copied to there.
  //
  // TODO(jefftk): We should have a binary target that sets up the cluster for
  // tests, and then run_program_with_redis_cluster.sh can call that binary.
  // Then we don't have to duplicate the configuration.
  static void ResetClusterConfiguration(StringVector* node_ids,
                                       std::vector<int>* ports,
                                       ConnectionList* connections) {
    // TODO(cheesy): These should be collapsed onto a single vector of
    // struct { conn, port, node_id }.
    CHECK_EQ(6, connections->size());
    CHECK_EQ(connections->size(), ports->size());
    CHECK_EQ(connections->size(), node_ids->size());

    LOG(INFO) << "Resetting Redis Cluster configuration back to default";

    // First, flush all data from the cluster and reset all nodes.
    for (auto& conn : *connections) {
      conn->Send("FLUSHALL\r\nCLUSTER RESET SOFT\r\n");
    }
    for (auto& conn : *connections) {
      GoogleString flushall_reply = conn->ReadLineCrLf();
      // We'll get READONLY from slave nodes, which isn't a problem.
      CHECK(flushall_reply == "+OK\r\n" ||
            StringPiece(flushall_reply).starts_with("-READONLY"));
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
      int num_complete = 0;
      // For every connection, pull the node config and verify that it sees
      // the right number of nodes.
      for (auto& conn : *connections) {
        StringVector config = GetNodeClusterConfig(conn.get());
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
      cluster_is_up = true;
      for (auto& conn : *connections) {
        StringVector current_config = GetNodeClusterConfig(conn.get());
        CHECK_EQ(current_config.size(), connections->size());
        if (current_config.empty()) {
          cluster_is_up = false;
          break;
        }
        // Check configs are the same on all nodes.
        if (first_node_config.empty()) {
          first_node_config = current_config;
        }
        if (first_node_config != current_config) {
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

  static bool LoadClusterConfiguration(StringVector* node_ids,
                                       std::vector<int>* ports,
                                       ConnectionList* connections) {
    // Parsing environment variables.
    const char* ports_env = getenv("REDIS_CLUSTER_PORTS");
    const char* ids_env = getenv("REDIS_CLUSTER_IDS");
    if (!ports_env && !ids_env) {
      LOG(ERROR) << "Env variables REDIS_CLUSTER_* are not set, skipping"
                 << "RedisCacheClusterTest.* altogether. Use "
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

  bool InitRedisClusterOrSkip() {
    if (!LoadClusterConfiguration(&node_ids_, &ports_, &connections_)) {
      return false;  // Already logged an error.
    }

    // Setting up cache.
    cache_.reset(new RedisCache("localhost", ports_[0], thread_system_.get(),
                                &handler_, &timer_, kReconnectionDelayMs,
                                kTimeoutUs, &statistics_));
    cache_->StartUp();
    return true;
  }

  static GoogleString ReadBulkString(TcpConnectionForTesting* conn) {
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

  CacheInterface* Cache() override { return cache_.get(); }

  scoped_ptr<RedisCache> cache_;
  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats statistics_;
  MockTimer timer_;
  GoogleMessageHandler handler_;

  StringVector node_ids_;
  std::vector<int> ports_;
  ConnectionList connections_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RedisCacheClusterTest);
};

TEST_F(RedisCacheClusterTest, HashSlot) {
  // Expected crc16 hashes taken from running RedisClusterCRC16.crc16 from
  // https://github.com/antirez/redis-rb-cluster/blob/master/crc16.rb

  EXPECT_EQ(15332, RedisCache::HashSlot("hello world"));

  // If there's curly brace section, only that section is considered for the
  // key.
  EXPECT_EQ(7855, RedisCache::HashSlot("curly"));
  EXPECT_EQ(7855, RedisCache::HashSlot("hello {curly} world"));
  // Only take the first such section.
  EXPECT_EQ(7855, RedisCache::HashSlot("hello {curly} world {ignored}"));
  // Any other junk doesn't matter.
  EXPECT_EQ(7855, RedisCache::HashSlot(
      "hello {curly} world {nothing here matters"));
  EXPECT_EQ(7855, RedisCache::HashSlot(
      "}}} hello {curly} world {nothing else matters"));
  // Incomplete curlies are ignored.
  EXPECT_EQ(8673, RedisCache::HashSlot("hello {curly world"));
  EXPECT_EQ(950, RedisCache::HashSlot("hello }curly{ world"));
  EXPECT_EQ(3940, RedisCache::HashSlot("hello curly world{"));
  // Empty string is fine.
  EXPECT_EQ(0, RedisCache::HashSlot(""));
  // While {a} means to only consider a, {} means consider the whole message
  // when hashing.  (Otherwise this would return 0, the hash of "".)
  EXPECT_EQ(13934, RedisCache::HashSlot("hello {} world"));
  // After an empty curly, all other curlies are still ignored.  (Otherwise
  // this would return 7855.)
  EXPECT_EQ(2795, RedisCache::HashSlot("{}hello {curly} world"));
}

TEST_F(RedisCacheClusterTest, FirstNodePutGetDelete) {
  if (!InitRedisClusterOrSkip()) {
    return;
  }

  CheckPut(kKeyOnNode1, kValue1);
  CheckGet(kKeyOnNode1, kValue1);

  CheckDelete(kKeyOnNode1);
  CheckNotFound(kKeyOnNode1);

  // All requests are for node1, which is the main node, so we should never be
  // redirected or have to fetch slots.
  EXPECT_EQ(0, cache_->Redirections());
  EXPECT_EQ(0, cache_->ClusterSlotsFetches());
}

TEST_F(RedisCacheClusterTest, OtherNodesPutGetDelete) {
  if (!InitRedisClusterOrSkip()) {
    return;
  }

  CheckPut(kKeyOnNode2, kValue1);
  // This should have redirected us from node1 to node2, and prompted us to
  // update our cluster map.
  EXPECT_EQ(1, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());

  CheckPut(kKeyOnNode3, kValue2);

  CheckGet(kKeyOnNode2, kValue1);
  CheckGet(kKeyOnNode3, kValue2);

  CheckDelete(kKeyOnNode2);
  CheckDelete(kKeyOnNode3);

  CheckNotFound(kKeyOnNode2);
  CheckNotFound(kKeyOnNode3);

  // No more redirections or slots fetches triggered after the first one above.
  EXPECT_EQ(1, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());
}

TEST_F(RedisCacheClusterTest, SlotBoundaries) {
  // These are designed to exercise the slot lookup code at slot boundaries.
  // 0 and 16384 are min/max slot. Slot 10999 is on node 2 and 11000 is on node
  // 3.
  const char kHashesTo0[] = "";
  const char kHashesTo10999[] = "AFKb";
  const char kHashesTo11000[] = "PNP";
  const char kHashesTo16383[] = "C0p";

  if (!InitRedisClusterOrSkip()) {
    return;
  }

  ASSERT_EQ(0, RedisCache::HashSlot(kHashesTo0));
  ASSERT_EQ(10999, RedisCache::HashSlot(kHashesTo10999));
  ASSERT_EQ(11000, RedisCache::HashSlot(kHashesTo11000));
  ASSERT_EQ(16383, RedisCache::HashSlot(kHashesTo16383));

  // Do one lookup with a redirection, to prime the table.
  CheckPut(kKeyOnNode2, kValue1);
  EXPECT_EQ(1, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());

  for (const GoogleString& key :
       {kHashesTo0, kHashesTo10999, kHashesTo11000, kHashesTo16383}) {
    CheckPut(key, key);
    CheckGet(key, key);

    // If our cluster lookup code is correct, there shouldn't be any
    // redirections.
    EXPECT_EQ(1, cache_->Redirections()) << " for key " << key;
    EXPECT_EQ(1, cache_->ClusterSlotsFetches()) << " for key " << key;
  }
}

int CountSubstring(const GoogleString& haystack, const GoogleString& needle) {
  size_t pos = -1;
  int count = 0;
  while (true) {
    pos = haystack.find(needle, pos+1);
    if (pos == haystack.npos) {
      return count;
    }
    count++;
  }
}

TEST_F(RedisCacheClusterTest, GetStatus) {
  if (!InitRedisClusterOrSkip()) {
    return;
  }

  // We're only connected to the main node right now.
  GoogleString status;
  cache_->GetStatus(&status);
  EXPECT_EQ(1, CountSubstring(status, "redis_version:"));
  EXPECT_EQ(1, CountSubstring(status, "connected_clients:"));

  CheckPut(kKeyOnNode1, kValue1);

  // Still only on the main node.
  status.clear();
  cache_->GetStatus(&status);
  EXPECT_EQ(1, CountSubstring(status, "redis_version:"));
  EXPECT_EQ(1, CountSubstring(status, "connected_clients:"));

  CheckPut(kKeyOnNode2, kValue2);
  CheckPut(kKeyOnNode3, kValue1);

  // Now we're connected to all the nodes.
  status.clear();
  cache_->GetStatus(&status);
  LOG(INFO) << status;
  // Either three or four is ok here, because the connections map isn't fully
  // deduplicated.  Specifically, when we originally connect to redis we do it
  // by some name (host:port) and then when we learn about other nodes they
  // have other names (ip1:port1, ip2:port2, ...)  We can often learn about the
  // original node by whatever IP redis uses for it instead of the hostname or
  // IP we originally used for it, in which case we'll get a single duplicate
  // connection.  It would be possible to fix this by paying attention to node
  // ids, which newer versions of redis cluster give you, but it would be kind
  // of a pain just to reduce our connection count by 1.
  EXPECT_LE(3, CountSubstring(status, "redis_version:"));
  EXPECT_GE(4, CountSubstring(status, "redis_version:"));
  EXPECT_LE(3, CountSubstring(status, "connected_clients:"));
  EXPECT_GE(4, CountSubstring(status, "connected_clients:"));
}

class RedisCacheClusterTestWithReconfiguration : public RedisCacheClusterTest {
 protected:
  void TearDown() override {
    if (!connections_.empty()) {
      ResetClusterConfiguration(&node_ids_, &ports_, &connections_);
    }
  }
};

TEST_F(RedisCacheClusterTestWithReconfiguration, HandlesMigrations) {
  if (!InitRedisClusterOrSkip()) {
    return;
  }

  LOG(INFO) << "Putting value on the first node";
  CheckPut(kKeyOnNode1, kValue1);
  CheckPut(kKeyOnNode1b, kValue2);
  CheckGet(kKeyOnNode1, kValue1);
  CheckGet(kKeyOnNode1b, kValue2);

  // No redirections or slot fetches needed.
  EXPECT_EQ(0, cache_->Redirections());
  EXPECT_EQ(0, cache_->ClusterSlotsFetches());

  // Now trigger a redirection and slot fetch.
  CheckPut(kKeyOnNode3, kValue3);
  CheckGet(kKeyOnNode3, kValue3);
  EXPECT_EQ(1, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());

  LOG(INFO) << "Starting migration of the first node";
  for (int i = 0; i < 5000; i++) {
    connections_[1]->Send(StrCat("CLUSTER SETSLOT ", IntegerToString(i),
                                 " IMPORTING ", node_ids_[0], "\r\n"));
  }
  for (int i = 0; i < 5000; i++) {
    CHECK_EQ("+OK\r\n", connections_[1]->ReadLineCrLf());
  }
  for (int i = 0; i < 5000; i++) {
    connections_[0]->Send(StrCat("CLUSTER SETSLOT ", IntegerToString(i),
                                 " MIGRATING ", node_ids_[1], "\r\n"));
  }
  for (int i = 0; i < 5000; i++) {
    CHECK_EQ("+OK\r\n", connections_[0]->ReadLineCrLf());
  }

  LOG(INFO) << "Checking availability before actually moving the key";
  // The key should still be available on the first node, where it was.
  CheckGet(kKeyOnNode1, kValue1);
  CheckPut(kKeyOnNode1, kValue2);
  CheckGet(kKeyOnNode1, kValue2);

  // No additional redirects or slot fetches.
  EXPECT_EQ(1, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());

  connections_[0]->Send(StrCat("MIGRATE 127.0.0.1 ", IntegerToString(ports_[1]),
                               " ", kKeyOnNode1, " 0 5000\r\n"));
  CHECK_EQ("+OK\r\n", connections_[0]->ReadLineCrLf());

  LOG(INFO) << "Checking availability after actually moving the key";
  // This is ugly: because we moved the key and now it's not where it should be
  // for the slot it's in, we see redirections with ASK on every
  // interaction. They're ASKs, though, so they're just temporary and we
  // shouldn't reload mappings.
  CheckGet(kKeyOnNode1, kValue2);
  EXPECT_EQ(2, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());

  CheckPut(kKeyOnNode1, kValue3);
  EXPECT_EQ(3, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());

  CheckGet(kKeyOnNode1, kValue3);
  EXPECT_EQ(4, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());

  // But not for the second key, which is still on the first node.
  CheckGet(kKeyOnNode1b, kValue2);
  CheckPut(kKeyOnNode1b, kValue3);
  CheckGet(kKeyOnNode1b, kValue3);
  EXPECT_EQ(4, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());

  LOG(INFO) << "Moving the second key as well";
  connections_[0]->Send(StrCat("MIGRATE 127.0.0.1 ", IntegerToString(ports_[1]),
                               " ", kKeyOnNode1b, " 0 5000\r\n"));
  CHECK_EQ("+OK\r\n", connections_[0]->ReadLineCrLf());

  LOG(INFO) << "Ending migration";
  for (int c = 0; c < 3; c++) {
    auto &conn = connections_[c];
    for (int i = 0; i < 5000; i++) {
      conn->Send(StrCat("CLUSTER SETSLOT ", IntegerToString(i), " NODE ",
                        node_ids_[1], "\r\n"));
    }
    for (int i = 0; i < 5000; i++) {
      CHECK_EQ("+OK\r\n", conn->ReadLineCrLf());
    }
  }

  LOG(INFO) << "Checking availability after migration";
  CheckGet(kKeyOnNode1, kValue3);
  // Now that the migration is complete and we've called SETSLOT we'll get a
  // MOVED instead of an ASK, so we'll fetch slots.
  EXPECT_EQ(5, cache_->Redirections());
  EXPECT_EQ(2, cache_->ClusterSlotsFetches());

  CheckPut(kKeyOnNode1, kValue4);
  CheckGet(kKeyOnNode1, kValue4);

  CheckGet(kKeyOnNode1b, kValue3);
  CheckPut(kKeyOnNode1b, kValue4);
  CheckGet(kKeyOnNode1b, kValue4);

  // No more redirections or slots fetches.
  EXPECT_EQ(5, cache_->Redirections());
  EXPECT_EQ(2, cache_->ClusterSlotsFetches());
}

}  // namespace net_instaweb
