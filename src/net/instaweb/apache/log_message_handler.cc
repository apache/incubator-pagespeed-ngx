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

#include "net/instaweb/apache/log_message_handler.h"

#include <string>
#include "base/debug_util.h"
#include "base/logging.h"
#include "httpd.h"

// When HAVE_SYSLOG is defined, apache http_log.h will include syslog.h, which
// #defined LOG_* as numbers. This conflicts with what we are using those here.
#undef HAVE_SYSLOG
#include "http_log.h"

// Make sure we don't attempt to use LOG macros here, since doing so
// would cause us to go into an infinite log loop.
#undef LOG
#define LOG USING_LOG_HERE_WOULD_CAUSE_INFINITE_RECURSION

namespace {

int GetApacheLogLevel(int severity) {
  switch (severity) {
    case logging::LOG_INFO:
      return APLOG_NOTICE;
    case logging::LOG_WARNING:
      return APLOG_WARNING;
    case logging::LOG_ERROR:
      return APLOG_ERR;
    case logging::LOG_ERROR_REPORT:
      return APLOG_CRIT;
    case logging::LOG_FATAL:
      return APLOG_ALERT;
    default:
      return APLOG_NOTICE;
  }
}

apr_pool_t* log_pool = NULL;

bool LogMessageHandler(int severity, const std::string& str) {
  const int log_level = GetApacheLogLevel(severity);

  std::string message = str;
  if (severity == logging::LOG_FATAL) {
    if (DebugUtil::BeingDebugged()) {
      DebugUtil::BreakDebugger();
    } else {
      StackTrace trace;
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

  ap_log_perror(APLOG_MARK, log_level, APR_SUCCESS, log_pool,
                "log_message_handler: %s", message.c_str());

  if (severity == logging::LOG_FATAL) {
    // Crash the process to generate a dump.
    DebugUtil::BreakDebugger();
  }

  return true;
}

}  // namespace


namespace net_instaweb {

void InstallLogMessageHandler(apr_pool_t* pool) {
  log_pool = pool;
  logging::SetLogMessageHandler(&LogMessageHandler);
}

}  // namespace net_instaweb
