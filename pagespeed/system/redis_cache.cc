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

#include "pagespeed/system/redis_cache.h"

#include <sys/time.h>
#include <algorithm>
#include <cstddef>
#include <utility>

#include "base/logging.h"
#include "third_party/redis-crc/redis_crc.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

// Information below is mostly about RedisCache::Connection. RedisCache is a
// trivial wrapper around it when there is single Redis node. When Redis Cluster
// is enabled, RedisCache handles redirection errors and connection juggling.
//
// Hiredis is a non-thread-safe C library which we wrap around. We could use a
// single mutex for all operations, but we want to have two properties:
// 1. IsHealthy() never locks for a long time.
//    Why: it's specification of CacheInterface and it's called in a part of
//    rewriting where we need to know quickly whether to bother doing work.
// 2. If there is a connection in progress, at most one thread is locked. All
//    other threads fail current operation and unlock. All new threads return
//    failure immediately.
//    Why: connection happens after previous connection drops, which is caused
//    by network glitches or server failure. Either way, it's reasonable to
//    expect that reconnection may take a very long time, therefore we do not
//    want many threads to wait for it.
//
// Some invariants:
// 1. state_mutex_ can be locked for constant time only (by a single thread).
// 2. Current state of cache is kept in bunch of variables protected by
//    state_mutex_, therefore IsHealthy() can return fast.
// 3. All Redis-related errors are detected and reflected in state_ by
//    UpdateState(), which should be called after operations on redis_.
//    Note that if you have not actually performed operation on redis_, you
//    should not call it.
// 4. Mutexes should be locked in that order: redis_mutex_, state_mutex_.
// 5. Thread that wants to change state variables should have BOTH redis_mutex_
//    and state_mutex_. It should happen automatically: if thread does not hold
//    redis_mutex_, it cannot do anything with cache and cannot change state.
// 6. redis_mutex_ can be locked for non-constant time only if it's for
//    operation on healthy redis_. Ideologically, redis_mutex_ is used for
//    'queueing' Redis commands.
// 7. If reconnection is required, redis_mutex_ is unlocked so other threads can
//    unlock, see state_ == kConnecting and fail their operations. The mutex is
//    locked back after re-connection is completed.

// When Redis Cluster is stable, there should not be more than one redirection,
// as each node knows full cluster layout. If we go to the correct node right
// away, there should be zero redirections. When cluster is being reconfigured,
// there can potentially be some race conditions between our requests and layout
// changes, which can lead to multiple redirections.  Right now we ignore these
// situations and drop the operation requested if there is more than one
// reconnection.
//
// TODO(yeputons): consider removing limit on amount of redirections.
static const int kMaxRedirections = 1;

const char kRedisClusterRedirections[] = "redis_cluster_redirections";
const char kRedisClusterSlotsFetches[] = "redis_cluster_slots_fetches";

RedisCache::RedisCache(StringPiece host, int port, ThreadSystem* thread_system,
                       MessageHandler* message_handler, Timer* timer,
                       int64 reconnection_delay_ms, int64 timeout_us,
                       Statistics* stats)
    : main_host_(host.as_string()),
      main_port_(port),
      thread_system_(thread_system),
      message_handler_(message_handler),
      timer_(timer),
      reconnection_delay_ms_(reconnection_delay_ms),
      timeout_us_(timeout_us),
      thread_synchronizer_(new ThreadSynchronizer(thread_system)),
      connections_lock_(thread_system_->NewRWLock()),
      cluster_map_lock_(thread_system_->NewRWLock()),
      main_connection_(nullptr) {
  redirections_ = stats->GetVariable(kRedisClusterRedirections);
  cluster_slots_fetches_ = stats->GetVariable(kRedisClusterSlotsFetches);
}

GoogleString RedisCache::ServerDescription() const {
  return StrCat(main_host_, ":", IntegerToString(main_port_));
}

// static
void RedisCache::InitStats(Statistics* stats) {
  stats->AddVariable(kRedisClusterRedirections);
  stats->AddVariable(kRedisClusterSlotsFetches);
}

void RedisCache::StartUp(bool connect_now) {
  CHECK_NE("", main_host_);
  CHECK_NE(0, main_port_);
  {
    ScopedMutex lock(connections_lock_.get());
    CHECK(connections_.empty());
    CHECK(!main_connection_);
    std::unique_ptr<Connection> conn(
        new Connection(this, main_host_, main_port_));
    main_connection_ = conn.get();
    connections_.emplace(StrCat(main_host_, ":", IntegerToString(main_port_)),
                         std::move(conn));
  }
  main_connection_->StartUp(connect_now);
}

bool RedisCache::IsHealthy() const {
  // TODO(yeputons): note that IsHealthy() == true for Connection class can
  // mean that either connection is established or that it's reasonable to try
  // to reconnect. Things probably get more complicated when we have several
  // Connections. We should think about what IsHealthy() should mean in case of
  // Redis Cluster and probably split Connection::IsHealthy() into something
  // more detailed.
  ThreadSystem::ScopedReader lock(connections_lock_.get());
  for (auto& conn : connections_) {
    // IsHealthy() should be fast enough so it's ok to hold reader lock.
    if (!conn.second->IsHealthy()) {
      return false;
    }
  }
  return true;
}

void RedisCache::ShutDown() {
  ThreadSystem::ScopedReader lock(connections_lock_.get());
  for (auto& conn : connections_) {
    // As there should be no operations after ShutDown(), it's safe to perform
    // costly operation under connections_mutex_.
    conn.second->ShutDown();
  }
}

void RedisCache::Get(const GoogleString& key, Callback* callback) {
  KeyState keyState = CacheInterface::kNotFound;
  RedisReply reply = RedisCommand(
      LookupConnection(key),
      "GET %b", {REDIS_REPLY_STRING, REDIS_REPLY_NIL},
      key.data(), key.length());

  if (reply) {
    if (reply->type == REDIS_REPLY_STRING) {
      // The only type of values that we store in Redis is string.
      callback->set_value(SharedString(StringPiece(reply->str, reply->len)));
      keyState = CacheInterface::kAvailable;
    } else {
      // REDIS_REPLY_NIL means 'key not found', do nothing.
    }
  }
  ValidateAndReportResult(key, keyState, callback);
}

void RedisCache::Put(const GoogleString& key, const SharedString& value) {
  RedisReply reply = RedisCommand(
      LookupConnection(key),
      "SET %b %b",
      {REDIS_REPLY_STATUS},
      key.data(), key.length(),
      value.data(), static_cast<size_t>(value.size()));

  if (!reply) {
    return;
  }

  GoogleString answer(reply->str, reply->len);
  if (answer == "OK") {
    // Success, nothing to do.
  } else {
    LOG(DFATAL) << "Unexpected status from redis as answer to SET: " << answer;
    message_handler_->Message(
        kError, "Unexpected status from redis as answer to SET: %s",
        answer.c_str());
  }
}

void RedisCache::Delete(const GoogleString& key) {
  // Redis returns amount of keys deleted (probably, zero), no need in check
  // that amount; all other errors are handled by RedisCommand.
  RedisCommand(LookupConnection(key),
               "DEL %b", {REDIS_REPLY_INTEGER}, key.data(), key.length());
}

void RedisCache::GetStatus(GoogleString* buffer) {
  StrAppend(buffer, "Statistics for Redis (", ServerDescription(), "):\n");

  // We don't want to hold the connections lock while querying all the servers
  // and connections_ is guaranteed only to grow, so take the lock briefly while
  // we make a copy.
  std::vector<Connection*> connections_copy;
  {
    ScopedMutex lock(connections_lock_.get());
    for (auto& entry : connections_) {
      Connection* connection = entry.second.get();
      connections_copy.push_back(connection);
    }
  }

  for (Connection* connection : connections_copy) {
    StrAppend(buffer, "\nConnection ", connection->ToString(), ":\n");

    RedisReply reply = RedisCommand(connection, "INFO", {REDIS_REPLY_STRING});
    if (reply != nullptr) {
      StrAppend(buffer, reply->str);
    } else {
      StrAppend(buffer, "Error calling INFO");
    }
  }
}

RedisCache::RedisReply RedisCache::RedisCommand(
    Connection* likely_connection, const char* format,
    std::initializer_list<int> valid_reply_types, ...) {

  if (likely_connection == nullptr) {
    return nullptr;
  }
  GoogleString command = format;
  command = command.substr(0, command.find_first_of(' '));

  va_list args;
  va_start(args, valid_reply_types);

  RedisReply reply;
  Connection* conn = likely_connection;
  bool with_asking = false;
  ExternalServerSpec redirected_to;
  Connection* last_redirecting_connection = nullptr;
  // This loop will break when no further redirections are needed.
  int redirections;
  for (redirections = 0; redirections <= kMaxRedirections;
       redirections++,
           last_redirecting_connection = conn,
           conn = GetOrCreateConnection(redirected_to),
           redirections_->Add(1)) {
    ScopedMutex lock(conn->GetOperationMutex());

    if (with_asking) {
      // Send the ASKING command before the main operation, if required by a
      // prior redirect.
      if (!conn->ValidateRedisReply(conn->RedisCommand("ASKING"),
                                    {REDIS_REPLY_STATUS}, "ASKING")) {
        break;  // Fail operation.
      }
    }

    // va_list is getting invalidated on RedisCommand() call so we have to make
    // a copy for each retry.
    va_list args_copy;
    va_copy(args_copy, args);
    reply = conn->RedisCommand(format, args_copy);
    va_end(args_copy);

    // Process redirection errors.
    redirected_to = ExternalServerSpec();
    if (reply && reply->type == REDIS_REPLY_ERROR) {
      StringPiece error(reply->str, reply->len);
      if (error.starts_with("MOVED ")) {
        redirected_to = ParseRedirectionError(error);
        with_asking = false;
      } else if (error.starts_with("ASK ")) {
        redirected_to = ParseRedirectionError(error);
        with_asking = true;
      }
    }

    // ValidateRedisReply should be called after RedisCommand() to update state.
    if (!redirected_to.empty()) {
      conn->ValidateRedisReply(reply, {REDIS_REPLY_ERROR}, command.c_str());
      reply.reset();
    } else {
      // We have sent command to a correct node and will stop the loop.
      if (!conn->ValidateRedisReply(reply, valid_reply_types,
                                    command.c_str())) {
        reply.reset();
      }
      break;
    }
  }

  va_end(args);

  if (redirections > 0 && !with_asking) {
    // We were redirected with a MOVED, so query the server that told us to look
    // somewhere else what we should update our mappings to.
    message_handler_->Message(
        kInfo, "Redirected %d time(s), updating our slot mappings (in %s)",
        redirections, format);
    FetchClusterSlotMapping(last_redirecting_connection);
  }

  return reply;
}

// Redis may return errors like this: `MOVED 12182 127.0.0.1:7215`,
// `ASK 12182 127.0.0.1:1419`. First token is type of redirection (permanent or
// there is a migration in process), second token is calculated key slot, third
// token specified the node we should re-ask (host:port). See
// http://redis.io/topics/cluster-spec#moved-redirection.
ExternalServerSpec RedisCache::ParseRedirectionError(StringPiece error) {
  StringPieceVector error_tokens;
  SplitStringPieceToVector(error, " ", &error_tokens,
                           true /* omit_empty_strings */);
  if (error_tokens.size() != 3) {
    LOG(ERROR) << "Invalid redirection error: '" << error << "'";
    return ExternalServerSpec();
  }
  GoogleString host_port = error_tokens[2].as_string();

  StringPieceVector host_port_vec;
  SplitStringPieceToVector(host_port, ":", &host_port_vec,
                           false /* omit_empty_strings */);
  if (host_port_vec.size() != 2) {
    LOG(ERROR) << "Invalid address in redirection error: '" << error << "'";
    return ExternalServerSpec();
  }

  ExternalServerSpec result;
  result.host = host_port_vec[0].as_string();
  if (!StringToInt(host_port_vec[1], &result.port)) {
    LOG(ERROR) << "Invalid port in redirection error: '" << error << "'";
    return ExternalServerSpec();
  }

  return result;
}

RedisCache::Connection* RedisCache::GetOrCreateConnection(
    ExternalServerSpec spec) {
  Connection* result;
  bool should_start_up = false;
  {
    ScopedMutex lock(connections_lock_.get());
    GoogleString name = spec.ToString();
    ConnectionsMap::iterator it = connections_.find(name);
    if (it == connections_.end()) {
      LOG(INFO) << "Initiating connection Redis server at " << spec.ToString();
      it = connections_.emplace(name, std::unique_ptr<Connection>(
          new Connection(this, spec.host, spec.port))).first;
      should_start_up = true;
    }
    result = it->second.get();
  }
  if (should_start_up) {
    result->StartUp();
  }
  return result;
}

// static
int RedisCache::HashSlot(StringPiece key) {
  // If the key has a non-empty {}-section, truncate to just that section.  Our
  // code currently doesn't use {}-sections, except possibly by mistake, but
  // they're part of the protocol and we may want them someday for keeping
  // related keys together.
  stringpiece_ssize_type open_curly_index = key.find_first_of('{');
  if (open_curly_index != key.npos) {
    stringpiece_ssize_type close_curly_index = key.find_first_of(
        '}', open_curly_index);
    if (close_curly_index != key.npos) {
      stringpiece_ssize_type segment_length =
          close_curly_index - open_curly_index - 1;
      if (segment_length > 0) {
        key = key.substr(open_curly_index + 1,
                         close_curly_index - open_curly_index - 1);
      }
    }
  }

  // Return the last 14 bits of crc16(key).  To make sure we're using the same
  // crc16 as redis, we include it from redis 3.2.4 as redis_crc.h
  return redis_crc::crc16(key.data(), key.length()) & 0x3FFF;
}

void RedisCache::FetchClusterSlotMapping(Connection* connection) {
  // TODO(jefftk): If the mapping on the cluster changes this currently could
  // request the mapping up to once per cluster server, instead of once total.
  // To fix this, we could maintain a timestamp of when we last fetched the
  // mapping, and then only do the lookup here if that timestamp hasn't changed
  // since we used the mapping to decide (incorrectly, it turns out) which
  // server to talk to.  (If it changed, then someone else did a mapping update
  // in the mean time.)
  cluster_slots_fetches_->Add(1);
  RedisReply reply = RedisCommand(
      connection, "CLUSTER SLOTS", {REDIS_REPLY_ARRAY});
  if (reply == nullptr) {
    return;  // error
  }

  std::vector<ClusterMapping> new_cluster_mappings;
  for (size_t i = 0; i < reply->elements; i++) {
    // For each server we have an array in the form:
    // 1) [int] start slot range, inclusive
    // 2) [int] end slot range, inclusive
    // 3) [array] master spec
    //    a) [string] ip address
    //    b) [int] port
    //
    // Both of these arrays can have more entries, but (a) we don't need more
    // info and (b) what additional info there is depends on the redis server
    // version.
    redisReply* server_info = reply->element[i];
    if (server_info->elements < 3) {
      message_handler_->Message(kError, "Got short reply for CLUSTER SLOTS");
      return;
    }
    redisReply* start_slot_range = server_info->element[0];
    redisReply* end_slot_range = server_info->element[1];
    redisReply* master_spec = server_info->element[2];
    if (start_slot_range->type != REDIS_REPLY_INTEGER ||
        end_slot_range->type != REDIS_REPLY_INTEGER ||
        master_spec->type != REDIS_REPLY_ARRAY) {
      message_handler_->Message(
          kError, "Wrong type in reply from CLUSTER SLOTS");
      return;
    }
    if (master_spec->elements < 2) {
      message_handler_->Message(
          kError, "Short master spec in reply from CLUSTER SLOTS");
      return;
    }
    redisReply* master_ip = master_spec->element[0];
    redisReply* master_port = master_spec->element[1];
    if (master_ip->type != REDIS_REPLY_STRING ||
        master_port->type != REDIS_REPLY_INTEGER) {
      message_handler_->Message(
          kError, "Wrong type in master spec from CLUSTER SLOTS");
      return;
    }
    if (start_slot_range->integer > end_slot_range->integer) {
      message_handler_->Message(
          kError, "Got range with start > end from CLUSTER SLOTS");
      return;
    }
    // Everything is there and is the right type.  Store it.
    new_cluster_mappings.push_back(ClusterMapping(
        start_slot_range->integer, end_slot_range->integer,
        GetOrCreateConnection(ExternalServerSpec(
            master_ip->str, master_port->integer))));
  }

  // Sort new_cluster_mappings based on start_slot_range_.
  sort(new_cluster_mappings.begin(), new_cluster_mappings.end(),
       [](const RedisCache::ClusterMapping& a,
          const RedisCache::ClusterMapping& b) {
         return a.start_slot_range_ < b.start_slot_range_;
       });

  // Now they are sorted, verify that the slot ranges don't overlap.
  // Note: This starts at 1, not 0, because we are looking backwards.
  for (size_t i = 1; i < new_cluster_mappings.size(); ++i) {
    if (new_cluster_mappings[i].start_slot_range_ <=
        new_cluster_mappings[i - 1].end_slot_range_) {
      message_handler_->Message(
          kError, "Redis returned overlapping slot ranges for CLUSTER SLOTS");
      return;
    }
  }

  // We don't verify that it's a complete mapping, because Redis allows the
  // slot ranges to not cover the entire space. For slots that's aren't in the
  // mapping, we just fall back to the main connection

  ScopedMutex lock(cluster_map_lock_.get());
  cluster_mappings_.swap(new_cluster_mappings);
}

RedisCache::Connection* RedisCache::LookupConnection(StringPiece key) {
  int slot = HashSlot(key);
  ScopedMutex lock(cluster_map_lock_.get());
  // Find the first element not less than 'slot' in cluster_mappings_.
  // This depends on cluster_mappings_ being sorted.
  auto it =
      std::lower_bound(cluster_mappings_.begin(), cluster_mappings_.end(), slot,
                       [](const RedisCache::ClusterMapping& mapping, int slot) {
                         // lower_bound returns the first entry that is "not
                         // less than", so we must define our comparison such
                         // that it's true when slot is in the range (which is
                         // inclusive).
                         return mapping.end_slot_range_ < slot;
                       });

  if (it != cluster_mappings_.end()) {
    const ClusterMapping& cluster_mapping = *it;
    // The slot mapping is discontinuous, so the returned mapping still might
    // not match. slot ranges are inclusive.
    if (slot >= cluster_mapping.start_slot_range_ &&
        slot <= cluster_mapping.end_slot_range_) {
      return cluster_mapping.connection_;
    }
  }
  return main_connection_;
}

RedisCache::Connection::Connection(RedisCache* redis_cache, StringPiece host,
                                   int port)
    : redis_cache_(redis_cache),
      host_(host.as_string()),
      port_(port),
      redis_mutex_(redis_cache_->thread_system_->NewMutex()),
      state_mutex_(redis_cache_->thread_system_->NewMutex()),
      redis_(nullptr),
      state_(kShutDown),
      next_reconnect_at_ms_(redis_cache_->timer_->NowMs()) {}

void RedisCache::Connection::StartUp(bool connect_now) {
  CHECK_NE("", host_);
  CHECK_NE(0, port_);
  ScopedMutex lock1(redis_mutex_.get());
  {
    ScopedMutex lock2(state_mutex_.get());
    CHECK_EQ(state_, kShutDown);
    state_ = kDisconnected;
  }
  if (connect_now) {
    EnsureConnection();
  }
}

bool RedisCache::Connection::IsHealthy() const {
  ScopedMutex lock(state_mutex_.get());
  return IsHealthyLockHeld();
}

void RedisCache::Connection::ShutDown() {
  ScopedMutex lock1(redis_mutex_.get());
  ScopedMutex lock2(state_mutex_.get());
  if (state_ == kShutDown) {
    return;
  }
  // As we were able to grab redis_mutex_, there is no operation in progress,
  // maybe except for connection. EnsureConnection() handles the possibility
  // that a shutdown happens while it has released its lock and is waiting
  // for TryConnect().
  //
  // TODO(yeputons): be careful when adding async requests: ShutDown can be
  // called while there are some unfinished requests, they should return.
  redis_.reset();
  state_ = kShutDown;
}

bool RedisCache::Connection::EnsureConnection() {
  {
    ScopedMutex lock(state_mutex_.get());
    if (state_ == kConnected) {
      return true;
    }
    DCHECK(!redis_);
    // IsHealthyLockHeld() knows whether reconnection is sensible.
    if (!IsHealthyLockHeld()) {
      return false;
    }
    redis_.reset();
    state_ = kConnecting;
  }

  // redis_mutex_ is held on entry to EnsureConnection().
  redis_mutex_->Unlock();
  RedisContext loc_redis = TryConnect();
  redis_mutex_->Lock();

  ScopedMutex lock(state_mutex_.get());
  if (state_ == kConnecting) {
    CHECK(!redis_);
    redis_ = std::move(loc_redis);

    next_reconnect_at_ms_ = redis_cache_->timer_->NowMs();
    if (!redis_) {
      // It's better to wait some time before next attempt.
      next_reconnect_at_ms_ += redis_cache_->reconnection_delay_ms_;
    }
    UpdateState();
  } else {
    // Looks like cache was shut down. It also possible that it was started up
    // back (though it's prohibited by CacheInterface), but in that case we
    // will just fail this old operation, no big deal.
    DCHECK_EQ(state_, kShutDown);
  }
  return state_ == kConnected;
}

RedisCache::RedisContext RedisCache::Connection::TryConnect() {
  struct timeval timeout;
  timeout.tv_sec = redis_cache_->timeout_us_ / Timer::kSecondUs;
  timeout.tv_usec = redis_cache_->timeout_us_ % Timer::kSecondUs;

  RedisContext loc_redis(
      redisConnectWithTimeout(host_.c_str(), port_, timeout));
  redis_cache_->thread_synchronizer_->Signal("RedisConnect.After.Signal");
  redis_cache_->thread_synchronizer_->Wait("RedisConnect.After.Wait");

  if (loc_redis == nullptr) {
    redis_cache_->message_handler_->Message(kError,
                                            "Cannot allocate redis context");
  } else if (loc_redis->err) {
    LogRedisContextError(loc_redis.get(), "Error while connecting to redis");
  } else if (redisSetTimeout(loc_redis.get(), timeout) != REDIS_OK) {
    LogRedisContextError(loc_redis.get(),
                         "Error while setting timeout on redis context");
  } else {
    return loc_redis;
  }
  return nullptr;
}

bool RedisCache::Connection::IsHealthyLockHeld() const {
  switch (state_) {
    case kShutDown:
      return false;
    case kDisconnected:
      // Reconnection can happen during cache request onnly. We want some thread
      // to make a cache request, so we return `true` if cache is eligible for
      // reconnection.
      return next_reconnect_at_ms_ <= redis_cache_->timer_->NowMs();
    case kConnecting:
      return false;
    case kConnected:
      return true;
  }
  // gcc thinks following lines are reachable.
  LOG(FATAL) << "Invalid state_ in IsHealthyLockHeld()";
  return false;
}

void RedisCache::Connection::UpdateState() {
  // Quoting hireds documentation: "once an error is returned the context cannot
  // be reused and you should set up a new connection".
  if (redis_ != nullptr && redis_->err == 0) {
    state_ = kConnected;
  } else {
    state_ = kDisconnected;
    redis_.reset();
  }
}

RedisCache::RedisReply RedisCache::Connection::RedisCommand(const char* format,
                                                            va_list args) {
  if (!EnsureConnection()) {
    return nullptr;
  }

  void* result = redisvCommand(redis_.get(), format, args);
  redis_cache_->thread_synchronizer_->Signal("RedisCommand.After.Signal");
  redis_cache_->thread_synchronizer_->Wait("RedisCommand.After.Wait");

  return RedisReply(static_cast<redisReply*>(result));
}

RedisCache::RedisReply RedisCache::Connection::RedisCommand(
    const char* format, ...) {
  va_list args;
  va_start(args, format);
  RedisCache::RedisReply reply = RedisCommand(format, args);
  va_end(args);
  return reply;
}

void RedisCache::Connection::LogRedisContextError(redisContext* context,
                                      const char* cause) {
  if (context == nullptr) {
    // Can happen if EnsureConnection() failed to allocate context
    redis_cache_->message_handler_->Message(
        kError, "%s: unknown error (redis context is not available)", cause);
  } else {
    redis_cache_->message_handler_->Message(kError, "%s: err flags is %d, %s",
                              cause, context->err, context->errstr);
  }
}

bool RedisCache::Connection::ValidateRedisReply(const RedisReply& reply,
                                    std::initializer_list<int> valid_types,
                                    const char* command_executed) {
  {
    ScopedMutex lock(state_mutex_.get());
    if (state_ != kConnected) {
      // We should not have made a request to Redis at all.
      // Thus, reply is non-existent and there are no errors to log.
      DCHECK(!reply);
      return false;
    }
  }
  bool valid = false;
  if (reply == nullptr) {
    LogRedisContextError(redis_.get(), command_executed);
  } else {
    for (int type : valid_types) {
      if (reply->type == type) {
        valid = true;
      }
    }
    if (!valid) {
      if (reply->type == REDIS_REPLY_ERROR) {
        GoogleString error(reply->str, reply->len);
        LOG(DFATAL) << command_executed << ": redis returned error: " << error;
        redis_cache_->message_handler_->Message(
            kError, "%s: redis returned error: %s", command_executed,
            error.c_str());
      } else {
        LOG(DFATAL) << command_executed
                    << ": unexpected reply type from redis: " << reply->type;
        redis_cache_->message_handler_->Message(
            kError, "%s: unexpected reply type from redis: %d",
            command_executed, reply->type);
      }
    }
  }
  // We have to UpdateState() in the very end of ValidateRedisReply() because it
  // can reset redis_, but we want to access errors which are stored there.
  ScopedMutex lock(state_mutex_.get());
  UpdateState();
  return valid;
}

}  // namespace net_instaweb
