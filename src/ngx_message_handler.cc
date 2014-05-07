// Copyright 2013 Google Inc.
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

// Author: oschaaf@gmail.com (Otto van der Schaaf)

#include "ngx_message_handler.h"

#include <signal.h>

#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/debug.h"
#include "net/instaweb/util/public/shared_circular_buffer.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/public/version.h"
#include "pagespeed/kernel/base/posix_timer.h"
#include "pagespeed/kernel/base/time_util.h"

namespace {

// This will be prefixed to every logged message.
const char kModuleName[] = "ngx_pagespeed";

// If set, the crash handler will use this to output a backtrace using
// ngx_log_error.
ngx_log_t* global_log = NULL;

}  // namespace

extern "C" {
  static void signal_handler(int sig) {
    // Try to output the backtrace to the log file. Since this may end up
    // crashing/deadlocking/etc. we set an alarm() to abort us if it comes to
    // that.
    alarm(2);
    if (global_log != NULL) {
      ngx_log_error(NGX_LOG_ALERT, global_log, 0, "Trapped signal [%d]\n%s",
                    sig, net_instaweb::StackTraceString().c_str());
    } else {
      fprintf(stderr, "Trapped signal [%d]\n%s\n",
              sig, net_instaweb::StackTraceString().c_str());
    }
    kill(getpid(), SIGKILL);
  }
}  // extern "C"

namespace net_instaweb {

NgxMessageHandler::NgxMessageHandler(AbstractMutex* mutex)
    : mutex_(mutex),
      buffer_(NULL),
      log_(NULL) {
  SetPidString(static_cast<int64>(getpid()));
}

// Installs a signal handler for common crash signals, that tries to print
// out a backtrace.
void NgxMessageHandler::InstallCrashHandler(ngx_log_t* log) {
  global_log = log;
  signal(SIGTRAP, signal_handler);  // On check failures
  signal(SIGABRT, signal_handler);
  signal(SIGFPE, signal_handler);
  signal(SIGSEGV, signal_handler);
}

bool NgxMessageHandler::Dump(Writer* writer) {
  // Can't dump before SharedCircularBuffer is set up.
  if (buffer_ == NULL) {
    return false;
  }
  return buffer_->Dump(writer, &handler_);
}

ngx_uint_t NgxMessageHandler::GetNgxLogLevel(MessageType type) {
  switch (type) {
    case kInfo:
      return NGX_LOG_INFO;
    case kWarning:
      return NGX_LOG_WARN;
    case kError:
      return NGX_LOG_ERR;
    case kFatal:
      return NGX_LOG_ALERT;
  }

  // This should never fall through, but some compilers seem to complain if
  // we don't include this.
  return NGX_LOG_ALERT;
}

void NgxMessageHandler::set_buffer(SharedCircularBuffer* buff) {
  ScopedMutex lock(mutex_.get());
  buffer_ = buff;
}

void NgxMessageHandler::MessageVImpl(MessageType type, const char* msg,
                                     va_list args) {
  ngx_uint_t log_level = GetNgxLogLevel(type);
  GoogleString formatted_message = Format(msg, args);
  if (log_ != NULL) {
    ngx_log_error(log_level, log_, 0/*ngx_err_t*/, "[%s %s] %s",
                  kModuleName, kModPagespeedVersion, formatted_message.c_str());
  } else {
    GoogleMessageHandler::MessageVImpl(type, msg, args);
  }

  // Prepare a log message for the SharedCircularBuffer only.
  // Prepend time and severity to message.
  // Format is [time] [severity] [pid] message.
  GoogleString message;
  GoogleString time;
  PosixTimer timer;
  if (!ConvertTimeToString(timer.NowMs(), &time)) {
    time = "?";
  }
  StrAppend(&message, "[", time, "] ",
            "[", MessageTypeToString(type), "] ");
  StrAppend(&message, pid_string_, " ", formatted_message, "\n");
  {
    ScopedMutex lock(mutex_.get());
    if (buffer_ != NULL) {
      buffer_->Write(message);
    }
  }
}

void NgxMessageHandler::FileMessageVImpl(MessageType type, const char* file,
                                         int line, const char* msg,
                                         va_list args) {
  ngx_uint_t log_level = GetNgxLogLevel(type);
  GoogleString formatted_message = Format(msg, args);
  if (log_ != NULL) {
    ngx_log_error(log_level, log_, 0/*ngx_err_t*/, "[%s %s] %s:%d:%s",
                  kModuleName, kModPagespeedVersion, file, line,
                  formatted_message.c_str());
  } else {
    GoogleMessageHandler::FileMessageVImpl(type, file, line, msg, args);
  }
}

}  // namespace net_instaweb
