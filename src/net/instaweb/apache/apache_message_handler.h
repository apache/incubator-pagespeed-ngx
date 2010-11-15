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

#ifndef NET_INSTAWEB_APACHE_APACHE_MESSAGE_HANDLER_H_
#define NET_INSTAWEB_APACHE_APACHE_MESSAGE_HANDLER_H_

#include <string>
#include "base/basictypes.h"
#include "net/instaweb/util/public/message_handler.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

struct server_rec;

namespace net_instaweb {

// Implementation of an HTML parser message handler that uses Apache
// logging to emit messsages.
class ApacheMessageHandler : public MessageHandler {
 public:
  ApacheMessageHandler(const server_rec* server, const StringPiece& version);

 protected:
  virtual void MessageVImpl(MessageType type, const char* msg, va_list args);

  virtual void FileMessageVImpl(MessageType type, const char* filename,
                                int line, const char* msg, va_list args);

 private:
  int GetApacheLogLevel(MessageType type);
  std::string Format(const char* msg, va_list args);

  const server_rec* server_rec_;
  const std::string version_;

  DISALLOW_COPY_AND_ASSIGN(ApacheMessageHandler);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_MESSAGE_HANDLER_H_
