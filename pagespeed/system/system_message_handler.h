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

#ifndef PAGESPEED_SYSTEM_SYSTEM_MESSAGE_HANDLER_H_
#define PAGESPEED_SYSTEM_SYSTEM_MESSAGE_HANDLER_H_

#include <cstdarg>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class Timer;
class Writer;

// Implementation of methods that are shared by both ApacheMessageHandler
// and NgxMessageHandler.
class SystemMessageHandler : public GoogleMessageHandler {
 public:
  SystemMessageHandler(Timer* timer, AbstractMutex* mutex);

  virtual ~SystemMessageHandler();

  // When we initialize SystemMessageHandler in the SystemRewriteDriverFactory,
  // the factory's buffer_ is not initialized yet.  In a live server, we need to
  // set buffer_ later in RootInit() or ChildInit().
  void set_buffer(Writer* buff);

  void SetPidString(const int64 pid) {
    pid_string_ = StrCat("[", Integer64ToString(pid), "]");
  }

  // Dump contents of SharedCircularBuffer.
  virtual bool Dump(Writer* writer);

 protected:
  // Add messages to the SharedCircularBuffer.
  void AddMessageToBuffer(MessageType type, StringPiece formatted_message);
  void AddMessageToBuffer(MessageType type, const char* file, int line,
                          StringPiece formatted_message);

  // Since we subclass GoogleMessageHandler but want to format messages
  // internally we must provide overrides of these two logging methods.
  virtual void MessageVImpl(MessageType type, const char* msg, va_list args);
  virtual void FileMessageVImpl(MessageType type, const char* file,
                                int line, const char* msg, va_list args);

 private:
  friend class SystemMessageHandlerTest;

  // This timer is used to prepend time when writing a message
  // to SharedCircularBuffer.
  Timer* timer_;
  scoped_ptr<AbstractMutex> mutex_;
  Writer* buffer_;
  // This handler is for internal use.
  // Some functions of SharedCircularBuffer need MessageHandler as argument,
  // We do not want to pass in another SystemMessageHandler to cause infinite
  // loop.
  GoogleMessageHandler internal_handler_;
  GoogleString pid_string_;  // String "[pid]".
  NullMessageHandler null_handler_;

  DISALLOW_COPY_AND_ASSIGN(SystemMessageHandler);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_SYSTEM_MESSAGE_HANDLER_H_
