#!/bin/bash
#
# Copyright 2013 Google Inc.
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

# Tests that mod_pagespeed can fetch HTTPS resources.  Note that mod_pagespeed
# does not work like this by default: a flag must be specified in
# pagespeed.conf:
#   ModPagespeedFetchHttps enable

echo Testing that HTTPS fetching is enabled and working in mod_pagespeed.
echo Note that this test will fail with timeouts if the serf fetcher has not
echo been compiled in.

this_dir="$( dirname "${BASH_SOURCE[0]}" )"
PAGESPEED_CODE_DIR="$this_dir/../../../third_party/pagespeed"
if [ ! -e "$PAGESPEED_CODE_DIR" ] ; then
  PAGESPEED_CODE_DIR="$this_dir/../pagespeed"
fi
SERVER_NAME=apache
source "$PAGESPEED_CODE_DIR/automatic/system_test_helpers.sh" || exit 1

echo Test that we can rewrite an HTTPS resource from a domain with a valid cert.
fetch_until $TEST_ROOT/https_fetch/https_fetch.html \
  'grep -c /https_gstatic_dot_com/1.gif.pagespeed.ce' 1
