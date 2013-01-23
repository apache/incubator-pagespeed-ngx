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
#
# Runs pagespeed's generic system test.  Will eventually run nginx-specific
# tests as well.
#
# Exits with status 0 if all tests pass.
# Exits with status 1 immediately if any test fails.
# Exits with status 2 if command line args are wrong.
#
# Usage:
#   Set up nginx to serve mod_pagespeed/src/install/ statically at the server
#   root, then run:
#     ./ngx_system_test.sh HOST:PORT
#   for example:
#     ./ngx_system_test.sh localhost:8050
#

this_dir="$( dirname "$0" )"

SYSTEM_TEST_FILE="$this_dir/../../mod_pagespeed/src/install/system_test.sh"

if [ ! -e "$SYSTEM_TEST_FILE" ] ; then
  echo "Not finding $SYSTEM_TEST_FILE -- is mod_pagespeed not in a parallel"
  echo "directory to ngx_pagespeed?"
  exit 2
fi

PSA_JS_LIBRARY_URL_PREFIX="ngx_pagespeed_static"

PAGESPEED_EXPECTED_FAILURES="
  ~compression is enabled for rewritten JS.~
  ~convert_meta_tags~
  ~insert_dns_prefetch~
"

source $SYSTEM_TEST_FILE
check_failures_and_exit
