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

#ifndef LOG_MESSAGE_HANDLER_H_
#define LOG_MESSAGE_HANDLER_H_

extern "C" {
  #include <ngx_config.h>
  #include <ngx_core.h>
  #include <ngx_http.h>
  #include <ngx_log.h>
}

namespace net_instaweb {

namespace log_message_handler {

// Install a log message handler that routes LOG() messages to the
// server error log.  Should be called once at startup.  If server blocks define
// their own logging files you would expect that LOG() messages would be routed
// appropriately, but because logging is all done with global variables this
// isn't possible.
void Install(ngx_log_t* log_in);

}  // namespace log_message_handler

}  // namespace net_instaweb

#endif  // LOG_MESSAGE_HANDLER_H_
