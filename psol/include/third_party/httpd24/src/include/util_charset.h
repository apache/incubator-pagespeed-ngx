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
 * @file  util_charset.h
 * @brief charset conversion
 *
 * @defgroup APACHE_CORE_CHARSET Charset Conversion
 * @ingroup  APACHE_CORE
 * @{
 */

#ifndef APACHE_UTIL_CHARSET_H
#define APACHE_UTIL_CHARSET_H

#ifdef __cplusplus
extern "C" {
#endif

#include "apr.h"

#if APR_CHARSET_EBCDIC

#include "apr_xlate.h"

/** On EBCDIC machine this is a translation handle used to translate the
 *  headers from the local machine format to ASCII for network transmission.
 *  On an ASCII machine this is NULL */
extern apr_xlate_t *ap_hdrs_to_ascii;
/** On EBCDIC machine this is a translation handle used to translate the
 *  headers from ASCII to the local machine format after network transmission.
 *  On an ASCII machine this is NULL */
extern apr_xlate_t *ap_hdrs_from_ascii;

#endif  /* APR_CHARSET_EBCDIC */

#ifdef __cplusplus
}
#endif

#endif  /* !APACHE_UTIL_CHARSET_H */
/** @} */
