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
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/log_message_handler.h"
#include "net/instaweb/util/public/time_util.h"

#include "httpd.h"
#include "net/instaweb/apache/apache_logging_includes.h"

namespace {

// This name will be prefixed to every logged message.  This could be made
// smaller if people think it's too long.  In my opinion it's probably OK,
// and it would be good to let people know where messages are coming from.
const char kModuleName[] = "mod_pagespeed";

}

namespace net_instaweb {

// filename_prefix of ApacheRewriteDriverFactory is needed to initialize
// SharedCircuarBuffer. However, ApacheRewriteDriverFactory needs
// ApacheMessageHandler before its filename_prefix is set. So we initialize
// ApacheMessageHandler without SharedCircularBuffer first, then initalize its
// SharedCircularBuffer in RootInit() when filename_prefix is set.
ApacheMessageHandler::ApacheMessageHandler(const server_rec* server,
                                           const StringPiece& version,
                                           Timer* timer)
    : server_rec_(server),
      version_(version.data(), version.size()),
      timer_(timer),
      buffer_(NULL) {
  // Tell log_message_handler about this server_rec and version.
  log_message_handler::AddServerConfig(server_rec_, version);
  // Set pid string.
  SetPidString(static_cast<int64>(getpid()));
  // TODO(jmarantz): consider making this a little terser by default.
  // The string we expect in is something like "0.9.1.1-171" and we will
  // may be able to pick off some of the 5 fields that prove to be boring.
}

bool ApacheMessageHandler::Dump(Writer* writer) {
  // Can't dump before SharedCircularBuffer is set up.
  if (buffer_ == NULL) {
    return false;
  }
  return buffer_->Dump(writer, &handler_);
}

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

  // This should never fall through, but some compilers seem to complain if
  // we don't include this.
  return APLOG_ALERT;
}

void ApacheMessageHandler::MessageVImpl(MessageType type, const char* msg,
                                        va_list args) {
  int log_level = GetApacheLogLevel(type);
  GoogleString formatted_message = Format(msg, args);
  ap_log_error(APLOG_MARK, log_level, APR_SUCCESS, server_rec_,
               "[%s %s @%ld] %s",
               kModuleName, version_.c_str(), static_cast<long>(getpid()),
               formatted_message.c_str());
  // Can not write to SharedCircularBuffer before it's set up.
  if (buffer_ != NULL) {
    // Prepend time (down to microseconds) and severity to message.
    // Format is [time:microseconds] [severity] [pid] message.
    GoogleString message;
    GoogleString time_us;
    ConvertTimeToStringWithUs(timer_->NowUs(), &time_us);
    StrAppend(&message, "[", time_us, "] ",
              "[", MessageTypeToString(type), "] ");
    StrAppend(&message, pid_string_, " ", formatted_message, "\n");
    buffer_->Write(message);
  }
}

void ApacheMessageHandler::FileMessageVImpl(MessageType type, const char* file,
                                            int line, const char* msg,
                                            va_list args) {
  int log_level = GetApacheLogLevel(type);
  GoogleString formatted_message = Format(msg, args);
  ap_log_error(APLOG_MARK, log_level, APR_SUCCESS, server_rec_,
               "[%s %s @%ld] %s:%d: %s",
               kModuleName, version_.c_str(), static_cast<long>(getpid()),
               file, line, formatted_message.c_str());
}

// TODO(sligocki): It'd be nice not to do so much string copying.
GoogleString ApacheMessageHandler::Format(const char* msg, va_list args) {
  GoogleString buffer;

  // Ignore the name of this routine: it formats with vsnprintf.
  // See base/stringprintf.cc.
  StringAppendV(&buffer, msg, args);
  return buffer;
}

}  // namespace net_instaweb
