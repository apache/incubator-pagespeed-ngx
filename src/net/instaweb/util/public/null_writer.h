/*
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_NULL_WRITER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_NULL_WRITER_H_

#include "net/instaweb/util/public/writer.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
class MessageHandler;

// A writer that silently eats the bytes.  This can be used, for
// example, with writers is designed to cascade to another one, such
// as CountingWriter.  If you just want to count the bytes and don't
// want to store them, you can pass a NullWriter to a CountingWriter's
// constructor.
class NullWriter : public Writer {
 public:
  explicit NullWriter() { }
  virtual ~NullWriter();
  virtual bool Write(const StringPiece& str, MessageHandler* handler);
  virtual bool Flush(MessageHandler* handler);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_NULL_WRITER_H_
