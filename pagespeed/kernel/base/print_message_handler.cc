/*
 * Copyright 2012 Google Inc.
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

// Author: dgerman@google.com (David German)

#include "pagespeed/kernel/base/print_message_handler.h"

#include <cstdio>

#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

PrintMessageHandler::PrintMessageHandler() {
}

PrintMessageHandler::~PrintMessageHandler() {
}

void PrintMessageHandler::MessageSImpl(MessageType type,
                                       const GoogleString& message) {
  fputs(message.c_str(), stdout);
  fflush(stdout);
}

void PrintMessageHandler::FileMessageSImpl(
    MessageType type, const char* filename, int line,
    const GoogleString& message) {
  // This is the PrintMessageHandler, so we always print!
  MessageSImpl(type, message);
}

}  // namespace net_instaweb
