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

#include "net/instaweb/system/public/system_message_handler.h"

#include <unistd.h>

#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/shared_circular_buffer.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/time_util.h"

namespace net_instaweb {

SystemMessageHandler::SystemMessageHandler(Timer* timer, AbstractMutex* mutex)
    : timer_(timer),
      mutex_(mutex),
      buffer_(NULL) {
  SetPidString(static_cast<int64>(getpid()));
}

SystemMessageHandler::~SystemMessageHandler() {
}

void SystemMessageHandler::set_buffer(SharedCircularBuffer* buff) {
  ScopedMutex lock(mutex_.get());
  buffer_ = buff;
}

void SystemMessageHandler::AddMessageToBuffer(
    MessageType type, GoogleString formatted_message) {
  GoogleString message;
  GoogleString time;
  GoogleString type_str = MessageTypeToString(type);
  if (!ConvertTimeToString(timer_->NowMs(), &time)) {
    time = "?";
  }
  StrAppend(&message, type_str.substr(0, 1), "[", time, "] ",
            "[", type_str, "] ");
  StrAppend(&message, pid_string_, " ", formatted_message, "\n");
  {
    ScopedMutex lock(mutex_.get());
    // Cannot write to SharedCircularBuffer before it's set up.
    if (buffer_ != NULL) {
      buffer_->Write(message);
    }
  }
}

bool SystemMessageHandler::Dump(Writer* writer) {
  if (buffer_ == NULL) {
    return false;
  }
  return buffer_->Dump(writer, &internal_handler_);
}

}  // namespace net_instaweb
