/**
 * Copyright 2010 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_HTTP_CACHE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_HTTP_CACHE_H_

#include "net/instaweb/util/public/cache_interface.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CacheInterface;
class MessageHandler;
class MetaData;
class Timer;
class Writer;

// Implements HTTP caching semantics, including cache expiration and
// retention of the originally served cache headers.
class HTTPCache {
 public:
  explicit HTTPCache(CacheInterface* cache, Timer* timer)
      : cache_(cache),
        timer_(timer),
        force_caching_(false) {
  }

  bool Get(const char* key, MetaData* headers, Writer* writer,
           MessageHandler* message_handler);

  void Put(const char* key, const MetaData& headers,
           const StringPiece& content,
           MessageHandler* handler);

  CacheInterface::KeyState Query(const char* key, MessageHandler* handler);
  void set_force_caching(bool force) { force_caching_ = true; }

  Timer* timer() const { return timer_; }

 private:
  bool IsCurrentlyValid(const MetaData& headers);

  CacheInterface* cache_;
  Timer* timer_;
  bool force_caching_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_HTTP_CACHE_H_
