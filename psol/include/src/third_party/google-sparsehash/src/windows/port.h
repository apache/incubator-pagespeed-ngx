/* Copyright (c) 2007, Google Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ---
 * Author: Craig Silverstein
 *
 * These are some portability typedefs and defines to make it a bit
 * easier to compile this code -- in particular, unittests -- under VC++.
 * Other portability code is found in windows/google/sparsehash/sparseconfig.h.
 *
 * Several of these are taken from glib:
 *    http://developer.gnome.org/doc/API/glib/glib-windows-compatability-functions.html
 */

#ifndef SPARSEHASH_WINDOWS_PORT_H_
#define SPARSEHASH_WINDOWS_PORT_H_

#include "config.h"

#ifdef WIN32

#define WIN32_LEAN_AND_MEAN  /* We always want minimal includes */
#include <windows.h>
#include <io.h>              /* because we so often use open/close/etc */
#include <string>

// 4996: Yes, we're ok using the "unsafe" functions like _vsnprintf and fopen
// 4127: We use "while (1)" sometimes: yes, we know it's a constant
#pragma warning(disable:4996 4127)


// file I/O
#define PATH_MAX 1024
#define access  _access
#define getcwd  _getcwd
#define open    _open
#define read    _read
#define write   _write
#define lseek   _lseek
#define close   _close
#define unlink  _unlink
#define popen   _popen
#define pclose  _pclose

#define strdup  _strdup

// We can't just use _snprintf as a drop-in replacement, because it
// doesn't always NUL-terminate. :-(
extern int snprintf(char *str, size_t size, const char *format, ...);

extern std::string TmpFile(const char* basename); // used in hashtable_unittest

#endif  /* WIN32 */

#endif  /* SPARSEHASH_WINDOWS_PORT_H_ */
