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

#ifndef PAGESPEED_KERNEL_BASE_WRITER_H_
#define PAGESPEED_KERNEL_BASE_WRITER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {
class MessageHandler;

// Interface for writing bytes to an output stream.
class Writer {
 public:
  Writer() {}
  virtual ~Writer();

  virtual bool Write(const StringPiece& str, MessageHandler* handler) = 0;
  virtual bool Flush(MessageHandler* message_handler) = 0;

  // Dumps the contents of what's been written to the Writer.  Many
  // Writer implementations will not be able to do this, and the default
  // implementation will return false.  But StringWriter and
  // SharedCircularBuffer can dump their contents, and override
  // this with implementations that return true.
  virtual bool Dump(Writer* writer, MessageHandler* message_handler);

 private:
  DISALLOW_COPY_AND_ASSIGN(Writer);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_WRITER_H_
