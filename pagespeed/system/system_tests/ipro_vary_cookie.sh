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
# Even though we don't have a cookie, we will conservatively avoid
# optimizing resources with Vary:Cookie set on the response, so we
# will not get the instant response, of "body{background:#9370db}":
# 24 bytes, but will get the full original text:
#     "body {\n    background: MediumPurple;\n}\n"
# This will happen whether or not we send a cookie.
#
# Testing this requires proving we'll never optimize something, which
# can't be distinguished from the not-yet-optimized case, except by the
# ipro_not_rewritable stat, so we loop by scraping that stat and seeing
# when it changes.

# Executes commands until ipro_no_rewrite_count changes.  The
# command-line options are all passed to WGET_DUMP.  Leaves command
# wget output in $IPRO_OUTPUT.
function ipro_expect_no_rewrite() {
  ipro_no_rewrite_count_start=$(scrape_stat ipro_not_rewritable)
  ipro_no_rewrite_count=$ipro_no_rewrite_count_start
  iters=0
  while [ $ipro_no_rewrite_count -eq $ipro_no_rewrite_count_start ]; do
    if [ $iters -ne 0 ]; then
      sleep 0.1
      if [ $iters -gt 100 ]; then
        echo TIMEOUT
        exit 1
      fi
    fi
    IPRO_OUTPUT=$($WGET_DUMP "$@")
    ipro_no_rewrite_count=$(scrape_stat ipro_not_rewritable)
    iters=$((iters + 1))
  done
}

start_test ipro with vary:cookie with no cookie set
ipro_expect_no_rewrite $TEST_ROOT/ipro/cookie/vary_cookie.css
check_from "$IPRO_OUTPUT" fgrep -q '    background: MediumPurple;'
check_from "$IPRO_OUTPUT" egrep -q 'Vary: (Accept-Encoding,)?Cookie'

start_test ipro with vary:cookie with cookie set
ipro_expect_no_rewrite $TEST_ROOT/ipro/cookie/vary_cookie.css \
  --header=Cookie:cookie-data
check_from "$IPRO_OUTPUT" fgrep -q '    background: MediumPurple;'
check_from "$IPRO_OUTPUT" egrep -q 'Vary: (Accept-Encoding,)?Cookie'

start_test ipro with vary:cookie2 with no cookie2 set
ipro_expect_no_rewrite $TEST_ROOT/ipro/cookie2/vary_cookie2.css
check_from "$IPRO_OUTPUT" fgrep -q '    background: MediumPurple;'
check_from "$IPRO_OUTPUT" egrep -q 'Vary: (Accept-Encoding,)?Cookie2'

start_test ipro with vary:cookie2 with cookie2 set
ipro_expect_no_rewrite $TEST_ROOT/ipro/cookie2/vary_cookie2.css \
  --header=Cookie2:cookie2-data
check_from "$IPRO_OUTPUT" fgrep -q '    background: MediumPurple;'
check_from "$IPRO_OUTPUT" egrep -q 'Vary: (Accept-Encoding,)?Cookie2'
