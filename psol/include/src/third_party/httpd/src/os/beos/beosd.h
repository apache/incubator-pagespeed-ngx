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
 * @file  beosd.h
 * @brief common stuff that beos MPMs will want 
 *
 * @addtogroup APACHE_OS_BEOS
 * @{
 */

#ifndef BEOSD_H
#define BEOSD_H

#include "httpd.h"
#include "ap_listen.h"

/* Default user name and group name. These may be specified as numbers by
 * placing a # before a number */

#ifndef DEFAULT_USER
#define DEFAULT_USER "#-1"
#endif
#ifndef DEFAULT_GROUP
#define DEFAULT_GROUP "#"
#endif

typedef struct {
    char *user_name;
    uid_t user_id;
    gid_t group_id;
} beosd_config_rec;
extern beosd_config_rec beosd_config;

void beosd_detach(void);
int beosd_setup_child(void);
void beosd_pre_config(void);
AP_DECLARE(const char *) beosd_set_user (cmd_parms *cmd, void *dummy, 
                                         const char *arg);
AP_DECLARE(const char *) beosd_set_group(cmd_parms *cmd, void *dummy, 
                                         const char *arg);
AP_DECLARE(apr_status_t) beosd_accept(void **accepted, ap_listen_rec *lr,
                                      apr_pool_t *ptrans);

#define beosd_killpg(x, y)	(kill (-(x), (y)))
#define ap_os_killpg(x, y)      (kill (-(x), (y)))

#define BEOS_DAEMON_COMMANDS	\
AP_INIT_TAKE1("User", beosd_set_user, NULL, RSRC_CONF, \
  "Effective user id for this server (NO-OP)"), \
AP_INIT_TAKE1("Group", beosd_set_group, NULL, RSRC_CONF, \
  "Effective group id for this server (NO-OP)")

#endif /* BEOSD_H */
/** @} */
