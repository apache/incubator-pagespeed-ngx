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

#ifndef PAGESPEED_KERNEL_BASE_MESSAGE_HANDLER_H_
#define PAGESPEED_KERNEL_BASE_MESSAGE_HANDLER_H_

#include <cstdarg>

#include "pagespeed/kernel/base/printf_format.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class Writer;

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

  // Convert string to MessageType.
  static MessageType StringToMessageType(const StringPiece& msg);

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

  // Unformatted messaging.  Delegating classes can call directly to
  // MessageSImpl and FileMessageSImpl, but clients should call these methods.
  void MessageS(MessageType type, const GoogleString& message);
  void FileMessageS(MessageType type, const char* filename, int line,
                    const GoogleString& message);

  // Dumps recent messages, or returns false if this was not possible.
  // The default implementation returns false, but derived classes may
  // add a circular buffer to implement this, e.g. SharedCircularBuffer.
  virtual bool Dump(Writer* writer);

  // Parse the dumped log into messages.
  virtual void ParseMessageDumpIntoMessages(
      StringPiece message_dump, StringPieceVector* messages);

  // Return the message type.
  virtual MessageType GetMessageType(StringPiece message);

  // Make the message look more readable when rendering to history page.
  virtual StringPiece ReformatMessage(StringPiece message);

 protected:
  // 'MessageVImpl' and 'FileMessageVImpl' have default implementations in terms
  // of MessageSImpl and FileMessageSImpl.
  virtual void MessageVImpl(MessageType type, const char* msg,
                            va_list args);
  virtual void FileMessageVImpl(MessageType type, const char* filename,
                                int line, const char* msg, va_list args);
  // These methods don't perform any formatting on the string, since it turns
  // out delegating message_handlers generally only need to format once at the
  // top of the stack and then propagate the formatted string inwards.
  virtual void MessageSImpl(MessageType type, const GoogleString& message) = 0;
  virtual void FileMessageSImpl(
      MessageType type, const char* filename, int line,
      const GoogleString& message) = 0;
  // FormatTo appends to *buffer.
  void FormatTo(GoogleString* buffer, const char* msg, va_list args);

 private:
  // The minimum message type to log at. Any messages below this level
  // will not be logged.
  MessageType min_message_type_;
};

// Macros for logging messages.
#define PS_LOG_INFO(handler, ...) \
    (handler)->Info(__FILE__, __LINE__, __VA_ARGS__)
#define PS_LOG_WARN(handler, ...) \
    (handler)->Warning(__FILE__, __LINE__, __VA_ARGS__)
#define PS_LOG_ERROR(handler, ...) \
    (handler)->Error(__FILE__, __LINE__, __VA_ARGS__)
#define PS_LOG_FATAL(handler, ...) \
    (handler)->FatalError(__FILE__, __LINE__, __VA_ARGS__)

#ifndef NDEBUG
#define PS_LOG_DFATAL(handler, ...) \
    PS_LOG_FATAL(handler, __VA_ARGS__)
#else
#define PS_LOG_DFATAL(handler, ...) \
    PS_LOG_ERROR(handler, __VA_ARGS__)
#endif  // NDEBUG

// Macros for logging debugging messages. They expand to no-ops in opt-mode
// builds.
#ifndef NDEBUG
#define PS_DLOG_INFO(handler, ...) \
    PS_LOG_INFO(handler, __VA_ARGS__)
#define PS_DLOG_WARN(handler, ...) \
    PS_LOG_WARN(handler, __VA_ARGS__)
#define PS_DLOG_ERROR(handler, ...) \
    PS_LOG_ERROR(handler, __VA_ARGS__)
#else
// A dummy function that will be optimized away. This is needed
// because the macros below are sometimes used in comma expressions and
// thus can't expand to nothing.
inline void NoOpMacroPlaceholder() {}

#define PS_DLOG_INFO(handler, ...) ::net_instaweb::NoOpMacroPlaceholder()
#define PS_DLOG_WARN(handler, ...) ::net_instaweb::NoOpMacroPlaceholder()
#define PS_DLOG_ERROR(handler, ...) ::net_instaweb::NoOpMacroPlaceholder()
#endif  // NDEBUG
}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_MESSAGE_HANDLER_H_
