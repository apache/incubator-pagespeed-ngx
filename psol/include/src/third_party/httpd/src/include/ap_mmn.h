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
 * @file  ap_mmn.h
 * @brief Module Magic Number
 *
 * @defgroup APACHE_CORE_MMN Module Magic Number
 * @ingroup  APACHE_CORE
 * @{
 */

#ifndef APACHE_AP_MMN_H
#define APACHE_AP_MMN_H

/*
 * MODULE_MAGIC_NUMBER_MAJOR
 * Major API changes that could cause compatibility problems for older modules
 * such as structure size changes.  No binary compatibility is possible across
 * a change in the major version.
 *
 * MODULE_MAGIC_NUMBER_MINOR
 * Minor API changes that do not cause binary compatibility problems.
 * Should be reset to 0 when upgrading MODULE_MAGIC_NUMBER_MAJOR.
 *
 * See the AP_MODULE_MAGIC_AT_LEAST macro below for an example.
 */

/*
 * 20010224 (2.0.13-dev) MODULE_MAGIC_COOKIE reset to "AP20"
 * 20010523 (2.0.19-dev) bump for scoreboard structure reordering
 * 20010627 (2.0.19-dev) more API changes than I can count
 * 20010726 (2.0.22-dev) more big API changes
 * 20010808 (2.0.23-dev) dir d_is_absolute bit introduced, bucket changes, etc
 * 20010825 (2.0.25-dev) removed d_is_absolute, introduced map_to_storage hook
 * 20011002 (2.0.26-dev) removed 1.3-depreciated request_rec.content_language
 * 20011127 (2.0.29-dev) bump for postconfig hook change, and removal of socket
 *                       from connection record
 * 20011212 (2.0.30-dev) bump for new used_path_info member of request_rec
 * 20011218 (2.0.30-dev) bump for new sbh member of conn_rec, different 
 *                       declarations for scoreboard, new parameter to
 *                       create_connection hook
 * 20020102 (2.0.30-dev) bump for changed type of limit_req_body in 
 *                       core_dir_config
 * 20020109 (2.0.31-dev) bump for changed shm and scoreboard declarations
 * 20020111 (2.0.31-dev) bump for ETag fields added at end of cor_dir_config
 * 20020114 (2.0.31-dev) mod_dav changed how it asks its provider to fulfill
 *                       a GET request
 * 20020118 (2.0.31-dev) Input filtering split of blocking and mode
 * 20020127 (2.0.31-dev) bump for pre_mpm hook change
 * 20020128 (2.0.31-dev) bump for pre_config hook change
 * 20020218 (2.0.33-dev) bump for AddOutputFilterByType directive
 * 20020220 (2.0.33-dev) bump for scoreboard.h structure change
 * 20020302 (2.0.33-dev) bump for protocol_filter additions.
 * 20020306 (2.0.34-dev) bump for filter type renames.
 * 20020318 (2.0.34-dev) mod_dav's API for REPORT generation changed
 * 20020319 (2.0.34-dev) M_INVALID changed, plus new M_* methods for RFC 3253
 * 20020327 (2.0.35-dev) Add parameter to quick_handler hook
 * 20020329 (2.0.35-dev) bump for addition of freelists to bucket API
 * 20020329.1 (2.0.36) minor bump for new arg to opt fn ap_cgi_build_command
 * 20020506 (2.0.37-dev) Removed r->boundary in request_rec.
 * 20020529 (2.0.37-dev) Standardized the names of some apr_pool_*_set funcs
 * 20020602 (2.0.37-dev) Bucket API change (metadata buckets)
 * 20020612 (2.0.38-dev) Changed server_rec->[keep_alive_]timeout to apr time
 * 20020625 (2.0.40-dev) Changed conn_rec->keepalive to an enumeration
 * 20020628 (2.0.40-dev) Added filter_init to filter registration functions
 * 20020903 (2.0.41-dev) APR's error constants changed
 * 20020903.1 (2.1.0-dev) allow_encoded_slashes added to core_dir_config
 * 20020903.2 (2.0.46-dev) add ap_escape_logitem
 * 20030213.1 (2.1.0-dev) changed log_writer optional fn's to return previous
 *                        handler
 * 20030821 (2.1.0-dev) bumped mod_include's entire API
 * 20030821.1 (2.1.0-dev) added XHTML doctypes
 * 20030821.2 (2.1.0-dev) added ap_escape_errorlog_item
 * 20030821.3 (2.1.0-dev) added ap_get_server_revision / ap_version_t
 * 20040425 (2.1.0-dev) removed ap_add_named_module API
 *                      changed ap_add_module, ap_add_loaded_module,
 *                      ap_setup_prelinked_modules, ap_process_resource_config
 * 20040425.1 (2.1.0-dev) Added ap_module_symbol_t and ap_prelinked_module_symbols
 * 20050101.0 (2.1.2-dev) Axed misnamed http_method for http_scheme (which it was!)
 * 20050127.0 (2.1.3-dev) renamed regex_t->ap_regex_t, regmatch_t->ap_regmatch_t,
 *                        REG_*->AP_REG_*, removed reg* in place of ap_reg*;
 *                        added ap_regex.h
 * 20050217.0 (2.1.3-dev) Axed find_child_by_pid, mpm_*_completion_context (winnt mpm)
 *                        symbols from the public sector, and decorated real_exit_code
 *                        with ap_ in the win32 os.h.
 * 20050305.0 (2.1.4-dev) added pid and generation fields to worker_score
 * 20050305.1 (2.1.5-dev) added ap_vhost_iterate_given_conn.
 * 20050305.2 (2.1.5-dev) added AP_INIT_TAKE_ARGV.
 * 20050305.3 (2.1.5-dev) added Protocol Framework.
 * 20050701.0 (2.1.7-dev) Bump MODULE_MAGIC_COOKIE to "AP21"!
 * 20050701.1 (2.1.7-dev) trace_enable member added to core server_config
 * 20050708.0 (2.1.7-dev) Bump MODULE_MAGIC_COOKIE to "AP22"!
 * 20050708.1 (2.1.7-dev) add proxy request_status hook (minor)
 * 20051006.0 (2.1.8-dev) NET_TIME filter eliminated
 * 20051115.0 (2.1.10-dev/2.2.0) add use_canonical_phys_port to core_dir_config
 * 20051115.1 (2.2.1)  flush_packets and flush_wait members added to
 *                         proxy_server (minor)
 * 20051115.2 (2.2.2)  added inreslist member to proxy_conn_rec (minor)
 * 20051115.3 (2.2.3)  Added server_scheme member to server_rec (minor)
 * 20051115.4 (2.2.4)  Added ap_get_server_banner() and
 *                         ap_get_server_description() (minor)
 * 20051115.5 (2.2.5)  Added ap_mpm_safe_kill() (minor)
 * 20051115.6 (2.2.7)  Added retry_set to proxy_worker (minor)
 * 20051115.7 (2.2.7)  Added conn_rec::clogging_input_filters (minor)
 * 20051115.8 (2.2.7)  Added flags to proxy_alias (minor)
 * 20051115.9 (2.2.7)  Add ap_send_interim_response API
 * 20051115.10 (2.2.7)  Added ap_mod_status_reqtail (minor)
 * 20051115.11 (2.2.7)  Add *ftp_directory_charset to proxy_dir_conf
 * 20051115.12 (2.2.8)  Add optional function ap_logio_add_bytes_in() to mog_logio
 * 20051115.13 (2.2.9)  Add disablereuse and disablereuse_set
 *                      to proxy_worker struct (minor)
 * 20051115.14 (2.2.9)  Add ap_proxy_ssl_connection_cleanup and
 *                      add *scpool, *r and need_flush to proxy_conn_rec
 *                      structure
 * 20051115.15 (2.2.9)  Add interpolate_env to proxy_dir_conf and
 *                      introduce proxy_req_conf.
 * 20051115.16 (2.2.9)  Add conn_timeout and conn_timeout_set to
 *                      proxy_worker struct.
 * 20051115.17 (2.2.10) Add scolonsep to proxy_balancer
 * 20051115.18 (2.2.10) Add chroot support to unixd_config
 * 20051115.19 (2.2.11) Added ap_timeout_parameter_parse to util.c / httpd.h
 * 20051115.20 (2.2.11) Add ap_proxy_buckets_lifetime_transform to mod_proxy.h
 * 20051115.21 (2.2.11) Export mod_rewrite.h in the public API
 * 20051115.22 (2.2.12) Add ap_escape_html2 API, with additional option
 * 20051115.23 (2.2.12) Add ap_open_piped_log_ex API, with cmdtype option,
 *                      and conditional cmdtype member of piped_log struct
 * 20051115.24 (2.2.15) Add forward member to proxy_conn_rec
 */

#define MODULE_MAGIC_COOKIE 0x41503232UL /* "AP22" */

#ifndef MODULE_MAGIC_NUMBER_MAJOR
#define MODULE_MAGIC_NUMBER_MAJOR 20051115
#endif
#define MODULE_MAGIC_NUMBER_MINOR 24                    /* 0...n */

/**
 * Determine if the server's current MODULE_MAGIC_NUMBER is at least a
 * specified value.
 * <pre>
 * Useful for testing for features.
 * For example, suppose you wish to use the apr_table_overlap
 *    function.  You can do this:
 * 
 * #if AP_MODULE_MAGIC_AT_LEAST(19980812,2)
 *     ... use apr_table_overlap()
 * #else
 *     ... alternative code which doesn't use apr_table_overlap()
 * #endif
 * </pre>
 * @param major The major module magic number
 * @param minor The minor module magic number
 * @deffunc AP_MODULE_MAGIC_AT_LEAST(int major, int minor)
 */
#define AP_MODULE_MAGIC_AT_LEAST(major,minor)		\
    ((major) < MODULE_MAGIC_NUMBER_MAJOR 		\
	|| ((major) == MODULE_MAGIC_NUMBER_MAJOR 	\
	    && (minor) <= MODULE_MAGIC_NUMBER_MINOR))

/** @deprecated present for backwards compatibility */
#define MODULE_MAGIC_NUMBER MODULE_MAGIC_NUMBER_MAJOR
#define MODULE_MAGIC_AT_LEAST old_broken_macro_we_hope_you_are_not_using

#endif /* !APACHE_AP_MMN_H */
/** @} */
