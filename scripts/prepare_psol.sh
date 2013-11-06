#!/bin/bash
#
# Copyright 2012 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Author: jefftk@google.com (Jeff Kaufman)
#
# Usage:
#   scripts/prepare_psol.sh /path/to/mod_pagespeed/src
#
# Creates a directory psol/ and copies headers and a few source files from a
# depot_tools (glient) checkout into psol/include.  Along with creating
# binaries, this is a step in preparing psol.tar.gz for distribution.
#

set -u  # check for undefined variables
set -e  # exit on failed commands

if [ "$(basename "$PWD")" != "ngx_pagespeed" ] ; then
  echo "$(basename $0) must be invoked from the ngx_pagespeed directory"
  exit 1
fi

if [ $# -ne 1 ] ; then
  echo "Usage: $(basename $0) /path/to/mod_pagespeed/src"
  exit 1
fi

MOD_PAGESPEED_SRC="$1"

if [ "$(basename "$(dirname "$MOD_PAGESPEED_SRC")")/$( \
        basename "$MOD_PAGESPEED_SRC")" != "mod_pagespeed/src" ] ; then
  echo "Usage: $(basename $0) /path/to/mod_pagespeed/src"
  exit 1
fi

if [ -e psol ] ; then
  echo "A psol/ directory already exists.  Move it somewhere else and rerun."
  exit 1
fi
mkdir psol/
# Copy over the .h files, plus a few selected .cc and .c files.
rsync -arvz "$MOD_PAGESPEED_SRC/" "psol/include/" --prune-empty-dirs \
  --exclude=".svn" \
  --exclude=".git" \
  --include='*.h' \
  --include='*/' \
  --include="apr_thread_compatible_pool.cc" \
  --include="serf_url_async_fetcher.cc" \
  --include="apr_mem_cache.cc" \
  --include="key_value_codec.cc" \
  --include="apr_memcache2.c" \
  --include="loopback_route_fetcher.cc" \
  --include="add_headers_fetcher.cc" \
  --include="console_css_out.cc" \
  --include="console_out.cc" \
  --include="dense_hash_map" \
  --include="dense_hash_set" \
  --include="sparse_hash_map" \
  --include="sparse_hash_set" \
  --include="sparsetable" \
  --include="mod_pagespeed_console_out.cc" \
  --include="mod_pagespeed_console_css_out.cc" \
  --include="mod_pagespeed_console_html_out.cc" \
  --exclude='*'
mkdir -p psol/lib/Debug/linux/ia32
mkdir -p psol/lib/Debug/linux/x64
mkdir -p psol/lib/Release/linux/ia32
mkdir -p psol/lib/Release/linux/x64

# Log that we did this.
SVN_REVISION="$(svn info $MOD_PAGESPEED_SRC | grep Revision | awk '{print $2}')"
SVN_TAG="$(svn info $MOD_PAGESPEED_SRC | grep URL |  awk -F/ '{print $(NF-1)}')"

DATE="$(date +%F)"
echo "${DATE}: Copied from mod_pagespeed ${SVN_TAG}@r${SVN_REVISION} ($USER)" \
  >> psol/include_history.txt

echo
echo "Output is in psol/include.  Now put binaries in psol/lib following"
echo "https://github.com/pagespeed/ngx_pagespeed/wiki/Building-Release-Binaries"
echo "and then you can distribute PSOL."

