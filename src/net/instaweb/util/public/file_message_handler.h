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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_FILE_MESSAGE_HANDLER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_FILE_MESSAGE_HANDLER_H_

#include <cstdarg>
#include <cstdio>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/message_handler.h"

namespace net_instaweb {

// Message handler implementation for directing all error and
// warning messages to a file.
class FileMessageHandler : public MessageHandler {
 public:
  explicit FileMessageHandler(FILE* file);

 protected:
  virtual void MessageVImpl(MessageType type, const char* msg, va_list args);

  virtual void FileMessageVImpl(MessageType type, const char* filename,
                                int line, const char* msg, va_list args);

 private:
  FILE* file_;

  DISALLOW_COPY_AND_ASSIGN(FileMessageHandler);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_FILE_MESSAGE_HANDLER_H_
