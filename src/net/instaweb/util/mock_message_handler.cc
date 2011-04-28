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

#include "net/instaweb/util/public/mock_message_handler.h"

#include <cstdarg>
#include <map>
#include <utility>

#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/message_handler.h"

namespace net_instaweb {

MockMessageHandler::~MockMessageHandler() {
}

void MockMessageHandler::MessageVImpl(MessageType type,
                                      const char* msg,
                                      va_list args) {
  GoogleMessageHandler::MessageVImpl(type, msg, args);
  ++message_counts_[type];
}

void MockMessageHandler::FileMessageVImpl(MessageType type,
                                          const char* filename, int line,
                                          const char* msg, va_list args) {
  GoogleMessageHandler::FileMessageVImpl(type, filename, line, msg, args);
  ++message_counts_[type];
}

int MockMessageHandler::MessagesOfType(MessageType type) const {
  MessageCountMap::const_iterator i = message_counts_.find(type);
  if (i != message_counts_.end()) {
    return i->second;
  } else {
    return 0;
  }
}

int MockMessageHandler::TotalMessages() const {
  int total = 0;
  for (MessageCountMap::const_iterator i = message_counts_.begin();
       i != message_counts_.end(); ++i) {
    total += i->second;
  }
  return total;
}

int MockMessageHandler::SeriousMessages() const {
  return TotalMessages() - MessagesOfType(kInfo);
}

}  // namespace net_instaweb
