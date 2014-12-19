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

#ifndef PAGESPEED_KERNEL_BASE_PRINT_MESSAGE_HANDLER_H_
#define PAGESPEED_KERNEL_BASE_PRINT_MESSAGE_HANDLER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

// A message handler that spews messages to stdout, with no annotation.
class PrintMessageHandler : public MessageHandler {
 public:
  PrintMessageHandler();
  virtual ~PrintMessageHandler();

 protected:
  virtual void MessageSImpl(MessageType type, const GoogleString& message);
  virtual void FileMessageSImpl(MessageType type, const char* filename,
                                int line, const GoogleString& message);

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintMessageHandler);
};

}  // namespace net_instaweb
#endif  // PAGESPEED_KERNEL_BASE_PRINT_MESSAGE_HANDLER_H_
