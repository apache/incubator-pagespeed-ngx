// Copyright 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: jefftk@google.com (Jeff Kaufman)

#ifndef NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_SERVER_CONTEXT_H_
#define NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_SERVER_CONTEXT_H_

#include "net/instaweb/rewriter/public/server_context.h"

#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/scoped_ptr.h"

namespace net_instaweb {

class Statistics;
class SystemRewriteDriverFactory;
class SystemRewriteOptions;
class Variable;

// A server context with features specific to a PSOL port on a unix system.
class SystemServerContext : public ServerContext {
 public:
  explicit SystemServerContext(SystemRewriteDriverFactory* driver_factory);

  // Implementations should call this method on every request, both for html and
  // resources, to avoid serving stale resources.
  //
  // TODO(jmarantz): allow a URL-based mechanism to flush cache, even if
  // we implement it by simply writing the cache.flush file so other
  // servers can see it.  Note that using shared-memory is not a great
  // plan because we need the cache-invalidation to persist across server
  // restart.
  void FlushCacheIfNecessary();

  SystemRewriteOptions* system_rewrite_options();

  static void InitStats(Statistics* statistics);

 protected:
  // Flush the cache by updating the cache flush timestamp in the global
  // options.  This will change its signature, which is part of the cache key,
  // and so all previously cached entries will be unreachable.
  //
  // Returns true if it actually updated the timestamp, false if the existing
  // cache flush timestamp was newer or the same as the one provided.
  //
  // Subclasses which add aditional configurations need to override this method
  // to additionally update the cache flush timestamp in those other
  // configurations.  See ApacheServerContext::UpdateCacheFlushTimestampMs where
  // the separate SpdyConfig that mod_pagespeed uses when using SPDY also needs
  // to have it's timestamp bumped.
  virtual bool UpdateCacheFlushTimestampMs(int64 timestamp_ms);

 private:
  // State used to implement periodic polling of $FILE_PREFIX/cache.flush.
  // last_cache_flush_check_sec_ is ctor-initialized to 0 so the first
  // time we Poll we will read the file.
  scoped_ptr<AbstractMutex> cache_flush_mutex_;
  int64 last_cache_flush_check_sec_;  // seconds since 1970

  Variable* cache_flush_count_;
  Variable* cache_flush_timestamp_ms_;

  DISALLOW_COPY_AND_ASSIGN(SystemServerContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_SERVER_CONTEXT_H_
