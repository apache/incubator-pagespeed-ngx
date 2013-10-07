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
//
// Author: jmarantz@google.com (Joshua Marantz)
// Author: sligocki@google.com (Shawn Ligocki)
// Author: jefftk@google.com (Jeff Kaufman)

// TODO(jefftk): share more of this code with apache's log_message_handler

#include "log_message_handler.h"

#include <unistd.h>

#include <limits>
#include <string>

#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/util/public/string_util.h"

// Make sure we don't attempt to use LOG macros here, since doing so
// would cause us to go into an infinite log loop.
#undef LOG
#define LOG USING_LOG_HERE_WOULD_CAUSE_INFINITE_RECURSION

namespace {

ngx_log_t* log = NULL;

ngx_uint_t GetNgxLogLevel(int severity) {
  switch (severity) {
    case logging::LOG_INFO:
      return NGX_LOG_INFO;
    case logging::LOG_WARNING:
      return NGX_LOG_WARN;
    case logging::LOG_ERROR:
      return NGX_LOG_ERR;
    case logging::LOG_ERROR_REPORT:
    case logging::LOG_FATAL:
      return NGX_LOG_ALERT;
    default:  // For VLOG(s)
      return NGX_LOG_DEBUG;
  }
}

bool LogMessageHandler(int severity, const char* file, int line,
                       size_t message_start, const GoogleString& str) {
  ngx_uint_t this_log_level = GetNgxLogLevel(severity);

  GoogleString message = str;
  if (severity == logging::LOG_FATAL) {
    if (base::debug::BeingDebugged()) {
      base::debug::BreakDebugger();
    } else {
      base::debug::StackTrace trace;
      std::ostringstream stream;
      trace.OutputToStream(&stream);
      message.append(stream.str());
    }
  }

  // Trim the newline off the end of the message string.
  size_t last_msg_character_index = message.length() - 1;
  if (message[last_msg_character_index] == '\n') {
    message.resize(last_msg_character_index);
  }

  ngx_log_error(this_log_level, log, 0, "[ngx_pagespeed %s] %s",
                net_instaweb::kModPagespeedVersion,
                message.c_str());

  if (severity == logging::LOG_FATAL) {
    // Crash the process to generate a dump.
    base::debug::BreakDebugger();
  }

  return true;
}

}  // namespace


namespace net_instaweb {

namespace log_message_handler {


void Install(ngx_log_t* log_in) {
  log = log_in;
  logging::SetLogMessageHandler(&LogMessageHandler);

  // All VLOG(2) and higher will be displayed as DEBUG logs if the nginx log
  // level is DEBUG.
  if (log->log_level >= NGX_LOG_DEBUG) {
    logging::SetMinLogLevel(-2);
  }
}

}  // namespace log_message_handler

}  // namespace net_instaweb
