/*
 * Copyright 2012 Google Inc.
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
// Author: mmohabey@google.com (Megha Mohabey)

#ifndef NET_INSTAWEB_HTTP_PUBLIC_HTTP_VALUE_WRITER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_HTTP_VALUE_WRITER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class HTTPCache;
class HTTPValue;
class MessageHandler;
class ResponseHeaders;

// Wrappper for buffering an HTTPValue. HTTPValueWriter ensures that an
// HTTPValue which cannot be eventually cached is not buffered.
class HTTPValueWriter {
 public:
  HTTPValueWriter(HTTPValue* value, HTTPCache* cache)
      : value_(value),
        cache_(cache),
        has_buffered_(true) {}

  void SetHeaders(ResponseHeaders* headers);

  bool Write(const StringPiece& str, MessageHandler* handler);

  bool has_buffered() const { return has_buffered_; }

  // Checks if the http_value should be buffered or not depending on whether we
  // can eventually cache it. It also clears the http_value if it can not be
  // buffered.
  bool CheckCanCacheElseClear(ResponseHeaders* headers);

 private:
  HTTPValue* value_;
  HTTPCache* cache_;
  bool has_buffered_;
  DISALLOW_COPY_AND_ASSIGN(HTTPValueWriter);
};

}  // namespace net_instaweb
#endif  // NET_INSTAWEB_HTTP_PUBLIC_HTTP_VALUE_WRITER_H_
