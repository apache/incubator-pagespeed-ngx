// Copyright 2011 Google Inc.
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

#ifndef MOD_SPDY_COMMON_SPDY_SESSION_IO_H_
#define MOD_SPDY_COMMON_SPDY_SESSION_IO_H_

#include "base/basictypes.h"

namespace net {
class SpdyFrame;
class BufferedSpdyFramer;
}  // namespace net

namespace mod_spdy {

class SpdyStream;

// SpdySessionIO is a helper interface for the SpdySession class.  The
// SpdySessionIO takes care of implementation-specific details about how to
// send and receive data, allowing the SpdySession to focus on the SPDY
// protocol itself.  For example, a SpdySessionIO for Apache would hold onto a
// conn_rec object and invoke the input and output filter chains for
// ProcessAvailableInput and SendFrameRaw, respectively.  The SpdySessionIO
// itself does not need to be thread-safe -- it is only ever used by the main
// connection thread.
class SpdySessionIO {
 public:
  // Status to describe whether reading succeeded.
  enum ReadStatus {
    READ_SUCCESS,  // we successfully pushed data into the SpdyFramer
    READ_NO_DATA,  // no data is currently available
    READ_CONNECTION_CLOSED,  // the connection has been closed
    READ_ERROR  // an unrecoverable error (e.g. client sent malformed data)
  };

  // Status to describe whether writing succeeded.
  enum WriteStatus {
    WRITE_SUCCESS,  // we successfully wrote the frame out to the network
    WRITE_CONNECTION_CLOSED,  // the connection has been closed
  };

  SpdySessionIO();
  virtual ~SpdySessionIO();

  // Return true if the connection has been externally aborted and should
  // stop, false otherwise.
  virtual bool IsConnectionAborted() = 0;

  // Pull any available input data from the connection and feed it into the
  // ProcessInput() method of the given SpdyFramer.  If no input data is
  // currently available and the block argument is true, this should block
  // until more data arrives; otherwise, this should not block.
  virtual ReadStatus ProcessAvailableInput(
      bool block, net::BufferedSpdyFramer* framer) = 0;

  // Send a single SPDY frame to the client as-is; block until it has been
  // sent down the wire.  Return true on success.
  //
  // TODO(mdsteele): We do need to be able to flush a single frame down the
  //   wire, but we probably don't need/want to flush every single frame
  //   individually in places where we send multiple frames at once.  We'll
  //   probably want to adjust this API a bit.
  virtual WriteStatus SendFrameRaw(const net::SpdyFrame& frame) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdySessionIO);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_COMMON_SPDY_SESSION_IO_H_
