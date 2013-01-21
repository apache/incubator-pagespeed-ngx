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

#ifndef MOD_SPDY_APACHE_MASTER_CONNECTION_CONTEXT_H_
#define MOD_SPDY_APACHE_MASTER_CONNECTION_CONTEXT_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace mod_spdy {

class SpdyStream;

// Shared context object for a SPDY connection to the outside world.
class MasterConnectionContext {
 public:
  // Create a context object for a master connection (one to the outside world,
  // not for talking to Apache).
  explicit MasterConnectionContext(bool using_ssl);
  ~MasterConnectionContext();

  // Return true if the connection to the user is over SSL.  This is almost
  // always true, but may be false if we've been set to use SPDY for non-SSL
  // connections (for debugging).
  bool is_using_ssl() const { return using_ssl_; }

  // Return true if we are using SPDY for this connection, which is the case if
  // either 1) SPDY was chosen by NPN, or 2) we are assuming SPDY regardless of
  // NPN.
  bool is_using_spdy() const;

  enum NpnState {
    // NOT_DONE_YET: NPN has not yet completed.
    NOT_DONE_YET,
    // USING_SPDY: We have agreed with the client to use SPDY for this
    // connection.
    USING_SPDY,
    // NOT_USING_SPDY: We have decided not to use SPDY for this connection.
    NOT_USING_SPDY
  };

  // Get the NPN state of this connection.  Unless you actually care about NPN
  // itself, you probably don't want to use this method to check if SPDY is
  // being used; instead, use is_using_spdy().
  NpnState npn_state() const;

  // Set the NPN state of this connection.
  void set_npn_state(NpnState state);

  // If true, we are simply _assuming_ SPDY, regardless of the outcome of NPN.
  bool is_assuming_spdy() const;

  // Set whether we are assuming SPDY for this connection (regardless of NPN).
  void set_assume_spdy(bool assume);

  // Return the SPDY version number we will be using.  Requires that
  // is_using_spdy() is true and that the version number has already been set.
  int spdy_version() const;

  // Set the SPDY version number we will be using.  Requires that
  // is_using_spdy() is true, and set_spdy_version hasn't already been called.
  void set_spdy_version(int spdy_version);

 private:
  const bool using_ssl_;
  NpnState npn_state_;
  bool assume_spdy_;
  int spdy_version_;

  DISALLOW_COPY_AND_ASSIGN(MasterConnectionContext);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_APACHE_MASTER_CONNECTION_CONTEXT_H_
