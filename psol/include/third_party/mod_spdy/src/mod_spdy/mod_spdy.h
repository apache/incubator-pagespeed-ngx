/* Copyright 2011 Google Inc.
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

/* This is a public header file, to be used by other Apache modules.  So,
 * identifiers declared here should follow Apache module naming conventions
 * (specifically, identifiers should be lowercase_with_underscores, and our
 * identifiers should start with the spdy_ prefix), and this header file must
 * be valid in old-school C (not just C++). */

#ifndef MOD_SPDY_MOD_SPDY_H_
#define MOD_SPDY_MOD_SPDY_H_

#include "httpd.h"
#include "apr_optional.h"

#ifdef __cplusplus
extern "C" {
#endif

/** An optional function which returns zero if the given connection is _not_
 * using SPDY, and otherwise returns the (non-zero) SPDY protocol version
 * number being used on the connection.  This can be used e.g. to alter the
 * response for a given request to optimize for SPDY if SPDY is being used. */
APR_DECLARE_OPTIONAL_FN(int, spdy_get_version, (conn_rec*));

/* TODO(mdsteele): Add an optional function for doing a SPDY server push. */

/* TODO(mdsteele): Consider adding an optional function to tell mod_spdy NOT to
 * use SPDY for a connection (similar to ssl_engine_disable in mod_ssl). */

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MOD_SPDY_MOD_SPDY_H_ */
