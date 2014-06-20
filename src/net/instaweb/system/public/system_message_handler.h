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

#ifndef NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_MESSAGE_HANDLER_H_
#define NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_MESSAGE_HANDLER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class SharedCircularBuffer;
class Timer;
class Writer;

// Implementation of methods that are shared by both ApacheMessageHandler
// and NgxMessageHandler.
class SystemMessageHandler : public GoogleMessageHandler {
 public:
  SystemMessageHandler(Timer* timer, AbstractMutex* mutex);

  virtual ~SystemMessageHandler();

  // When we initialize SystemMessageHandler in the SystemRewriteDriverFactory,
  // the factory's SharedCircularBuffer is not initialized yet.
  // We need to set buffer_ later in RootInit() or ChildInit().
  void set_buffer(SharedCircularBuffer* buff);

  void SetPidString(const int64 pid) {
    pid_string_ = StrCat("[", Integer64ToString(pid), "]");
  }

  // Dump contents of SharedCircularBuffer.
  virtual bool Dump(Writer* writer);

 protected:
  // Add messages to the SharedCircularBuffer.
  virtual void AddMessageToBuffer(MessageType type,
                                  GoogleString formatted_message);

 private:
  // This timer is used to prepend time when writing a message
  // to SharedCircularBuffer.
  Timer* timer_;
  scoped_ptr<AbstractMutex> mutex_;
  SharedCircularBuffer* buffer_;
  // This handler is for internal use.
  // Some functions of SharedCircularBuffer need MessageHandler as argument,
  // We do not want to pass in another SystemMessageHandler to cause infinite
  // loop.
  GoogleMessageHandler internal_handler_;
  GoogleString pid_string_;  // String "[pid]".

  DISALLOW_COPY_AND_ASSIGN(SystemMessageHandler);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_MESSAGE_HANDLER_H_
