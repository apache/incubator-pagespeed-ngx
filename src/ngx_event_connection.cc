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

extern "C" {

#include <ngx_channel.h>

}

#include "ngx_event_connection.h"

#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/message_handler.h"

namespace net_instaweb {

  NgxEventConnection::NgxEventConnection(callbackPtr callback)
    : event_handler_(callback) {
}

bool NgxEventConnection::Init(ngx_cycle_t* cycle) {
  int file_descriptors[2];

  if (pipe(file_descriptors) != 0) {
    ngx_log_error(NGX_LOG_EMERG, cycle->log, 0, "pagespeed: pipe() failed");
    return false;
  }
  if (ngx_nonblocking(file_descriptors[0]) == -1) {
    ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_socket_errno,
                  ngx_nonblocking_n "pagespeed:  pipe[0] failed");
  } else if (ngx_nonblocking(file_descriptors[1]) == -1) {
    ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_socket_errno,
                  ngx_nonblocking_n "pagespeed:  pipe[1] failed");
  } else if (!CreateNgxConnection(cycle, file_descriptors[0])) {
    ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                  "pagespeed: failed to create connection.");
  } else {
    pipe_read_fd_ = file_descriptors[0];
    pipe_write_fd_ = file_descriptors[1];
    // Attempt to bump the pipe capacity, because running out of buffer space
    // can potentially lead up to writes spinning on EAGAIN.
    // See https://github.com/pagespeed/ngx_pagespeed/issues/1380
    // TODO(oschaaf): Consider implementing a queueing mechanism for retrying
    // failed writes.
#ifdef F_SETPIPE_SZ
    fcntl(pipe_write_fd_, F_SETPIPE_SZ, 200*1024 /* minimal amount of bytes */);
#endif
    return true;
  }
  close(file_descriptors[0]);
  close(file_descriptors[1]);
  return false;
}

bool NgxEventConnection::CreateNgxConnection(ngx_cycle_t* cycle,
                                             ngx_fd_t pipe_fd) {
  // pipe_fd (the read side of the pipe will end up as c->fd on the
  // underlying ngx_connection_t that gets created here)
  ngx_int_t rc = ngx_add_channel_event(cycle, pipe_fd, NGX_READ_EVENT,
      &NgxEventConnection::ReadEventHandler);
  return rc  == NGX_OK;
}

void NgxEventConnection::ReadEventHandler(ngx_event_t* ev) {
  ngx_connection_t* c = static_cast<ngx_connection_t*>(ev->data);
  ngx_int_t result = ngx_handle_read_event(ev, 0);
  if (result != NGX_OK) {
    CHECK(false) << "pagespeed: ngx_handle_read_event error: " << result;
  }

  if (ev->timedout) {
    ev->timedout = 0;
    return;
  }

  if (!NgxEventConnection::ReadAndNotify(c->fd)) {
    // This was copied from ngx_channel_handler(): for epoll, we need to call
    // ngx_del_conn(). Sadly, no documentation as to why.
    if (ngx_event_flags & NGX_USE_EPOLL_EVENT) {
      ngx_del_conn(c, 0);
    }
    ngx_close_connection(c);
    ngx_del_event(ev, NGX_READ_EVENT, 0);
  }
}

// Deserialize ps_event_data's from the pipe as they become available.
// Subsequently do some bookkeeping, cleanup, and error checking to keep
// the mess out of ps_base_fetch_handler.
bool NgxEventConnection::ReadAndNotify(ngx_fd_t fd) {
  while (true) {
    // We read only one ps_event_data at a time for now:
    // We can end up recursing all the way and end up calling ourselves here.
    // If that happens in the middle of looping over multiple ps_event_data's we
    // have obtained with read(), the results from the next read() will make us
    // process events out of order. Which can give headaches.
    // Alternatively, we could maintain a queue to make sure we process in
    // sequence
    ps_event_data data;
    ngx_int_t size = read(fd, static_cast<void*>(&data), sizeof(data));

    if (size == -1) {
      if (errno == EINTR) {
        continue;
      // TODO(oschaaf): should we worry about spinning here?
      } else if (ngx_errno == EAGAIN || ngx_errno == EWOULDBLOCK) {
        return true;
      }
    }

    if (size <= 0) {
      return false;
    }

    data.connection->event_handler_(data);
    return true;
  }
}

bool NgxEventConnection::WriteEvent(void* sender) {
  return WriteEvent('X' /* Anything char is fine */, sender);
}

bool NgxEventConnection::WriteEvent(char type, void* sender) {
  ssize_t size = 0;
  ps_event_data data;

  ngx_memzero(&data, sizeof(data));
  data.type = type;
  data.sender = sender;
  data.connection = this;

  while (true) {
    size = write(pipe_write_fd_,
                 static_cast<void*>(&data), sizeof(data));
    if (size == sizeof(data)) {
      return true;
    } else if (size == -1) {
      // TODO(oschaaf): should we worry about spinning here?
      if (ngx_errno == EINTR || ngx_errno == EAGAIN
          || ngx_errno == EWOULDBLOCK) {
        continue;
      } else {
        return false;
      }
    } else {
      CHECK(false) << "pagespeed: unexpected return value from write(): "
                   << size;
    }
  }
  CHECK(false) << "Should not get here";
  return false;
}

// Reads and processes what is available in the pipe.
void NgxEventConnection::Drain() {
  NgxEventConnection::ReadAndNotify(pipe_read_fd_);
}

void NgxEventConnection::Shutdown() {
  close(pipe_write_fd_);
  close(pipe_read_fd_);
}

}  // namespace net_instaweb
