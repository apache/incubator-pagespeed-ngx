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
#include "pagespeed/kernel/base/posix_timer.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/cache/cache_test_base.h"
#include "pagespeed/kernel/thread/worker_test_base.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/util/simple_stats.h"
#include "pagespeed/system/tcp_connection_for_testing.h"
#include "pagespeed/system/tcp_server_thread_for_testing.h"

namespace net_instaweb {

namespace {
  static const int kReconnectionDelayMs = 10;
  static const int kTimeoutUs = 100 * Timer::kMsUs;
  static const char kSomeKey[] = "SomeKey";
  static const char kSomeValue[] = "SomeValue";
}

using testing::HasSubstr;

// TODO(yeputons): refactor this class with AprMemCacheTest, see details in
// apr_mem_cache_test.cc
class RedisCacheTest : public CacheTestBase {
 protected:
  RedisCacheTest()
      : thread_system_(Platform::CreateThreadSystem()),
        statistics_(thread_system_.get()),
        timer_(new NullMutex, 0) {
    RedisCache::InitStats(&statistics_);
  }

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

    {
      TcpConnectionForTesting conn;
      CHECK(conn.Connect("localhost", port))
          << "Cannot connect to Redis server";
      conn.Send("FLUSHALL\r\n");
      CHECK_EQ("+OK\r\n", conn.ReadLineCrLf());
    }

    cache_.reset(new RedisCache("localhost", port, thread_system_.get(),
                                &handler_, &timer_, kReconnectionDelayMs,
                                kTimeoutUs, &statistics_));
    cache_->StartUp();
    return true;
  }

  void InitRedisWithCustomServer() {
    cache_.reset(new RedisCache("localhost", custom_server_port_,
                                thread_system_.get(), &handler_, &timer_,
                                kReconnectionDelayMs, kTimeoutUs,
                                &statistics_));
  }

  void InitRedisWithUnreachableServer() {
    // Try to connect to some definitely unreachable host.
    // 192.0.2.0/24 is reserved for documentation purposes in RFC5737 and no
    // machine should ever be routable in that subnet.
    cache_.reset(new RedisCache("192.0.2.1", 12345, thread_system_.get(),
                                &handler_, &timer_, kReconnectionDelayMs,
                                kTimeoutUs, &statistics_));
  }

  static void SetUpTestCase() {
    apr_initialize();
    TcpServerThreadForTesting::PickListenPortOnce(&custom_server_port_);
    CHECK_NE(custom_server_port_, 0);
  }

  template<class ServerThread>
  bool StartCustomServer() {
    WaitForCustomServerShutdown();
    custom_server_.reset(
        new ServerThread(custom_server_port_, thread_system_.get()));
    if (!custom_server_->Start()) {
      return false;
    }
    // Wait while server starts and check its port
    return custom_server_->GetListeningPort() == custom_server_port_;
  }

  void WaitForCustomServerShutdown() {
    custom_server_.reset();
  }

  static void TearDownTestCase() {
    apr_terminate();
  }

  CacheInterface* Cache() override { return cache_.get(); }

  ThreadSynchronizer* GetThreadSynchronizer() {
    return cache_->GetThreadSynchronizerForTesting();
  }

  scoped_ptr<RedisCache> cache_;
  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats statistics_;
  MockTimer timer_;
  GoogleMessageHandler handler_;

  scoped_ptr<TcpServerThreadForTesting> custom_server_;
  static apr_port_t custom_server_port_;
};

apr_port_t RedisCacheTest::custom_server_port_ = 0;

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

  // We're not running against redis cluster, so we don't expect to ever be
  // redirected, and we should never ask for cluster slots.
  EXPECT_EQ(0, cache_->Redirections());
  EXPECT_EQ(0, cache_->ClusterSlotsFetches());
}

// Make sure curly braces in keys aren't treated specially.
TEST_F(RedisCacheTest, CurlyBraces) {
  if (!InitRedisOrSkip()) {
    return;
  }
  CheckPut("{1}NameA", "Value1A");
  CheckPut("{2}NameB", "Value2B");
  CheckPut("{2}NameC", "Value2C");

  CheckGet("{1}NameA", "Value1A");
  CheckGet("{2}NameB", "Value2B");
  CheckGet("{2}NameC", "Value2C");
}

// And spaces
TEST_F(RedisCacheTest, Spaces) {
  if (!InitRedisOrSkip()) {
    return;
  }
  CheckPut("1 NameA", "Value1A");
  CheckPut("2 NameB", "Value2B");
  CheckPut("2 NameC", "Value2C");

  CheckGet("1 NameA", "Value1A");
  CheckGet("2 NameB", "Value2B");
  CheckGet("2 NameC", "Value2C");
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
  cache_->GetStatus(&status);

  // Check that some reasonable info is present.
  EXPECT_THAT(status, HasSubstr(cache_->ServerDescription()));
  EXPECT_THAT(status, HasSubstr("redis_version:"));
  EXPECT_THAT(status, HasSubstr("connected_clients:"));
  EXPECT_THAT(status, HasSubstr("tcp_port:"));
  EXPECT_THAT(status, HasSubstr("used_memory:"));
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

TEST_F(RedisCacheTest, ReconnectsInstantly) {
  InitRedisWithCustomServer();
  ASSERT_TRUE(StartCustomServer<RedisGetRespondingServerThread>());
  cache_->StartUp();

  CheckGet(kSomeKey, kSomeValue);
  // Server closes connection after processing one request, but cache does not
  // know about that yet.
  WaitForCustomServerShutdown();
  EXPECT_TRUE(Cache()->IsHealthy());

  // Client should not reconnect as it learns about disconnection only when it
  // tries to run the command.
  ASSERT_TRUE(StartCustomServer<RedisGetRespondingServerThread>());
  CheckNotFound(kSomeKey);

  // First reconnection attempt should happen right away.
  EXPECT_TRUE(Cache()->IsHealthy());  // Allow reconnection.
  CheckGet(kSomeKey, kSomeValue);
}

TEST_F(RedisCacheTest, ReconnectsUntilSuccessWithTimeout) {
  InitRedisWithCustomServer();
  ASSERT_TRUE(StartCustomServer<RedisGetRespondingServerThread>());
  cache_->StartUp();

  CheckGet(kSomeKey, kSomeValue);
  // Server closes connection after processing one request, but cache does not
  // know about that yet.
  WaitForCustomServerShutdown();
  EXPECT_TRUE(Cache()->IsHealthy());

  // Let client know that we're disconnected by trying to read.
  CheckNotFound(kSomeKey);

  // Try to reconnect right away after failure.
  EXPECT_TRUE(Cache()->IsHealthy());  // Reconnection is allowed...
  CheckNotFound(kSomeKey);  // ...but it fails.

  // Second attempt, should not reconnect before timeout.
  ASSERT_TRUE(StartCustomServer<RedisGetRespondingServerThread>());
  timer_.AdvanceMs(kReconnectionDelayMs - 1);
  EXPECT_FALSE(Cache()->IsHealthy());  // Reconnection is not allowed.
  CheckNotFound(kSomeKey);

  // Should reconnect after timeout passes.
  timer_.AdvanceMs(1);
  EXPECT_TRUE(Cache()->IsHealthy());  // Reconnection is allowed.
  CheckGet(kSomeKey, kSomeValue);
}

TEST_F(RedisCacheTest, ReconnectsIfStartUpFailed) {
  InitRedisWithCustomServer();
  cache_->StartUp();

  // Client already knows that connection failed.
  EXPECT_FALSE(Cache()->IsHealthy());
  CheckNotFound(kSomeKey);

  // Should not reconnect before timeout.
  ASSERT_TRUE(StartCustomServer<RedisGetRespondingServerThread>());
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

// This server always waits until connection is received to avoid race
// condition between server destruction and accepting connection (like in
// ShutDownDuringConnection). Other servers do not do that because tests
// actually rely on their answers to client.
class RedisNotRespondingServerThread : public TcpServerThreadForTesting {
 public:
  RedisNotRespondingServerThread(apr_port_t listen_port,
                                 ThreadSystem* thread_system)
      : TcpServerThreadForTesting(listen_port, "redis_not_responding_server",
                                  thread_system),
        connection_received_(thread_system) {}

  ~RedisNotRespondingServerThread() {
    connection_received_.Wait();
    ShutDown();
  }

 protected:
  void HandleClientConnection(apr_socket_t* sock) override {
    // Do nothing, socket will be closed in destructor
    connection_received_.Notify();
  }

 private:
  WorkerTestBase::SyncPoint connection_received_;
};

// These constants are for timeout tests.
namespace {
// Experiments showed that I/O functions on Linux may sometimes time out
// slightly earlier than configured. It does not look like precision or
// rounding error; waking up recv() 2ms earlier has probability around 0.7%.
// That is partially leveraged by the fact that we have a bunch of code around
// I/O in RedisCache, but probability is still non-zero (0.05%). Probability
// of 1ms gap in RedisCacheOperationTimeouTest was around 5% at the time it was
// written. So we put here 5ms to be safe.
const int kTimedOutOperationMinTimeUs = kTimeoutUs - 5 * Timer::kMsUs;

// Upper gap is bigger because taking more time than time out is expected.
// Unfortunately, it still gives 0.05%-0.1% of spurious failures and 'real'
// overhead in these outliers can be bigger than 100ms.
const int kTimedOutOperationMaxTimeUs = kTimeoutUs + 50 * Timer::kMsUs;

// We want timeout to be significantly greater than measuring gap.
static_assert(kTimeoutUs >= 60 * Timer::kMsUs,
              "kTimeoutUs is smaller than measuring gap");
}  // namespace

TEST_F(RedisCacheTest, ConnectionTimeout) {
  InitRedisWithUnreachableServer();
  PosixTimer timer;
  int64 started_at_us = timer.NowUs();
  cache_->StartUp();  // Should try to connect as well.
  int64 waited_for_us = timer.NowUs() - started_at_us;
  EXPECT_FALSE(cache_->IsHealthy());
  EXPECT_GE(waited_for_us, kTimedOutOperationMinTimeUs);
  EXPECT_LE(waited_for_us, kTimedOutOperationMaxTimeUs);
}

class GetRequestThread : public ThreadSystem::Thread {
 public:
  GetRequestThread(CacheInterface* cache, ThreadSystem* system)
      : ThreadSystem::Thread(system, "get_request_thread",
                             ThreadSystem::kJoinable),
        cache_(cache),
        check_called_(false) {}

  void Run() override {
    cache_->Get(kSomeKey, &callback_);
  }

  void CheckLookupResult(CacheInterface::KeyState expected_state) {
    check_called_ = true;
    Join();
    EXPECT_TRUE(callback_.called());
    EXPECT_EQ(callback_.state(), expected_state);
    if (expected_state == CacheInterface::kAvailable) {
      EXPECT_STREQ(callback_.value().Value(), kSomeValue);
    }
  }

  ~GetRequestThread() override {
    EXPECT_TRUE(check_called_)
        << "CheckLookupResult() was not called on GetRequestThread";
  }

 private:
  CacheInterface* cache_;
  CacheInterface::SynchronousCallback callback_;
  bool check_called_;
};

TEST_F(RedisCacheTest, IsHealthyDoesNotBlock) {
  InitRedisWithCustomServer();
  StartCustomServer<RedisGetRespondingServerThread>();
  GetThreadSynchronizer()->EnableForPrefix("RedisCommand.After");
  cache_->StartUp();

  GetRequestThread thread(Cache(), thread_system_.get());
  ASSERT_TRUE(thread.Start());
  GetThreadSynchronizer()->Wait("RedisCommand.After.Signal");

  // Check that IsHealthy() returns even when operation is in progress.
  Cache()->IsHealthy();

  GetThreadSynchronizer()->Signal("RedisCommand.After.Wait");
  thread.CheckLookupResult(CacheInterface::kAvailable);
}

TEST_F(RedisCacheTest, ConnectionFastFail) {
  InitRedisWithCustomServer();
  StartCustomServer<RedisGetRespondingServerThread>();
  GetThreadSynchronizer()->EnableForPrefix("RedisConnect.After");
  cache_->StartUp(/* connect_now */ false);

  EXPECT_TRUE(Cache()->IsHealthy());
  GetRequestThread thread(Cache(), thread_system_.get());
  ASSERT_TRUE(thread.Start());
  GetThreadSynchronizer()->Wait("RedisConnect.After.Signal");

  EXPECT_FALSE(Cache()->IsHealthy());  // Connection is in progress.

  // Check that Get() returns immediately. Two times becase first call may
  // theoretically override cache's state (real bug).
  for (int iter = 1; iter <= 2; iter++) {
    CheckNotFound(kSomeKey);
  }

  GetThreadSynchronizer()->Signal("RedisConnect.After.Wait");
  thread.CheckLookupResult(CacheInterface::kAvailable);

  // Now that thread is terminated, connection is established, cache should be
  // healthy again.
  EXPECT_TRUE(Cache()->IsHealthy());
}

TEST_F(RedisCacheTest, ShutDownDuringConnection) {
  InitRedisWithCustomServer();
  StartCustomServer<RedisNotRespondingServerThread>();
  GetThreadSynchronizer()->EnableForPrefix("RedisConnect.After");
  cache_->StartUp(/* connect_now */ false);

  EXPECT_TRUE(Cache()->IsHealthy());
  GetRequestThread thread(Cache(), thread_system_.get());
  ASSERT_TRUE(thread.Start());
  GetThreadSynchronizer()->Wait("RedisConnect.After.Signal");

  Cache()->ShutDown();

  GetThreadSynchronizer()->Signal("RedisConnect.After.Wait");
  thread.CheckLookupResult(CacheInterface::kNotFound);

  // Cache potentially may want to reconnect, so we advance timer to ensure that
  // it does not want to.
  timer_.AdvanceMs(kReconnectionDelayMs);
  EXPECT_FALSE(Cache()->IsHealthy());
}

// All RedisCacheOperationTimeoutTests start with a cache connected to a server
// which accepts single connection and does not answer until test is finished.
// The test calls a single command. If the timeout handling is correct, it times
// out and the test terminates correctly. If the timeout handling is not
// correct, the test hangs.
class RedisCacheOperationTimeoutTest : public RedisCacheTest {
 protected:
  void SetUp() {
    InitRedisWithCustomServer();
    CHECK(StartCustomServer<RedisNotRespondingServerThread>());
    cache_->StartUp();
    started_at_us_ = timer_.NowUs();
  }

  void TearDown() {
    int64 waited_for_us = timer_.NowUs() - started_at_us_;
    EXPECT_GE(waited_for_us, kTimedOutOperationMinTimeUs);
    EXPECT_LE(waited_for_us, kTimedOutOperationMaxTimeUs);
  }

 private:
  PosixTimer timer_;
  int64 started_at_us_;
};

TEST_F(RedisCacheOperationTimeoutTest, Get) {
  CheckNotFound("Key");
}

// TODO(yeputons): test MultiGet when it's a single command, not sequenced GET()

TEST_F(RedisCacheOperationTimeoutTest, Put) {
  CheckPut("Key", "Value");
}

TEST_F(RedisCacheOperationTimeoutTest, Delete) {
  CheckDelete("Key");
}

}  // namespace net_instaweb
