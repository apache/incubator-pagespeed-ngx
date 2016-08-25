/*
 * Copyright 2014 Google Inc.
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

// Author: xqyin@google.com (XiaoQian Yin)

#include "pagespeed/system/system_message_handler.h"

#include <unistd.h>

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/time_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/writer.h"

namespace net_instaweb {

SystemMessageHandler::SystemMessageHandler(Timer* timer, AbstractMutex* mutex)
    : timer_(timer),
      mutex_(mutex),
      buffer_(NULL) {
  SetPidString(static_cast<int64>(getpid()));
}

SystemMessageHandler::~SystemMessageHandler() {
}

void SystemMessageHandler::set_buffer(Writer* buff) {
  ScopedMutex lock(mutex_.get());
  buffer_ = buff;
}

void SystemMessageHandler::AddMessageToBuffer(MessageType type,
                                              StringPiece formatted_message) {
  AddMessageToBuffer(type, nullptr, 0, formatted_message);
}

void SystemMessageHandler::AddMessageToBuffer(MessageType type,
                                              const char* file, int line,
                                              StringPiece formatted_message) {
  if (formatted_message.empty()) {
    return;
  }
  GoogleString message;
  GoogleString time;
  const char* type_str = MessageTypeToString(type);
  if (!ConvertTimeToString(timer_->NowMs(), &time)) {
    time = "?";
  }
  StringPiece type_char = StringPiece(type_str, 1);
  StringPieceVector lines;
  SplitStringPieceToVector(formatted_message, "\n", &lines, false);
  StrAppend(&message, type_char, "[", time, "] [", type_str, "] ");
  StrAppend(&message, pid_string_, " ");
  if (file) {
    StrAppend(&message, "[", file, ":", IntegerToString(line), "] ");
  }
  StrAppend(&message, lines[0], "\n");
  for (int i = 1, n = lines.size(); i < n; ++i) {
    StrAppend(&message, type_char, lines[i], "\n");
  }
  {
    ScopedMutex lock(mutex_.get());
    // Cannot write to SharedCircularBuffer before it's set up.
    if (buffer_ != NULL) {
      NullMessageHandler null_handler;
      buffer_->Write(message, &null_handler);
    }
  }
}

void SystemMessageHandler::MessageVImpl(MessageType type, const char* msg,
                                        va_list args) {
  GoogleString buffer;
  FormatTo(&buffer, msg, args);
  MessageSImpl(type, buffer);
}

void SystemMessageHandler::FileMessageVImpl(MessageType type, const char* file,
                                            int line, const char* msg,
                                            va_list args) {
  GoogleString buffer;
  FormatTo(&buffer, msg, args);
  FileMessageSImpl(type, file, line, buffer);
}

bool SystemMessageHandler::Dump(Writer* writer) {
  if (buffer_ == NULL) {
    return false;
  }
  return buffer_->Dump(writer, &internal_handler_);
}

}  // namespace net_instaweb
