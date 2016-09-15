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
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/cache/cache_test_base.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/system/tcp_connection_for_testing.h"

namespace net_instaweb {

namespace {
  static const int kReconnectionDelayMs = 10;
  static const int kTimeoutUs = 100 * Timer::kMsUs;
  static const int kSlaveNodesFlushingTimeoutMs = 1000;

  // One can check following constants with CLUSTER KEYSLOT command.
  // For testing purposes, both KEY and {}KEY should be in the same slot range.
  // TODO(yeputons): why?
  static const char kKeyOnNode1[] = "Foobar";        // Slots 0-5499
  static const char kKeyOnNode2[] = "SomeOtherKey";  // Slots 5500-10999
  static const char kKeyOnNode3[] = "Key";           // Slots 11000-16383
}  // namespace

class RedisCacheClusterTest : public CacheTestBase {
 protected:
  RedisCacheClusterTest()
      : thread_system_(Platform::CreateThreadSystem()),
        timer_(new NullMutex, 0) {}

  bool InitRedisClusterOrSkip() {
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

    std::vector<int> ports;
    for (auto port_str : port_strs) {
      int port;
      CHECK(StringToInt(port_str, &port)) << "Invalid port: " << port_str;
      ports.push_back(port);
    }
    nodes_ids_.clear();
    for (StringPiece id : id_strs) {
      nodes_ids_.push_back(id.as_string());
    }

    // Flushing all data on master nodes.
    for (int port : ports) {
      TcpConnectionForTesting conn;
      CHECK(conn.Connect("localhost", port))
          << "Cannot connect to Redis Cluster node";
      conn.Send("FLUSHDB\r\n");
      GoogleString answer = conn.ReadLineCrLf();
      if (StringPiece(answer).starts_with("-READONLY")) {
        // Looks like slave node, skipping.
      } else {
        CHECK_EQ("+OK\r\n", answer)
            << "Redis Cluster node failed to flush all data";
      }
    }

    // Wait until all nodes (including slaves) report zero keys.
    // Typically it happens instantly after flushing master nodes,
    // but we want to be sure.
    PosixTimer timer;
    int64 timeout_at_ms = timer.NowMs() + kSlaveNodesFlushingTimeoutMs;
    for (int port : ports) {
      TcpConnectionForTesting conn;
      CHECK(conn.Connect("localhost", port))
         << "Cannot connect to Redis Cluster node";
      while (true) {
        CHECK_LE(timer.NowMs(), timeout_at_ms)
            << "Redis Cluster node did not flush data in time";
        conn.Send("DBSIZE\r\n");
        GoogleString answer = conn.ReadLineCrLf();
        CHECK(StringPiece(answer).starts_with(":"));
        if (answer == ":0\r\n") {
          break;
        }
        timer.SleepMs(20);
      }
    }

    // Setting up cache.
    cache_.reset(new RedisCache("localhost", ports[0], thread_system_.get(),
                                &handler_, &timer_, kReconnectionDelayMs,
                                kTimeoutUs));
    cache_->StartUp();
    return true;
  }

  CacheInterface* Cache() override { return cache_.get(); }

  scoped_ptr<RedisCache> cache_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockTimer timer_;
  GoogleMessageHandler handler_;

  StringVector nodes_ids_;
};

TEST_F(RedisCacheClusterTest, FirstNodePutGetDelete) {
  if (!InitRedisClusterOrSkip()) {
    return;
  }

  CheckPut(kKeyOnNode1, "Value");
  CheckGet(kKeyOnNode1, "Value");

  CheckDelete(kKeyOnNode1);
  CheckNotFound(kKeyOnNode1);
}

TEST_F(RedisCacheClusterTest, OtherNodesPutGetDelete) {
  if (!InitRedisClusterOrSkip()) {
    return;
  }

  CheckPut(kKeyOnNode2, "Value");
  CheckPut(kKeyOnNode3, "Value");

  CheckGet(kKeyOnNode2, "Value");
  CheckGet(kKeyOnNode3, "Value");

  CheckDelete(kKeyOnNode2);
  CheckDelete(kKeyOnNode3);

  CheckNotFound(kKeyOnNode2);
  CheckNotFound(kKeyOnNode3);
}

}  // namespace net_instaweb
