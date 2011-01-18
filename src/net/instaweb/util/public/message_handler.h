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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_MESSAGE_HANDLER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_MESSAGE_HANDLER_H_

#include <stdarg.h>

#include "net/instaweb/util/public/printf_format.h"

namespace net_instaweb {

enum MessageType {
  kInfo,
  kWarning,
  kError,
  kFatal
};

class MessageHandler {
 public:
  MessageHandler();
  virtual ~MessageHandler();

  // String representation for MessageType.
  const char* MessageTypeToString(const MessageType type) const;

  // Specify the minimum message type. Lower message types will not be
  // logged.
  void set_min_message_type(MessageType min) { min_message_type_ = min; }

  // Log an info, warning, error or fatal error message.
  void Message(MessageType type, const char* msg, ...)
      INSTAWEB_PRINTF_FORMAT(3, 4);
  void MessageV(MessageType type, const char* msg, va_list args);

  // Log a message with a filename and line number attached.
  void FileMessage(MessageType type, const char* filename, int line,
                   const char* msg, ...) INSTAWEB_PRINTF_FORMAT(5, 6);
  void FileMessageV(MessageType type, const char* filename, int line,
                    const char* msg, va_list args);


  // Conditional errors.
  void Check(bool condition, const char* msg, ...) INSTAWEB_PRINTF_FORMAT(3, 4);
  void CheckV(bool condition, const char* msg, va_list args);


  // Convenience functions for FileMessage for backwards compatibility.
  // TODO(sligocki): Rename these to InfoAt, ... so that Info, ... can be used
  // for general Messages.
  void Info(const char* filename, int line, const char* msg, ...)
      INSTAWEB_PRINTF_FORMAT(4, 5);
  void Warning(const char* filename, int line, const char* msg, ...)
      INSTAWEB_PRINTF_FORMAT(4, 5);
  void Error(const char* filename, int line, const char* msg, ...)
      INSTAWEB_PRINTF_FORMAT(4, 5);
  void FatalError(const char* filename, int line, const char* msg, ...)
      INSTAWEB_PRINTF_FORMAT(4, 5);

  void InfoV(const char* filename, int line, const char* msg, va_list args) {
    FileMessageV(kInfo, filename, line, msg, args);
  }
  void WarningV(const char* filename, int line, const char* msg, va_list a) {
    FileMessageV(kWarning, filename, line, msg, a);
  }
  void ErrorV(const char* filename, int line, const char* msg, va_list args) {
    FileMessageV(kError, filename, line, msg, args);
  }
  void FatalErrorV(const char* fname, int line, const char* msg, va_list a) {
    FileMessageV(kFatal, fname, line, msg, a);
  }

 protected:
  virtual void MessageVImpl(MessageType type, const char* msg,
                            va_list args) = 0;
  virtual void FileMessageVImpl(MessageType type, const char* filename,
                                int line, const char* msg, va_list args) = 0;

 private:
  // The minimum message type to log at. Any messages below this level
  // will not be logged.
  MessageType min_message_type_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_MESSAGE_HANDLER_H_
