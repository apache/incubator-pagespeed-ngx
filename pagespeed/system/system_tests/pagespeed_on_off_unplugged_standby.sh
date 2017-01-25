#!/bin/bash
#
# Copyright 2016 Google Inc.
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

start_test pagespeed on vs off vs unplugged vs standby

# Each host has collapse_whitespace enabled, and some queries try to turn on
# debug.

function assert_debug_on() {
  check_from "$OUT" grep "^mod_pagespeed on$"  
}
function assert_debug_off() {
  check_not_from "$OUT" grep "^mod_pagespeed on$"
}
function assert_collapse_on() {
  check_from "$OUT" grep "^</table>"
}
function assert_collapse_off() {
  check_not_from "$OUT" grep "^</table>"
}

http_proxy=$SECONDARY_HOSTNAME

start_test pagespeed on, no query params
URL="pagespeed-on.example.com/mod_pagespeed_example/"
OUT=$($WGET_DUMP "$URL")
assert_collapse_on
assert_debug_off

start_test pagespeed on, with query params
URL="pagespeed-on.example.com/mod_pagespeed_example/"
URL+="?PageSpeed=on&PageSpeedFilters=+debug"
OUT=$($WGET_DUMP "$URL")
assert_collapse_on
assert_debug_on

start_test pagespeed on, resource url
URL="pagespeed-on.example.com/mod_pagespeed_example/"
URL+="rewrite_javascript.js.pagespeed.jm.0.js"
check $WGET_DUMP "$URL" -O /dev/null

start_test pagespeed standby, no query params
URL="pagespeed-standby.example.com/mod_pagespeed_example/"
OUT=$($WGET_DUMP "$URL")
assert_collapse_off
assert_debug_off

start_test pagespeed standby, with query params
URL="pagespeed-standby.example.com/mod_pagespeed_example/"
URL+="?PageSpeed=on&PageSpeedFilters=+debug"
OUT=$($WGET_DUMP "$URL")
assert_collapse_on
assert_debug_on

start_test pagespeed standby, resource url
URL="pagespeed-standby.example.com/mod_pagespeed_example/"
URL+="rewrite_javascript.js.pagespeed.jm.0.js"
check $WGET_DUMP "$URL" -O /dev/null

start_test pagespeed unplugged, no query params
URL="pagespeed-unplugged.example.com/mod_pagespeed_example/"
OUT=$($WGET_DUMP "$URL")
assert_collapse_off
assert_debug_off

start_test pagespeed unplugged, with query params
URL="pagespeed-unplugged.example.com/mod_pagespeed_example/"
URL+="?PageSpeed=on&PageSpeedFilters=+debug"
OUT=$($WGET_DUMP "$URL")
assert_collapse_off
assert_debug_off

start_test pagespeed unplugged, resource url
URL="pagespeed-unplugged.example.com/mod_pagespeed_example/"
URL+="rewrite_javascript.js.pagespeed.jm.0.js"
check_not $WGET_DUMP "$URL" -O /dev/null

start_test pagespeed off, no query params
URL="pagespeed-off.example.com/mod_pagespeed_example/"
OUT=$($WGET_DUMP "$URL")
assert_collapse_off
assert_debug_off

start_test pagespeed off, with query params
URL="pagespeed-off.example.com/mod_pagespeed_example/"
URL+="?PageSpeed=on&PageSpeedFilters=+debug"
OUT=$($WGET_DUMP "$URL")

URL="pagespeed-off.example.com/mod_pagespeed_example/"
URL+="rewrite_javascript.js.pagespeed.jm.0.js"
if [ "$SERVER_NAME" = "nginx" ]; then
  # In ngx_pagespeed off=unplugged for historical reasons.
  assert_collapse_off
  assert_debug_off

  start_test pagespeed off, resource url, expect unplugged behavior
  check_not $WGET_DUMP "$URL" -O /dev/null
else
  # Everywhere else off=standby.
  assert_collapse_on
  assert_debug_on

  start_test pagespeed off, resource url, expect standby behavior
  check $WGET_DUMP "$URL" -O /dev/null
fi

unset http_proxy
