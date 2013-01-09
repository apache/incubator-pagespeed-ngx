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

#ifndef MOD_SPDY_APACHE_APACHE_SPDY_SESSION_IO_H_
#define MOD_SPDY_APACHE_APACHE_SPDY_SESSION_IO_H_

#include "httpd.h"

#include "base/basictypes.h"
#include "mod_spdy/common/spdy_session_io.h"

namespace net {
class BufferedSpdyFramer;
class SpdyFrame;
}  // namespace net

namespace mod_spdy {

class ApacheSpdySessionIO : public SpdySessionIO {
 public:
  explicit ApacheSpdySessionIO(conn_rec* connection);
  ~ApacheSpdySessionIO();

  // SpdySessionIO methods:
  virtual bool IsConnectionAborted();
  virtual ReadStatus ProcessAvailableInput(bool block,
                                           net::BufferedSpdyFramer* framer);
  virtual WriteStatus SendFrameRaw(const net::SpdyFrame& frame);

 private:
  conn_rec* const connection_;
  apr_bucket_brigade* const input_brigade_;
  apr_bucket_brigade* const output_brigade_;

  DISALLOW_COPY_AND_ASSIGN(ApacheSpdySessionIO);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_APACHE_APACHE_SPDY_SESSION_IO_H_
