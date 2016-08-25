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

// Unit-test the redis interface.

#include "pagespeed/system/redis_cache.h"

#include <cstdlib>

#include "apr_network_io.h"  // NOLINT
#include "base/logging.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/cache/cache_test_base.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/system/tcp_server_thread_for_testing.h"

namespace {
  static const int kReconnectionDelayMs = 10;
  static const char kSomeKey[] = "SomeKey";
  static const char kSomeValue[] = "SomeValue";
}

namespace net_instaweb {

using testing::HasSubstr;


// TODO(yeputons): refactor this class with AprMemCacheTest, see details in
// apr_mem_cache_test.cc
class RedisCacheTest : public CacheTestBase {
 protected:
  RedisCacheTest()
      : thread_system_(Platform::CreateThreadSystem()),
        timer_(new NullMutex, 0) {}

  bool InitRedisOrSkip() {
    const char* portString = getenv("REDIS_PORT");
    int port;
    if (portString == nullptr || !StringToInt(portString, &port)) {
      LOG(ERROR) << "RedisCache tests are skipped because env var "
                 << "$REDIS_PORT is not set to an integer. Set that "
                 << "to the port number where redis is running to "
                 << "enable the tests. See install/run_program_with_redis.sh";
      return false;
    }

    cache_.reset(new RedisCache("localhost", port, new NullMutex, &handler_,
                                &timer_, kReconnectionDelayMs));
    cache_->StartUp();
    return cache_->FlushAll();
  }

  CacheInterface* Cache() override { return cache_.get(); }

  scoped_ptr<RedisCache> cache_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockTimer timer_;
  GoogleMessageHandler handler_;
};

// Simple flow of putting in an item, getting it, deleting it.
TEST_F(RedisCacheTest, PutGetDelete) {
  if (!InitRedisOrSkip()) {
    return;
  }
  CheckPut("Name", "Value");
  CheckGet("Name", "Value");
  CheckNotFound("Another Name");

  CheckPut("Name", "NewValue");
  CheckGet("Name", "NewValue");

  CheckDelete("Name");
  CheckNotFound("Name");
}

TEST_F(RedisCacheTest, MultiGet) {
  if (!InitRedisOrSkip()) {
    return;
  }
  TestMultiGet();  // Test from CacheTestBase is just fine.
}

TEST_F(RedisCacheTest, BasicInvalid) {
  if (!InitRedisOrSkip()) {
    return;
  }

  // Check that we honor callback veto on validity.
  CheckPut("nameA", "valueA");
  CheckPut("nameB", "valueB");
  CheckGet("nameA", "valueA");
  CheckGet("nameB", "valueB");
  set_invalid_value("valueA");
  CheckNotFound("nameA");
  CheckGet("nameB", "valueB");
}

TEST_F(RedisCacheTest, GetStatus) {
  if (!InitRedisOrSkip()) {
    return;
  }

  GoogleString status;
  ASSERT_TRUE(cache_->GetStatus(&status));

  // Check that some reasonable info is present.
  EXPECT_THAT(status, HasSubstr(cache_->ServerDescription()));
  EXPECT_THAT(status, HasSubstr("redis_version:"));
  EXPECT_THAT(status, HasSubstr("connected_clients:"));
  EXPECT_THAT(status, HasSubstr("tcp_port:"));
  EXPECT_THAT(status, HasSubstr("used_memory:"));
}

TEST_F(RedisCacheTest, FlushAll) {
  if (!InitRedisOrSkip()) {
    return;
  }

  CheckPut("Name1", "Value1");
  CheckPut("Name2", "Value2");
  cache_->FlushAll();
  CheckNotFound("Name1");
  CheckNotFound("Name2");
}

// Two following tests are identical and ensure that no keys are leaked between
// tests through shared running Redis server.
TEST_F(RedisCacheTest, TestsAreIsolated1) {
  if (!InitRedisOrSkip()) {
    return;
  }

  CheckNotFound(kSomeKey);
  CheckPut(kSomeKey, kSomeValue);
}

TEST_F(RedisCacheTest, TestsAreIsolated2) {
  if (!InitRedisOrSkip()) {
    return;
  }

  CheckNotFound(kSomeKey);
  CheckPut(kSomeKey, kSomeValue);
}

class RedisGetRespondingServerThread : public TcpServerThreadForTesting {
 public:
  RedisGetRespondingServerThread(apr_port_t listen_port,
                                 ThreadSystem* thread_system)
      : TcpServerThreadForTesting(listen_port, "redis_get_answering_server",
                                  thread_system) {}

  virtual ~RedisGetRespondingServerThread() { ShutDown(); }

 private:
  void HandleClientConnection(apr_socket_t* sock) override {
    // See http://redis.io/topics/protocol for details. Request is an array of
    // two bulk strings, answer for GET is a single bulk string.
    static const char kRequest[] =
        "*2\r\n"
        "$3\r\nGET\r\n"
        "$7\r\nSomeKey\r\n";
    static const char kAnswer[] = "$9\r\nSomeValue\r\n";
    apr_size_t answer_size  = STATIC_STRLEN(kAnswer);

    char buf[STATIC_STRLEN(kRequest) + 1];
    apr_size_t size = sizeof(buf) - 1;

    apr_socket_recv(sock, buf, &size);
    EXPECT_EQ(STATIC_STRLEN(kRequest), size);
    buf[size] = 0;
    EXPECT_STREQ(kRequest, buf);

    apr_socket_send(sock, kAnswer, &answer_size);
    apr_socket_close(sock);
  }
};

class RedisCacheReconnectTest : public RedisCacheTest {
 public:
  void SetUp() {
    TcpServerThreadForTesting::PickListenPortOnce(&port_);
    CHECK_NE(port_, 0);

    cache_.reset(new RedisCache("localhost", port_, new NullMutex, &handler_,
                                &timer_, kReconnectionDelayMs));
  }

 protected:
  void ProcessOneMoreConnection() {
    // If there was an old server, we wait till it finishes as
    // TcpServerThreadForTesting calls Join in destructor.
    server_.reset(
        new RedisGetRespondingServerThread(port_, thread_system_.get()));
    ASSERT_TRUE(server_->Start());
    // Wait while server starts and check its port
    ASSERT_EQ(port_, server_->GetListeningPort());
  }

  void WaitForServerShutdown() {
    server_.reset();
  }

 private:
  apr_port_t port_;
  scoped_ptr<RedisGetRespondingServerThread> server_;
};

TEST_F(RedisCacheReconnectTest, ReconnectsInstantly) {
  ProcessOneMoreConnection();
  cache_->StartUp();

  CheckGet(kSomeKey, kSomeValue);
  // Server closes connection after processing one request, but cache does not
  // know about that yet.
  WaitForServerShutdown();
  EXPECT_TRUE(Cache()->IsHealthy());

  // Client should not reconnect as it learns about disconnection only when it
  // tries to run the command.
  ProcessOneMoreConnection();  // Should not receive connection right now.
  CheckNotFound(kSomeKey);

  // First reconnection attempt should happen right away.
  EXPECT_TRUE(Cache()->IsHealthy());  // Allow reconnection.
  CheckGet(kSomeKey, kSomeValue);
}

TEST_F(RedisCacheReconnectTest, ReconnectsUntilSuccessWithTimeout) {
  ProcessOneMoreConnection();
  cache_->StartUp();

  CheckGet(kSomeKey, kSomeValue);
  // Server closes connection after processing one request, but cache does not
  // know about that yet.
  WaitForServerShutdown();
  EXPECT_TRUE(Cache()->IsHealthy());

  // Let client know that we're disconnected by trying to read.
  CheckNotFound(kSomeKey);

  // Try to reconnect right away after failure.
  EXPECT_TRUE(Cache()->IsHealthy());  // Reconnection is allowed...
  CheckNotFound(kSomeKey);  // ...but it fails.

  // Second attempt, should not reconnect before timeout.
  ProcessOneMoreConnection();  // Should not receive connection right now.
  timer_.AdvanceMs(kReconnectionDelayMs - 1);
  EXPECT_FALSE(Cache()->IsHealthy());  // Reconnection is not allowed.
  CheckNotFound(kSomeKey);

  // Should reconnect after timeout passes.
  timer_.AdvanceMs(1);
  EXPECT_TRUE(Cache()->IsHealthy());  // Reconnection is allowed.
  CheckGet(kSomeKey, kSomeValue);
}

TEST_F(RedisCacheReconnectTest, ReconnectsIfStartUpFailed) {
  cache_->StartUp();

  // Client already knows that connection failed.
  EXPECT_FALSE(Cache()->IsHealthy());
  CheckNotFound(kSomeKey);

  // Should not reconnect before timeout.
  ProcessOneMoreConnection();  // Should not receive connection right now.
  timer_.AdvanceMs(kReconnectionDelayMs - 1);
  EXPECT_FALSE(Cache()->IsHealthy());  // Reconnection is not allowed.
  CheckNotFound(kSomeKey);

  // Should reconnect after timeout passes.
  timer_.AdvanceMs(1);
  EXPECT_TRUE(Cache()->IsHealthy());  // Reconnection is allowed.
  CheckGet(kSomeKey, kSomeValue);
}

TEST_F(RedisCacheTest, DoesNotReconnectAfterShutdown) {
  if (!InitRedisOrSkip()) {
    return;
  }

  CheckPut(kSomeKey, kSomeValue);
  CheckGet(kSomeKey, kSomeValue);
  EXPECT_TRUE(Cache()->IsHealthy());

  Cache()->ShutDown();
  timer_.AdvanceMs(kReconnectionDelayMs);

  EXPECT_FALSE(Cache()->IsHealthy());  // Reconnection is not allowed.
  CheckNotFound(kSomeKey);
}

}  // namespace net_instaweb
