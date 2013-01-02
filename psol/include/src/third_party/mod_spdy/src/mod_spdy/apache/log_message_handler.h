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

#ifndef MOD_SPDY_APACHE_LOG_MESSAGE_HANDLER_H_
#define MOD_SPDY_APACHE_LOG_MESSAGE_HANDLER_H_

#include <string>

#include "httpd.h"
#include "apr_pools.h"

#include "base/basictypes.h"

namespace mod_spdy {

class SpdyStream;

// Stack-allocate to install a server-specific log handler for the duration of
// the current scope (on the current thread only).  For example:
//
//   void SomeApacheHookFunction(server_rec* server) {
//     ScopedServerLogHandler handler(server);
//     ...call various other functions here...
//   }
//
// The log handler will be in effect until the end of the block, but only for
// this thread (even if this thread spawns other threads in the meantime).
// Establishing this server-specific log handler allows LOG() macros within
// called functions to produce better messages.
class ScopedServerLogHandler {
 public:
  explicit ScopedServerLogHandler(server_rec* server);
  ~ScopedServerLogHandler();
 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedServerLogHandler);
};

// Stack-allocate to install a connection-specific log handler for the duration
// of the current scope (on the current thread only).  See the doc comment for
// ScopedServerLogHandler above for an example.
class ScopedConnectionLogHandler {
 public:
  explicit ScopedConnectionLogHandler(conn_rec* connection);
  ~ScopedConnectionLogHandler();
 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedConnectionLogHandler);
};

// Stack-allocate to install a stream-specific log handler for the duration of
// the current scope (on the current thread only).  See the doc comment for
// ScopedServerLogHandler above for an example.
class ScopedStreamLogHandler {
 public:
  explicit ScopedStreamLogHandler(conn_rec* slave_connection,
                                  const SpdyStream* stream);
  ~ScopedStreamLogHandler();
 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedStreamLogHandler);
};

// Install a log message handler that routes LOG() messages to the
// apache error log.  Should be called once, at server startup.
void InstallLogMessageHandler(apr_pool_t* pool);

// Set the logging level for LOG() messages, based on the Apache log level and
// the VLOG-level specified in the server config.  Note that the VLOG level
// will be ignored unless the Apache log verbosity is at NOTICE or higher.
// Should be called once for each child process, at process startup.
void SetLoggingLevel(int apache_log_level, int vlog_level);

}  // namespace mod_spdy

#endif  // MOD_SPDY_APACHE_LOG_MESSAGE_HANDLER_H_
