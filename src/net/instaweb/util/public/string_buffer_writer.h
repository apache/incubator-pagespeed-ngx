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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_STRING_BUFFER_WRITER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_STRING_BUFFER_WRITER_H_

#include "base/basictypes.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

class StringBuffer;

// Writer implementation for directing HTML output to a StringBuffer.
class StringBufferWriter : public Writer {
 public:
  explicit StringBufferWriter(StringBuffer* str) : string_(str) { }
  virtual ~StringBufferWriter();
  virtual bool Write(const StringPiece& str, MessageHandler* message_handler);
  virtual bool Flush(MessageHandler* message_handler);
 private:
  StringBuffer* string_;

  DISALLOW_COPY_AND_ASSIGN(StringBufferWriter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_STRING_BUFFER_WRITER_H_
