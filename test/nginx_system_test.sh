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

: ${FILE_CACHE_PATH:?"Need to set FILE_CACHE_PATH environment variable"}

# We need check and check_not before we source SYSTEM_TEST_FILE that provides
# them.
function handle_failure_simple() {
  echo "FAIL"
  exit 2
}
function check_simple() {
  echo "     check" "$@"
  "$@" || handle_failure_simple
}
function check_not_simple() {
  echo "     check_not" "$@"
  "$@" && handle_failure_simple
}

this_dir="$( dirname "$0" )"

# set up the config file for the test
PAGESPEED_CONF="$this_dir/pagespeed_test.conf"
PAGESPEED_CONF_TEMPLATE="$PAGESPEED_CONF.template"
# check for config file template
check_simple test -e "$PAGESPEED_CONF_TEMPLATE"
# create PAGESPEED_CONF by substituting on PAGESPEED_CONF_TEMPLATE
cat $PAGESPEED_CONF_TEMPLATE \
  | sed 's#@@FILE_CACHE_PATH@@#'"$FILE_CACHE_PATH"'#' \
  > $PAGESPEED_CONF
# make sure we substituted all the variables
check_not_simple grep @@ $PAGESPEED_CONF

echo
echo "Please manually restart nginx.  Press return when done."
read

# run generic system tests
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
  ~In-place resource optimization~
"

source $SYSTEM_TEST_FILE

# nginx-specific system tests

# When we allow ourself to fetch a resource because the Host header tells us
# that it is one of our resources, we should be fetching it from ourself.
start_test Loopback fetches go to local IPs without DNS lookup

# If we're properly fetching from ourself we will issue loopback fetches for
# /mod_pagespeed_example/combine_javascriptN.js, which will succeed, so
# combining will work.  If we're taking 'Host:www.google.com' to mean that we
# should fetch from www.google.com then those fetches will fail because
# google.com won't have /mod_pagespeed_example/combine_javascriptN.js and so
# we'll not rewrite any resources.

URL="$HOSTNAME/mod_pagespeed_example/combine_javascript.html"
URL+="?ModPagespeed=on&ModPagespeedFilters=combine_javascript"
fetch_until "$URL" "fgrep -c .pagespeed." 1 --header=Host:www.google.com

# If this accepts the Host header and fetches from google.com it will fail with
# a 404.  Instead it should use a loopback fetch and succeed.
URL="$HOSTNAME/mod_pagespeed_example/.pagespeed.ce.8CfGBvwDhH.css"
check_simple wget -O /dev/null --header=Host:www.google.com "$URL"

check_failures_and_exit
