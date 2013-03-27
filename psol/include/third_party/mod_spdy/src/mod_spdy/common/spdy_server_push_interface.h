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

#ifndef MOD_SPDY_COMMON_SPDY_SERVER_PUSH_INTERFACE_H_
#define MOD_SPDY_COMMON_SPDY_SERVER_PUSH_INTERFACE_H_

#include "base/basictypes.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"

namespace mod_spdy {

class SpdyServerPushInterface {
 public:
  SpdyServerPushInterface();
  virtual ~SpdyServerPushInterface();

  enum PushStatus {
    // PUSH_STARTED: The server push was started successfully.
    PUSH_STARTED,
    // INVALID_REQUEST_HEADERS: The given request headers were invalid for a
    // server push (e.g. because required headers were missing).
    INVALID_REQUEST_HEADERS,
    // ASSOCIATED_STREAM_INACTIVE: The push could not be started because the
    // associated stream is not currently active.
    ASSOCIATED_STREAM_INACTIVE,
    // CANNOT_PUSH_EVER_AGAIN: We can't do any more pushes on this session,
    // either because the client has already sent us a GOAWAY frame, or the
    // session has been open so long that we've run out of stream IDs.
    CANNOT_PUSH_EVER_AGAIN,
    // TOO_MANY_CONCURRENT_PUSHES: The push could not be started right now
    // because there are too many currently active push streams.
    TOO_MANY_CONCURRENT_PUSHES,
    // PUSH_INTERNAL_ERROR: There was an internal error in the SpdySession
    // (typically something that caused a LOG(DFATAL).
    PUSH_INTERNAL_ERROR,
  };

  // Initiate a SPDY server push, roughly by pretending that the client sent a
  // SYN_STREAM with the given headers.  To repeat: the headers argument is
  // _not_ the headers that the server will send to the client, but rather the
  // headers to _pretend_ that the client sent to the server.
  virtual PushStatus StartServerPush(
      net::SpdyStreamId associated_stream_id,
      int32 server_push_depth,
      net::SpdyPriority priority,
      const net::SpdyHeaderBlock& request_headers) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdyServerPushInterface);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_COMMON_SPDY_SERVER_PUSH_INTERFACE_H_
