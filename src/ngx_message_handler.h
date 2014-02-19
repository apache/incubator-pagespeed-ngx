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

#ifndef NGX_MESSAGE_HANDLER_H_
#define NGX_MESSAGE_HANDLER_H_

extern "C" {
  #include <ngx_core.h>
  #include <ngx_log.h>
}

#include <cstdarg>

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

// Implementation of a message handler that uses ngx_log_error()
// logging to emit messages, with a fallback to GoogleMessageHandler
class NgxMessageHandler : public GoogleMessageHandler {
 public:
  explicit NgxMessageHandler(AbstractMutex* mutex);

  // Installs a signal handler for common crash signals that tries to print
  // out a backtrace.
  static void InstallCrashHandler(ngx_log_t* log);

  // When NgxRewriteDriver instantiates the NgxMessageHandlers, the
  // SharedCircularBuffer and ngx_log_t are not available yet. These
  // will later be set in RootInit/Childinit
  // Messages logged before that will be passed on to handler_;
  void set_buffer(SharedCircularBuffer* buff);
  void set_log(ngx_log_t* log) { log_ = log; }
  ngx_log_t* log() { return log_; }

  void SetPidString(const int64 pid) {
    pid_string_ = StrCat("[", Integer64ToString(pid), "]");
  }
  // Dump contents of SharedCircularBuffer.
  bool Dump(Writer* writer);

 protected:
  virtual void MessageVImpl(MessageType type, const char* msg, va_list args);

  virtual void FileMessageVImpl(MessageType type, const char* filename,
                                int line, const char* msg, va_list args);

 private:
  ngx_uint_t GetNgxLogLevel(MessageType type);

  scoped_ptr<AbstractMutex> mutex_;
  GoogleString pid_string_;
  // handler_ is used as a fallback when we can not use ngx_log_errort
  // It's also used when calling Dump on the internal SharedCircularBuffer
  GoogleMessageHandler handler_;
  SharedCircularBuffer* buffer_;
  ngx_log_t* log_;

  DISALLOW_COPY_AND_ASSIGN(NgxMessageHandler);
};

}  // namespace net_instaweb

#endif  // NGX_MESSAGE_HANDLER_H_
