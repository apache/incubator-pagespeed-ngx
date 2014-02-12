/*
 * Copyright 2013 Google Inc.
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

// Author: Huibao Lin

#include "pagespeed/kernel/base/annotated_message_handler.h"

#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

AnnotatedMessageHandler::AnnotatedMessageHandler(MessageHandler* handler) :
    message_handler_(handler) {
}

AnnotatedMessageHandler::AnnotatedMessageHandler(const GoogleString& annotation,
                                                 MessageHandler* handler) :
    message_handler_(handler) {
  annotation_ = annotation;
}

AnnotatedMessageHandler::~AnnotatedMessageHandler() {
}

void AnnotatedMessageHandler::MessageVImpl(MessageType type, const char* msg,
                                           va_list args) {
  message_handler_->Message(type, "%s%s", annotation_.c_str(),
                            Format(msg, args).c_str());
}

void AnnotatedMessageHandler::FileMessageVImpl(MessageType type,
                                               const char* filename,
                                               int line,
                                               const char* msg,
                                               va_list args) {
  message_handler_->FileMessage(type, filename, line, "%s%s",
                                annotation_.c_str(),
                                Format(msg, args).c_str());
}

}  // namespace net_instaweb
