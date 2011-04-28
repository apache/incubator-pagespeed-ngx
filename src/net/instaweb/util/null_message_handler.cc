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

// Author: lsong@google.com (Libo Song)

#include "net/instaweb/util/public/null_message_handler.h"

#include <cstdarg>

#include "net/instaweb/util/public/message_handler.h"

namespace net_instaweb {

NullMessageHandler::~NullMessageHandler() {
}

void NullMessageHandler::MessageVImpl(MessageType type, const char* msg,
                                      va_list args) {
}

void NullMessageHandler::FileMessageVImpl(MessageType type, const char* file,
                                          int line, const char* msg,
                                          va_list args) {
}

}  // namespace net_instaweb
