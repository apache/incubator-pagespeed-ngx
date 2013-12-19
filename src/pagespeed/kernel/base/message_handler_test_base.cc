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

// Author: bmcquade@google.com (Bryan McQuade)

#include "pagespeed/kernel/base/message_handler_test_base.h"

#include <cstdarg>
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

void TestMessageHandler::MessageVImpl(MessageType type, const char* msg,
                                      va_list args) {
  GoogleString message;
  StringAppendF(&message, "%s: ", MessageTypeToString(type));
  StringAppendV(&message, msg, args);
  messages_.push_back(message);
}

void TestMessageHandler::FileMessageVImpl(MessageType type,
                                          const char* filename,
                                          int line, const char* msg,
                                          va_list args) {
  GoogleString message;
  StringAppendF(&message, "%s: %s: %d: ", MessageTypeToString(type),
                filename, line);
  StringAppendV(&message, msg, args);
  messages_.push_back(message);
}

}  // namespace net_instaweb
