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

#ifndef PAGESPEED_KERNEL_BASE_ANNOTATED_MESSAGE_HANDLER_H_
#define PAGESPEED_KERNEL_BASE_ANNOTATED_MESSAGE_HANDLER_H_

#include <cstdarg>

#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/message_handler.h"

namespace net_instaweb {

// Class AnnotatedMessageHandler prints an annotation, in additional to the
// message printed by MessageHandler.
class AnnotatedMessageHandler : public MessageHandler {
 public:
  explicit AnnotatedMessageHandler(MessageHandler* handler);
  AnnotatedMessageHandler(const GoogleString& annotation,
                          MessageHandler* handler);
  virtual ~AnnotatedMessageHandler();

 protected:
  virtual void MessageVImpl(MessageType type, const char* msg, va_list args);
  virtual void MessageSImpl(MessageType type, const GoogleString& message);

  virtual void FileMessageVImpl(MessageType type, const char* filename,
                                int line, const char* msg, va_list args);
  virtual void FileMessageSImpl(MessageType type, const char* filename,
                                int line, const GoogleString& message);

 private:
  GoogleString annotation_;
  MessageHandler* message_handler_;  // Not owned
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_ANNOTATED_MESSAGE_HANDLER_H_
