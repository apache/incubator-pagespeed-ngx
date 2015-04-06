// Copyright 2010 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: sligocki@google.com (Shawn Ligocki)

#ifndef PAGESPEED_APACHE_APACHE_MESSAGE_HANDLER_H_
#define PAGESPEED_APACHE_APACHE_MESSAGE_HANDLER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/system/system_message_handler.h"

struct server_rec;

namespace net_instaweb {

class AbstractMutex;
class Timer;

// Implementation of an HTML parser message handler that uses Apache
// logging to emit messsages.
class ApacheMessageHandler : public SystemMessageHandler {
 public:
  // version is a string added to each message.
  // Timer is used to generate timestamp for messages in shared memory.
  ApacheMessageHandler(const server_rec* server, const StringPiece& version,
                       Timer* timer, AbstractMutex* mutex);

  // Installs a signal handler for common crash signals that tries to print
  // out a backtrace.
  static void InstallCrashHandler(server_rec* global_server);

 protected:
  virtual void MessageSImpl(MessageType type, const GoogleString& message);

  virtual void FileMessageSImpl(MessageType type, const char* filename,
                                int line, const GoogleString& message);

 private:
  int GetApacheLogLevel(MessageType type);

  const server_rec* server_rec_;
  const GoogleString version_;

  DISALLOW_COPY_AND_ASSIGN(ApacheMessageHandler);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_APACHE_APACHE_MESSAGE_HANDLER_H_
