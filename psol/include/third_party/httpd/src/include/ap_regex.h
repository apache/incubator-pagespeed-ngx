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

/* Derived from PCRE's pcreposix.h.

            Copyright (c) 1997-2004 University of Cambridge

-----------------------------------------------------------------------------
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the University of Cambridge nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------------
*/

/**
 * @file ap_regex.h
 * @brief Apache Regex defines
 */

#ifndef AP_REGEX_H
#define AP_REGEX_H

#include "apr.h"

/* Allow for C++ users */

#ifdef __cplusplus
extern "C" {
#endif

/* Options for ap_regexec: */

#define AP_REG_ICASE    0x01 /** use a case-insensitive match */
#define AP_REG_NEWLINE  0x02 /** don't match newlines against '.' etc */
#define AP_REG_NOTBOL   0x04 /** ^ will not match against start-of-string */
#define AP_REG_NOTEOL   0x08 /** $ will not match against end-of-string */

#define AP_REG_EXTENDED (0)  /** unused */
#define AP_REG_NOSUB    (0)  /** unused */

/* Error values: */
enum {
  AP_REG_ASSERT = 1,  /** internal error ? */
  AP_REG_ESPACE,      /** failed to get memory */
  AP_REG_INVARG,      /** invalid argument */
  AP_REG_NOMATCH      /** match failed */
};

/* The structure representing a compiled regular expression. */
typedef struct {
    void *re_pcre;
    apr_size_t re_nsub;
    apr_size_t re_erroffset;
} ap_regex_t;

/* The structure in which a captured offset is returned. */
typedef struct {
    int rm_so;
    int rm_eo;
} ap_regmatch_t;

/* The functions */

/**
 * Compile a regular expression.
 * @param preg Returned compiled regex
 * @param regex The regular expression string
 * @param cflags Must be zero (currently).
 * @return Zero on success or non-zero on error
 */
AP_DECLARE(int) ap_regcomp(ap_regex_t *preg, const char *regex, int cflags);

/**
 * Match a NUL-terminated string against a pre-compiled regex.
 * @param preg The pre-compiled regex
 * @param string The string to match
 * @param nmatch Provide information regarding the location of any matches
 * @param pmatch Provide information regarding the location of any matches
 * @param eflags Bitwise OR of any of AP_REG_* flags 
 * @return 0 for successful match, #REG_NOMATCH otherwise
 */ 
AP_DECLARE(int) ap_regexec(const ap_regex_t *preg, const char *string,
                           apr_size_t nmatch, ap_regmatch_t *pmatch, int eflags);

/**
 * Return the error code returned by regcomp or regexec into error messages
 * @param errcode the error code returned by regexec or regcomp
 * @param preg The precompiled regex
 * @param errbuf A buffer to store the error in
 * @param errbuf_size The size of the buffer
 */
AP_DECLARE(apr_size_t) ap_regerror(int errcode, const ap_regex_t *preg, 
                                   char *errbuf, apr_size_t errbuf_size);

/** Destroy a pre-compiled regex.
 * @param preg The pre-compiled regex to free.
 */
AP_DECLARE(void) ap_regfree(ap_regex_t *preg);

#ifdef __cplusplus
}   /* extern "C" */
#endif

#endif /* AP_REGEX_T */

