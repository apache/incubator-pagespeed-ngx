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
 * @file  util_script.h
 * @brief Apache script tools
 *
 * @defgroup APACHE_CORE_SCRIPT Script Tools
 * @ingroup  APACHE_CORE
 * @{
 */

#ifndef APACHE_UTIL_SCRIPT_H
#define APACHE_UTIL_SCRIPT_H

#include "apr_buckets.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef APACHE_ARG_MAX
#ifdef _POSIX_ARG_MAX
#define APACHE_ARG_MAX _POSIX_ARG_MAX
#else
#define APACHE_ARG_MAX 512
#endif
#endif

/**
 * Create an environment variable out of an Apache table of key-value pairs
 * @param p pool to allocate out of
 * @param t Apache table of key-value pairs
 * @return An array containing the same key-value pairs suitable for
 *         use with an exec call.
 * @deffunc char **ap_create_environment(apr_pool_t *p, apr_table_t *t)
 */
AP_DECLARE(char **) ap_create_environment(apr_pool_t *p, apr_table_t *t);

/**
 * This "cute" little function comes about because the path info on
 * filenames and URLs aren't always the same. So we take the two,
 * and find as much of the two that match as possible.
 * @param uri The uri we are currently parsing
 * @param path_info The current path info
 * @return The length of the path info
 * @deffunc int ap_find_path_info(const char *uri, const char *path_info)
 */
AP_DECLARE(int) ap_find_path_info(const char *uri, const char *path_info);

/**
 * Add CGI environment variables required by HTTP/1.1 to the request's 
 * environment table
 * @param r the current request
 * @deffunc void ap_add_cgi_vars(request_rec *r)
 */
AP_DECLARE(void) ap_add_cgi_vars(request_rec *r);

/**
 * Add common CGI environment variables to the requests environment table
 * @param r The current request
 * @deffunc void ap_add_common_vars(request_rec *r)
 */
AP_DECLARE(void) ap_add_common_vars(request_rec *r);

/**
 * Read headers output from a script, ensuring that the output is valid.  If
 * the output is valid, then the headers are added to the headers out of the
 * current request
 * @param r The current request
 * @param f The file to read from
 * @param buffer Empty when calling the function.  On output, if there was an
 *               error, the string that cause the error is stored here. 
 * @return HTTP_OK on success, HTTP_INTERNAL_SERVER_ERROR otherwise
 * @deffunc int ap_scan_script_header_err(request_rec *r, apr_file_t *f, char *buffer)
 */ 
AP_DECLARE(int) ap_scan_script_header_err(request_rec *r, apr_file_t *f, char *buffer);

/**
 * Read headers output from a script, ensuring that the output is valid.  If
 * the output is valid, then the headers are added to the headers out of the
 * current request
 * @param r The current request
 * @param bb The brigade from which to read
 * @param buffer Empty when calling the function.  On output, if there was an
 *               error, the string that cause the error is stored here. 
 * @return HTTP_OK on success, HTTP_INTERNAL_SERVER_ERROR otherwise
 * @deffunc int ap_scan_script_header_err_brigade(request_rec *r, apr_bucket_brigade *bb, char *buffer)
 */ 
AP_DECLARE(int) ap_scan_script_header_err_brigade(request_rec *r,
                                                  apr_bucket_brigade *bb,
                                                  char *buffer);

/**
 * Read headers strings from a script, ensuring that the output is valid.  If
 * the output is valid, then the headers are added to the headers out of the
 * current request
 * @param r The current request
 * @param buffer Empty when calling the function.  On output, if there was an
 *               error, the string that cause the error is stored here. 
 * @param termch Pointer to the last character parsed.
 * @param termarg Pointer to an int to capture the last argument parsed.
 * @param args   String arguments to parse consecutively for headers, 
 *               a NULL argument terminates the list.
 * @return HTTP_OK on success, HTTP_INTERNAL_SERVER_ERROR otherwise
 * @deffunc int ap_scan_script_header_err_core(request_rec *r, char *buffer, int (*getsfunc)(char *, int, void *), void *getsfunc_data)
 */ 
AP_DECLARE_NONSTD(int) ap_scan_script_header_err_strs(request_rec *r, 
                                                      char *buffer, 
                                                      const char **termch,
                                                      int *termarg, ...);

/**
 * Read headers output from a script, ensuring that the output is valid.  If
 * the output is valid, then the headers are added to the headers out of the
 * current request
 * @param r The current request
 * @param buffer Empty when calling the function.  On output, if there was an
 *               error, the string that cause the error is stored here. 
 * @param getsfunc Function to read the headers from.  This function should
                   act like gets()
 * @param getsfunc_data The place to read from
 * @return HTTP_OK on success, HTTP_INTERNAL_SERVER_ERROR otherwise
 * @deffunc int ap_scan_script_header_err_core(request_rec *r, char *buffer, int (*getsfunc)(char *, int, void *), void *getsfunc_data)
 */ 
AP_DECLARE(int) ap_scan_script_header_err_core(request_rec *r, char *buffer,
				       int (*getsfunc) (char *, int, void *),
				       void *getsfunc_data);

#ifdef __cplusplus
}
#endif

#endif	/* !APACHE_UTIL_SCRIPT_H */
/** @} */
