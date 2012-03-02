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

#include "net/instaweb/util/public/print_message_handler.h"

#include <cstdarg>
#include <cstdio>

namespace net_instaweb {

PrintMessageHandler::PrintMessageHandler() {
}

PrintMessageHandler::~PrintMessageHandler() {
}

void PrintMessageHandler::MessageVImpl(MessageType type,
                                       const char* msg,
                                       va_list args) {
  GoogleString buffer;
  StringAppendV(&buffer, msg, args);
  fputs(buffer.data(), stdout);
  fflush(stdout);
}

void PrintMessageHandler::FileMessageVImpl(MessageType type,
                                           const char* filename,
                                           int line,
                                           const char* msg,
                                           va_list args) {
  // This is the PrintMessageHandler, so we always print!
  MessageVImpl(type, msg, args);
}

}  // namespace net_instaweb
