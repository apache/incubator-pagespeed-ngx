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

// Author:  morlovich@google.com (Maksim Orlovich)

#include "pagespeed/kernel/base/mock_message_handler.h"

#include <map>
#include <utility>

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/writer.h"

namespace net_instaweb {

MockMessageHandler::MockMessageHandler(AbstractMutex* mutex)
    : mutex_(mutex) {
}

MockMessageHandler::~MockMessageHandler() {
}

void MockMessageHandler::MessageSImpl(MessageType type,
                                      const GoogleString& message) {
  ScopedMutex hold_mutex(mutex_.get());
  if (ShouldPrintMessage(message)) {
    internal_handler_.MessageSImpl(type, message);
    GoogleString type_str = MessageTypeToString(type);
    StrAppend(&buffer_, type_str.substr(0, 1), "[Wed Jan 01 00:00:00 2014] ");
    StrAppend(&buffer_, "[", type_str, "] [00000] ");
    StrAppend(&buffer_, message, "\n");
  } else {
    ++skipped_message_counts_[type];
  }
  ++message_counts_[type];
}

void MockMessageHandler::FileMessageSImpl(MessageType type,
                                          const char* filename, int line,
                                          const GoogleString& message) {
  ScopedMutex hold_mutex(mutex_.get());
  if (ShouldPrintMessage(message)) {
    internal_handler_.FileMessageSImpl(type, filename, line, message);
    GoogleString type_str = MessageTypeToString(type);
    StrAppend(&buffer_, type_str.substr(0, 1), "[Wed Jan 01 00:00:00 2014] ");
    StrAppend(&buffer_, "[", type_str, "] [00000] ");
    StrAppend(&buffer_, "[", filename, ":", IntegerToString(line), "] ");
    StrAppend(&buffer_, message, "\n");
  } else {
    ++skipped_message_counts_[type];
  }
  ++message_counts_[type];
}

int MockMessageHandler::MessagesOfType(MessageType type) const {
  ScopedMutex hold_mutex(mutex_.get());
  return MessagesOfTypeImpl(message_counts_, type);
}

int MockMessageHandler::SkippedMessagesOfType(MessageType type) const {
  ScopedMutex hold_mutex(mutex_.get());
  return MessagesOfTypeImpl(skipped_message_counts_, type);
}

int MockMessageHandler::MessagesOfTypeImpl(const MessageCountMap& counts,
                                           MessageType type) const {
  MessageCountMap::const_iterator i = counts.find(type);
  if (i != counts.end()) {
    return i->second;
  } else {
    return 0;
  }
}

int MockMessageHandler::TotalMessages() const {
  ScopedMutex hold_mutex(mutex_.get());
  return TotalMessagesImpl(message_counts_);
}

int MockMessageHandler::TotalSkippedMessages() const {
  ScopedMutex hold_mutex(mutex_.get());
  return TotalMessagesImpl(skipped_message_counts_);
}

int MockMessageHandler::TotalMessagesImpl(const MessageCountMap& counts) const {
  int total = 0;
  for (MessageCountMap::const_iterator i = counts.begin();
       i != counts.end(); ++i) {
    total += i->second;
  }
  return total;
}

int MockMessageHandler::SeriousMessages() const {
  ScopedMutex hold_mutex(mutex_.get());
  int num = TotalMessagesImpl(message_counts_) -
       MessagesOfTypeImpl(message_counts_, kInfo);
  return num;
}

void MockMessageHandler::set_mutex(AbstractMutex* mutex) {
  mutex_->DCheckUnlocked();
  mutex_.reset(mutex);
}

void MockMessageHandler::AddPatternToSkipPrinting(
    const char* pattern) {
  patterns_to_skip_.Allow(GoogleString(pattern));
}

bool MockMessageHandler::ShouldPrintMessage(
    const StringPiece& msg) {
  return !patterns_to_skip_.Match(msg, false);
}

bool MockMessageHandler::Dump(Writer* writer) {
  if (buffer_ == "") {
    return false;
  }
  return (writer->Write(buffer_, &internal_handler_));
}

}  // namespace net_instaweb
