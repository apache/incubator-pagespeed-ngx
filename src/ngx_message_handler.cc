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
#include <unistd.h>

#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/log_message_handler.h"
//#include "net/instaweb/apache/apache_logging_includes.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/debug.h"
#include "net/instaweb/util/public/shared_circular_buffer.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

// This name will be prefixed to every logged message.  This could be made
// smaller if people think it's too long.  In my opinion it's probably OK,
// and it would be good to let people know where messages are coming from.
const char kModuleName[] = "ngx_pagespeed";

// For crash handler's use.
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

// filename_prefix of NgxRewriteDriverFactory is needed to initialize
// SharedCircuarBuffer. However, NgxRewriteDriverFactory needs
// NgxMessageHandler before its filename_prefix is set. So we initialize
// NgxMessageHandler without SharedCircularBuffer first, then initalize its
// SharedCircularBuffer in RootInit() when filename_prefix is set.
NgxMessageHandler::NgxMessageHandler(Timer* timer, AbstractMutex* mutex)
    :  timer_(timer),
       // TODO(oschaaf): make sure this mutex is only used pre-fork
       mutex_(mutex),
       buffer_(NULL) {
  // Tell log_message_handler about this server_rec and version.
  // log_message_handler::AddServerConfig(server_rec_, version);
  // Set pid string.
  SetPidString(static_cast<int64>(getpid()));
  // TODO(jmarantz): consider making this a little terser by default.
  // The string we expect in is something like "0.9.1.1-171" and we will
  // may be able to pick off some of the 5 fields that prove to be boring.
}

// Installs a signal handler for common crash signals that tries to print
// out a backtrace.
void NgxMessageHandler::InstallCrashHandler(ngx_log_t* log) {
  global_log = log;
  signal(SIGTRAP, signal_handler);  // On check failures
  signal(SIGABRT, signal_handler);
  signal(SIGFPE, signal_handler);
  signal(SIGSEGV, signal_handler);
  // TODO(oschaaf): for testing only:
  signal(SIGTERM, signal_handler);
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
  if (log_!=NULL) {
    ngx_log_error(log_level, log_, 0/*ngx_err_t*/, "%s:%s",
                  kModuleName, formatted_message.c_str());
  } else {
    GoogleMessageHandler::MessageVImpl(type, msg, args);
  }

  // Can not write to SharedCircularBuffer before it's set up.
  // Prepend time (down to microseconds) and severity to message.
  // Format is [time:microseconds] [severity] [pid] message.
  GoogleString message;
  //char time_buffer[APR_CTIME_LEN + 1];
  // TODO(oschaaf): format time stamp
  const char* time = "?";//time_buffer;
  //apr_status_t status = apr_ctime(time_buffer, apr_time_now());
  //if (status != APR_SUCCESS) {
  //  time = "?";
  // }n
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
    ngx_log_error(log_level, log_, 0/*ngx_err_t*/, "%s:%s",
                  kModuleName,formatted_message.c_str());
  } else {
    GoogleMessageHandler::FileMessageVImpl(type, file, line, msg, args);
  }
}

// TODO(sligocki): It'd be nice not to do so much string copying.
GoogleString NgxMessageHandler::Format(const char* msg, va_list args) {
  GoogleString buffer;

  // Ignore the name of this routine: it formats with vsnprintf.
  // See base/stringprintf.cc.
  StringAppendV(&buffer, msg, args);
  return buffer;
}

}  // namespace net_instaweb
