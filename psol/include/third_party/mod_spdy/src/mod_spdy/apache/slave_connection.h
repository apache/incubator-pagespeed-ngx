/* Copyright 2012 Google Inc.
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

#ifndef MOD_SPDY_APACHE_SLAVE_CONNECTION_H_
#define MOD_SPDY_APACHE_SLAVE_CONNECTION_H_

#include "base/basictypes.h"
#include "mod_spdy/apache/pool_util.h"

struct apr_sockaddr_t;
struct apr_socket_t;
struct conn_rec;
struct server_rec;

namespace mod_spdy {

class SlaveConnection;
class SlaveConnectionContext;

// SlaveConnectionFactory + SlaveConnection helps execute requests within
// the current Apache process, with the request and response both going to
// some other code and not an external client talking over TCP.
//
// SlaveConnectionFactory + SlaveConnection help create a fake Apache conn_rec
// object and run it. That conn_rec will have a SlaveConnectionContext
// attached to it, which various hooks in mod_spdy.cc will recognize and handle
// specially. In particular, they will arrange to have the I/O for connection
// routed to and from the input & output filters set on the
// SlaveConnectionContext.
class SlaveConnectionFactory {
 public:
  // Prepares the factory to create slave connections with endpoint, SPDY and
  // SSL information matching that of the master_connection.
  //
  // Does not retain any pointers to data from master_connection, so may be
  // used after master_connection is destroyed.
  explicit SlaveConnectionFactory(conn_rec* master_connection);

  ~SlaveConnectionFactory();

  // Creates a slave connection matching the settings in the constructor.
  // You should attach I/O filters on its GetSlaveConnectionContext() before
  // calling Run().
  //
  // The resulted object lives on the C++ heap, and must be deleted.
  SlaveConnection* Create();

 private:
  friend class SlaveConnection;

  // Saved information from master_connection
  bool is_using_ssl_;
  int spdy_version_;
  server_rec* base_server_;
  LocalPool pool_;
  // All of these are in pool_:
  apr_sockaddr_t* local_addr_;
  char* local_ip_;
  apr_sockaddr_t* remote_addr_;
  char* remote_ip_;
  long master_connection_id_;

  DISALLOW_COPY_AND_ASSIGN(SlaveConnectionFactory);
};

class SlaveConnection {
 public:
  ~SlaveConnection();

  // Returns the Apache conn_rec object this manages.
  conn_rec* apache_connection() { return slave_connection_; }

  // Returns the underlying SlaveConnectionContext, which lets you query
  // information about the connection and hook in I/O filters.
  //
  // This is the same as GetSlaveConnectionContext(apache_connection()), and
  // can thus be accessed via the conn_rec* as well.
  SlaveConnectionContext* GetSlaveConnectionContext();

  // Executes the requests associated with this connection, taking a request
  // from the input filter set on the SlaveConnectionContext(), and directing
  // the response to the output filter. Note that this is a blocking operation.
  void Run();

 private:
  SlaveConnection(SlaveConnectionFactory* factory);
  friend class SlaveConnectionFactory;

  LocalPool pool_;
  conn_rec* slave_connection_;  // owned by pool_
  apr_socket_t* slave_socket_;  // owned by pool_
  long master_connection_id_;

  DISALLOW_COPY_AND_ASSIGN(SlaveConnection);
};

}  // namespace mod_spdy

#endif  /* MOD_SPDY_APACHE_SLAVE_CONNECTION_H_ */
