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

#ifndef PAGESPEED_SYSTEM_REDIS_CACHE_H_
#define PAGESPEED_SYSTEM_REDIS_CACHE_H_

#include <stdarg.h>

#include <memory>
#include <initializer_list>
#include <map>
#include <vector>

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/thread/thread_synchronizer.h"
#include "pagespeed/system/external_server_spec.h"
#include "third_party/hiredis/src/hiredis.h"

namespace net_instaweb {

// Interface to Redis using hiredis library. This implementation is blocking and
// thread-safe. One should call StartUp() before making any requests and
// ShutDown() after finishing work. Connecting will be initiated on StartUp()
// call. If it fails there or is dropped after any request, the following
// reconnection strategy is used:
// 1. If an operation fails because of communication or protocol error, try
//    reconnecting on the next Get/Put/Delete (without delay).
// 2. If (re-)connection attempt is unsuccessfull, try again on next
//    Get/Put/Delete operation, but not until at least reconnection_delay_ms_
//    have passed from the previous attempt.
// That ensures that we do not try to connect to unreachable server a lot, but
// still allows us to reconnect quickly in case of network glitches.
//
// See redis_cache.cc for details on different locks that the class has.
//
// When using clustering, to be efficient we need to know what redis cluster
// machine to talk to for a given key.  There are two steps:
// 1) hash the key in a redis-specific way determine the hash slot.
// 2) consult our cached copy of the slot-to-server mapping to see which
//    server to send to.
// If we get a redirection after following that approach, our cached mapping
// is invalid and should be refreshed.
//
// http://redis.io/topics/cluster-spec explains this all.
//
// TODO(yeputons): consider extracting a common interface with AprMemCache.
// TODO(yeputons): consider making Redis-reported errors treated as failures.
// TODO(yeputons): add redis AUTH command support.
class RedisCache : public CacheInterface {
 public:
  // Uses ThreadSystem to generate several mutexes in constructor only.
  // Does not take ownership of MessageHandler, Timer, and assumes
  // that these pointers are valid throughout full lifetime of RedisCache.
  RedisCache(StringPiece host, int port, ThreadSystem* thread_system,
             MessageHandler* message_handler, Timer* timer,
             int64 reconnection_delay_ms, int64 timeout_us,
             Statistics* stats);
  ~RedisCache() override { ShutDown(); }

  static void InitStats(Statistics* stats);

  GoogleString ServerDescription() const;

  void StartUp(bool connect_now = true);

  // CacheInterface implementations.
  GoogleString Name() const override { return FormatName(); }
  bool IsBlocking() const override { return true; }
  bool IsHealthy() const override;
  void ShutDown() override;

  static GoogleString FormatName() { return "RedisCache"; }

  // CacheInterface implementations.
  void Get(const GoogleString& key, Callback* callback) override;
  void Put(const GoogleString& key, const SharedString& value) override;
  void Delete(const GoogleString& key) override;

  // Appends detailed status for each server to a string.  If a server fails to
  // report a status, then for that server we append an error message instead.
  void GetStatus(GoogleString* status_string);

  // Redis spec defined hasher for keys.  Static, since it's a pure function.
  static int HashSlot(StringPiece key);

  // Total number of times we hit one server and were redirected to a different
  // one.
  int64 Redirections() {
    return redirections_->Get();
  }

  // Total number of times we looked up the cluster slots mappings to avoid
  // redirections.
  int64 ClusterSlotsFetches() {
    return cluster_slots_fetches_->Get();
  }

 private:
  struct RedisReplyDeleter {
    void operator()(redisReply* ptr) {
      if (ptr != nullptr) {  // hiredis 0.11 does not check.
        freeReplyObject(ptr);
      }
    }
  };
  typedef std::unique_ptr<redisReply, RedisReplyDeleter> RedisReply;

  struct RedisContextDeleter {
    void operator()(redisContext* ptr) {
      if (ptr != nullptr) {  // hiredis 0.11 does not check.
        redisFree(ptr);
      }
    }
  };
  typedef std::unique_ptr<redisContext, RedisContextDeleter> RedisContext;

  class Connection {
   public:
    Connection(RedisCache* redis_cache, StringPiece host, int port);

    void StartUp(bool connect_now = true)
        LOCKS_EXCLUDED(redis_mutex_, state_mutex_);
    bool IsHealthy() const LOCKS_EXCLUDED(redis_mutex_, state_mutex_);
    void ShutDown() LOCKS_EXCLUDED(redis_mutex_, state_mutex_);

    GoogleString ToString() {
      return StrCat(host_, ":", IntegerToString(port_));
    }

    AbstractMutex* GetOperationMutex() const LOCK_RETURNED(redis_mutex_) {
      return redis_mutex_.get();
    }

    // Must be followed by ValidateRedisReply() under the same lock.
    RedisReply RedisCommand(const char* format, va_list args)
        EXCLUSIVE_LOCKS_REQUIRED(redis_mutex_) LOCKS_EXCLUDED(state_mutex_);
    RedisReply RedisCommand(const char* format, ...)
        EXCLUSIVE_LOCKS_REQUIRED(redis_mutex_) LOCKS_EXCLUDED(state_mutex_);

    bool ValidateRedisReply(const RedisReply& reply,
                            std::initializer_list<int> valid_types,
                            const char* command_executed)
        EXCLUSIVE_LOCKS_REQUIRED(redis_mutex_) LOCKS_EXCLUDED(state_mutex_);

   private:
    enum State {
      kShutDown,
      kDisconnected,
      kConnecting,
      kConnected
    };

    bool IsHealthyLockHeld() const EXCLUSIVE_LOCKS_REQUIRED(state_mutex_);
    void UpdateState() EXCLUSIVE_LOCKS_REQUIRED(redis_mutex_, state_mutex_);

    bool EnsureConnection() EXCLUSIVE_LOCKS_REQUIRED(redis_mutex_)
        LOCKS_EXCLUDED(state_mutex_);

    RedisContext TryConnect() LOCKS_EXCLUDED(redis_mutex_, state_mutex_);

    void LogRedisContextError(redisContext* redis, const char* cause);

    const RedisCache* redis_cache_;
    const GoogleString host_;
    const int port_;
    const scoped_ptr<AbstractMutex> redis_mutex_;
    const scoped_ptr<AbstractMutex> state_mutex_;

    RedisContext redis_ GUARDED_BY(redis_mutex_);
    State state_ GUARDED_BY(state_mutex_);
    int64 next_reconnect_at_ms_ GUARDED_BY(state_mutex_);

    DISALLOW_COPY_AND_ASSIGN(Connection);
  };
  typedef std::map<GoogleString, std::unique_ptr<Connection>> ConnectionsMap;

  struct ClusterMapping {
    // We only ever add connections, so it's ok for us to save raw pointers.
    ClusterMapping(int start_slot_range,
                   int end_slot_range,
                   Connection* connection) :
        start_slot_range_(start_slot_range),
        end_slot_range_(end_slot_range),
        connection_(connection) {}
    int start_slot_range_;
    int end_slot_range_;
    Connection* connection_;
  };

  // Performs specified command, handling all reconnections, redirections
  // from Redis Cluster, and other stuff. Then it checks that Redis' reply
  // has expected type and returns the reply object. If anything fails,
  // returns nullptr.
  //
  // TODO(yeputons): it's expected that destruction of RedisReply is
  // thread-safe, which is true as of hiredis 0.13. Otherwise it's wrong to
  // return RedisReply here because lock is released on exit.
  // See https://github.com/redis/hiredis/issues/465
  RedisReply RedisCommand(Connection* connection, const char* format,
                          std::initializer_list<int> valid_reply_types, ...);

  ThreadSynchronizer* GetThreadSynchronizerForTesting() const {
    return thread_synchronizer_.get();
  }

  ExternalServerSpec ParseRedirectionError(StringPiece error);

  // Must not be called under Connection::GetOperationLock(), that will cause
  // lock inversion and potential theoretical deadlock.
  Connection* GetOrCreateConnection(ExternalServerSpec spec);

  // Ask redis what keys should go to which servers.
  void FetchClusterSlotMapping(Connection* connection)
      LOCKS_EXCLUDED(cluster_map_lock_);

  // If we have a cached copy of the connection slot mapping from
  // LookupClusterSlots then return the connection this key goes with.  If we
  // don't have it, or if this slot isn't in the mapping, return
  // main_connection_.
  Connection* LookupConnection(StringPiece key)
      LOCKS_EXCLUDED(cluster_map_lock_);

  const GoogleString main_host_;
  const int main_port_;
  ThreadSystem* thread_system_;
  MessageHandler* message_handler_;
  Timer* timer_;
  const int64 reconnection_delay_ms_;
  const int64 timeout_us_;
  const scoped_ptr<ThreadSynchronizer> thread_synchronizer_;
  const scoped_ptr<ThreadSystem::RWLock> connections_lock_;
  const scoped_ptr<ThreadSystem::RWLock> cluster_map_lock_;
  Variable* redirections_;
  Variable* cluster_slots_fetches_;

  // It's expected that connections are only added to the map. That way we can
  // safely use raw pointers to them during RedisCache lifetime.
  ConnectionsMap connections_ GUARDED_BY(connections_lock_);
  std::vector<ClusterMapping> cluster_mappings_ GUARDED_BY(cluster_map_lock_);

  // Not guarded, but should only be modified in StartUp().
  Connection* main_connection_;

  friend class RedisCacheTest;
  DISALLOW_COPY_AND_ASSIGN(RedisCache);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_REDIS_CACHE_H_
