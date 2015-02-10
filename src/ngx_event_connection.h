/*
 * Copyright 2014 Google Inc.
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

// Author: oschaaf@we-amp.com (Otto van der Schaaf)
//
// NgxEventConnection implements a means to send events from other threads to
// nginx's event loop, and is implemented by a named pipe under the hood.
// A single instance is used by NgxBaseFetch, and one instance is created per
// NgxUrlAsyncFetcher when native fetching is on.

#ifndef NGX_EVENT_CONNECTION_H_
#define NGX_EVENT_CONNECTION_H_

extern "C" {
#include <ngx_http.h>
}

#include <pthread.h>

#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/http/headers.h"

namespace net_instaweb {

class NgxEventConnection;

// Represents a single event that can be written to or read from the pipe.
// Technically, sender is the only data we need to send. type and connection are
// included to provide a means to trace the events along with some more
// info.
typedef struct {
  char type;
  void* sender;
  NgxEventConnection* connection;
} ps_event_data;

// Handler signature for receiving events
typedef void (*callbackPtr)(const ps_event_data&);

// Abstracts a connection to nginx through which events can be written.
class NgxEventConnection {
 public:
  explicit NgxEventConnection(callbackPtr handler);

  // Creates the file descriptors and ngx_connection_t required for event
  // messaging between pagespeed and nginx.
  bool Init(ngx_cycle_t* cycle);
  // Shuts down the underlying file descriptors and connection created in Init()
  void Shutdown();
  // Constructs a ps_event_data and writes it to the underlying named pipe.
  bool WriteEvent(char type, void* sender);
  // Convenience overload for clients that have a single event type.
  bool WriteEvent(void* sender);
  // Reads and processes what is available in the named pipe's buffer.
  void Drain();
 private:
  static bool CreateNgxConnection(ngx_cycle_t* cycle, ngx_fd_t pipe_fd);
  static void ReadEventHandler(ngx_event_t* e);
  static bool ReadAndNotify(ngx_fd_t fd);

  callbackPtr event_handler_;
  // We own these file descriptors
  ngx_fd_t pipe_write_fd_;
  ngx_fd_t pipe_read_fd_;

  DISALLOW_COPY_AND_ASSIGN(NgxEventConnection);
};

}  // namespace net_instaweb

#endif  // NGX_EVENT_CONNECTION_H_
