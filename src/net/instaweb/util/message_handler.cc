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

#include "net/instaweb/util/public/message_handler.h"

#include <cstdarg>
#include <cstddef>

#include "base/logging.h"

namespace net_instaweb {

MessageHandler::MessageHandler() : min_message_type_(kInfo) {
}

MessageHandler::~MessageHandler() {
}

const char* MessageHandler::MessageTypeToString(const MessageType type) const {
  const char* type_string = NULL;

  // Don't include a 'default:' clause so that the compiler can tell us when
  // we are missing an enum value.  Intead use a null check for 'type_string' to
  // indicate a data corruption that avoids hitting any of the cases.
  switch (type) {
    case kInfo:
      type_string = "Info";
      break;
    case kWarning:
      type_string = "Warning";
      break;
    case kError:
      type_string = "Error";
      break;
    case kFatal:
      type_string = "Fatal";
      break;
  }
  CHECK(type_string != NULL) << "INVALID MessageType!";
  return type_string;
}

void MessageHandler::Message(MessageType type, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  MessageV(type, msg, args);
  va_end(args);
}

void MessageHandler::MessageV(MessageType type, const char* msg, va_list args) {
  if (type >= min_message_type_) {
    MessageVImpl(type, msg, args);
  }
}

void MessageHandler::FileMessage(MessageType type, const char* file, int line,
                                 const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  FileMessageV(type, file, line, msg, args);
  va_end(args);
}

void MessageHandler::FileMessageV(MessageType type, const char* filename,
                                  int line, const char* msg, va_list args) {
  if (type >= min_message_type_) {
    FileMessageVImpl(type, filename, line, msg, args);
  }
}

void MessageHandler::Check(bool condition, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  CheckV(condition, msg, args);
  va_end(args);
}

void MessageHandler::CheckV(bool condition, const char* msg, va_list args) {
  if (!condition) {
    MessageV(kFatal, msg, args);
  }
}

void MessageHandler::Info(const char* file, int line, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  InfoV(file, line, msg, args);
  va_end(args);
}

void MessageHandler::Warning(const char* file, int line, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  WarningV(file, line, msg, args);
  va_end(args);
}

void MessageHandler::Error(const char* file, int line, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  ErrorV(file, line, msg, args);
  va_end(args);
}

void MessageHandler::FatalError(
    const char* file, int line, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  FatalErrorV(file, line, msg, args);
  va_end(args);
}

}  // namespace net_instaweb
