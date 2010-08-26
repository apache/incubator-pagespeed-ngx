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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_WRITER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_WRITER_H_

#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;

// Interface for writing bytes to an output stream.
class Writer {
 public:
  virtual ~Writer();

  virtual bool Write(const StringPiece& str, MessageHandler* handler) = 0;

  virtual bool Flush(MessageHandler* message_handler) = 0;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_WRITER_H_
