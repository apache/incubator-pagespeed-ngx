/**
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/string_util.h"
#include "base/logging.h"

namespace net_instaweb {

void GoogleMessageHandler::MessageVImpl(MessageType type, const char* msg,
                                        va_list args) {
  switch (type) {
    case kInfo:
      LOG(INFO) << Format(msg, args);
      break;
    case kWarning:
      LOG(WARNING) << Format(msg, args);
      break;
    case kError:
      LOG(ERROR) << Format(msg, args);
      break;
    case kFatal:
      LOG(FATAL) << Format(msg, args);
      break;
  }
}

void GoogleMessageHandler::FileMessageVImpl(MessageType type, const char* file,
                                            int line, const char* msg,
                                            va_list args) {
  switch (type) {
    case kInfo:
      LOG(INFO) << file << ":" << line << ": " << Format(msg, args);
      break;
    case kWarning:
      LOG(WARNING) << file << ":" << line << ": " << Format(msg, args);
      break;
    case kError:
      LOG(ERROR) << file << ":" << line << ": " << Format(msg, args);
      break;
    case kFatal:
      LOG(FATAL) << file << ":" << line << ": " << Format(msg, args);
      break;
  }
}

// TODO(sligocki): It'd be nice not to do so much string copying.
std::string GoogleMessageHandler::Format(const char* msg, va_list args) {
  std::string buffer;

  // Ignore the name of this routine: it formats with vsnprintf.
  // See base/stringprintf.cc.
  StringAppendV(&buffer, msg, args);
  return buffer;
}

}  // namespace net_instaweb
