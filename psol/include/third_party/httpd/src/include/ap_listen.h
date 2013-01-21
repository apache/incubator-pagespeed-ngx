/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file  ap_listen.h
 * @brief Apache Listeners Library
 *
 * @defgroup APACHE_CORE_LISTEN Apache Listeners Library
 * @ingroup  APACHE_CORE
 * @{
 */

#ifndef AP_LISTEN_H
#define AP_LISTEN_H

#include "apr_network_io.h"
#include "httpd.h"
#include "http_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ap_listen_rec ap_listen_rec;
typedef apr_status_t (*accept_function)(void **csd, ap_listen_rec *lr, apr_pool_t *ptrans);

/**
 * @brief Apache's listeners record.  
 *
 * These are used in the Multi-Processing Modules
 * to setup all of the sockets for the MPM to listen to and accept on.
 */
struct ap_listen_rec {
    /**
     * The next listener in the list
     */
    ap_listen_rec *next;
    /**
     * The actual socket 
     */
    apr_socket_t *sd;
    /**
     * The sockaddr the socket should bind to
     */
    apr_sockaddr_t *bind_addr;
    /**
     * The accept function for this socket
     */
    accept_function accept_func;
    /**
     * Is this socket currently active 
     */
    int active;
    /**
     * The default protocol for this listening socket.
     */
    const char* protocol;
};

/**
 * The global list of ap_listen_rec structures
 */
AP_DECLARE_DATA extern ap_listen_rec *ap_listeners;

/**
 * Setup all of the defaults for the listener list
 */
AP_DECLARE(void) ap_listen_pre_config(void);

/**
 * Loop through the global ap_listen_rec list and create all of the required
 * sockets.  This executes the listen and bind on the sockets.
 * @param s The global server_rec
 * @return The number of open sockets.
 */ 
AP_DECLARE(int) ap_setup_listeners(server_rec *s);

/**
 * Loop through the global ap_listen_rec list and close each of the sockets.
 */
AP_DECLARE_NONSTD(void) ap_close_listeners(void);

/* Although these functions are exported from libmain, they are not really
 * public functions.  These functions are actually called while parsing the
 * config file, when one of the LISTEN_COMMANDS directives is read.  These
 * should not ever be called by external modules.  ALL MPMs should include
 * LISTEN_COMMANDS in their command_rec table so that these functions are
 * called.
 */ 
AP_DECLARE_NONSTD(const char *) ap_set_listenbacklog(cmd_parms *cmd, void *dummy, const char *arg);
AP_DECLARE_NONSTD(const char *) ap_set_listener(cmd_parms *cmd, void *dummy, 
                                                int argc, char *const argv[]);
AP_DECLARE_NONSTD(const char *) ap_set_send_buffer_size(cmd_parms *cmd, void *dummy,
				    const char *arg);
AP_DECLARE_NONSTD(const char *) ap_set_receive_buffer_size(cmd_parms *cmd,
                                                           void *dummy,
                                                           const char *arg);

#define LISTEN_COMMANDS	\
AP_INIT_TAKE1("ListenBacklog", ap_set_listenbacklog, NULL, RSRC_CONF, \
  "Maximum length of the queue of pending connections, as used by listen(2)"), \
AP_INIT_TAKE_ARGV("Listen", ap_set_listener, NULL, RSRC_CONF, \
  "A port number or a numeric IP address and a port number, and an optional protocol"), \
AP_INIT_TAKE1("SendBufferSize", ap_set_send_buffer_size, NULL, RSRC_CONF, \
  "Send buffer size in bytes"), \
AP_INIT_TAKE1("ReceiveBufferSize", ap_set_receive_buffer_size, NULL, \
              RSRC_CONF, "Receive buffer size in bytes")

#ifdef __cplusplus
}
#endif

#endif
/** @} */
