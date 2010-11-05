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

#include "net/instaweb/apache/apache_message_handler.h"

#include "net/instaweb/util/public/string_util.h"

#include "httpd.h"
// When HAVE_SYSLOG is defined, apache http_log.h will include syslog.h, which
// #defined LOG_* as numbers. This conflicts with what we are using those here.
#undef HAVE_SYSLOG
#include "http_log.h"

namespace net_instaweb {

int ApacheMessageHandler::GetApacheLogLevel(MessageType type) {
  switch (type) {
    case kInfo:
      // TODO(sligocki): Do we want this to be INFO or NOTICE.
      return APLOG_INFO;
    case kWarning:
      return APLOG_WARNING;
    case kError:
      return APLOG_ERR;
    case kFatal:
      return APLOG_ALERT;
  }
}

void ApacheMessageHandler::MessageVImpl(MessageType type, const char* msg,
                                        va_list args) {
  int log_level = GetApacheLogLevel(type);
  std::string formatted_message = Format(msg, args);
  ap_log_error(APLOG_MARK, log_level, APR_SUCCESS, server_rec_,
               "%s", formatted_message.c_str());
}

void ApacheMessageHandler::FileMessageVImpl(MessageType type, const char* file,
                                            int line, const char* msg,
                                            va_list args) {
  int log_level = GetApacheLogLevel(type);
  std::string formatted_message = Format(msg, args);
  ap_log_error(APLOG_MARK, log_level, APR_SUCCESS, server_rec_,
               "%s:%d: %s", file, line, formatted_message.c_str());
}

// TODO(sligocki): It'd be nice not to do so much string copying.
std::string ApacheMessageHandler::Format(const char* msg, va_list args) {
  std::string buffer;

  // Ignore the name of this routine: it formats with vsnprintf.
  // See base/stringprintf.cc.
  StringAppendV(&buffer, msg, args);
  return buffer;
}

}  // namespace net_instaweb
