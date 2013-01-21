// Copyright 2012 Google Inc.
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

#ifndef MOD_SPDY_APACHE_SLAVE_CONNECTION_CONTEXT_H_
#define MOD_SPDY_APACHE_SLAVE_CONNECTION_CONTEXT_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

struct ap_filter_rec_t;

namespace mod_spdy {

class SpdyStream;

// Context for a 'slave' connection in mod_spdy, used to represent a fetch
// of a given URL from within Apache (as opposed to outgoing SPDY session to
// the client, which has a ConnectionContext).
class SlaveConnectionContext {
 public:
  SlaveConnectionContext();
  ~SlaveConnectionContext();

  // Return true if the connection to the user is over SSL.  This is almost
  // always true, but may be false if we've been set to use SPDY for non-SSL
  // connections (for debugging).  Note that for a slave connection, this
  // refers to whether the master network connection is using SSL.
  bool is_using_ssl() const { return using_ssl_; }
  void set_is_using_ssl(bool ssl_on) { using_ssl_ = ssl_on; }

  // Return the SpdyStream object associated with this slave connection.
  // Note that this may be NULL in case mod_spdy is acting on behalf of
  // another module. Not owned.
  SpdyStream* slave_stream() const { return slave_stream_; }
  void set_slave_stream(SpdyStream* stream) { slave_stream_ = stream; }

  // Return the SPDY version number we will be using, or 0 if we're not using
  // SPDY.
  int spdy_version() const { return spdy_version_; }
  void set_spdy_version(int version) { spdy_version_ = version; }

  // See SlaveConnection documentation for description of these.
  void SetOutputFilter(ap_filter_rec_t* handle, void* context);
  void SetInputFilter(ap_filter_rec_t* handle, void* context);

  ap_filter_rec_t* output_filter_handle() const {
    return output_filter_handle_;
  }

  void* output_filter_context() const {
    return output_filter_context_;
  }

  ap_filter_rec_t* input_filter_handle() const {
    return input_filter_handle_;
  }

  void* input_filter_context() const {
    return input_filter_context_;
  }

 private:
  // These are used to properly inform modules running on slave connections
  // on whether the connection should be treated as using SPDY and SSL.
  bool using_ssl_;
  int spdy_version_;

  // Used for SPDY push.
  SpdyStream* slave_stream_;

  // Filters to attach. These are set by clients of SlaveConnection
  // between creation and Run(), and read by mod_spdy's PreConnection hook,
  // where they are installed in Apache's filter chains.
  ap_filter_rec_t* output_filter_handle_;
  void* output_filter_context_;

  ap_filter_rec_t* input_filter_handle_;
  void* input_filter_context_;

  DISALLOW_COPY_AND_ASSIGN(SlaveConnectionContext);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_APACHE_SLAVE_CONNECTION_CONTEXT_H_
