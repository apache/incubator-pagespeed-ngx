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
start_test ipro and smaxage setup

# We can't use fetch_until because each time through we want to check that
# either s-maxage is set or it's optimized but not both.  So, writing something
# similar to fetch_until.
function check_ipro_s_maxage() {
  local url="$1"
  local expect_optimization="$2"                  # true or false
  local unoptimized_content_length="$3"           # integer
  local expected_unoptimized_cache_control="$4"   # exact match
  local expected_optimized_cache_control="$5"     # grep pattern

  local start_s=$(date +%s)
  local timeout_s=20
  if ! $expect_optimization; then
    timeout_s=3  # Don't wait a long time if we don't expect to succeed.
  fi
  local stop_s=$(($start_s+$timeout_s))

  local seen_unoptimized=false
  local response_body_file="$WGET_DIR/check_ipro_s_maxage.$$"
  echo "Fetching $url until it's optimized..."
  echo "(response body will be in $response_body_file)"
  echo
  while true; do
    OUT=$($CURL -sS -D- -o "$response_body_file" "$url" \
            | sed "s/$(printf "\r")//")
    local cache_control_line=$(echo "$OUT" | grep "^Cache-Control:")

    if [ $(echo "$cache_control_line" | wc -l) -gt 1 ]; then
      echo "Got more than one cache control header."
      echo "FAILed input: $OUT"
      fail
    fi

    local received_body_length=$(cat "$response_body_file" | wc -c)

    if [ "$received_body_length" -gt "$unoptimized_content_length" ]; then
      echo "Received response of $received_body_length bytes; unoptimized"
      echo "content should be $unoptimized_content_length"
      echo "Response body is in $response_body_file"
      echo "FAILed input: $OUT"
      fail
    elif [ "$received_body_length" = "$unoptimized_content_length" ]; then
      # Unoptimized response.
      # This block may run multiple times and should be silent on success.
      if [ "$cache_control_line" != "$expected_unoptimized_cache_control" ]; then
        echo "Got bad cache control, [$cache_control_line], expecting"
        echo "[$expected_unoptimized_cache_control]"
        echo "FAILed input: $OUT"
        fail
      fi

      if echo "$OUT" | grep -q "^Content-Length: "; then
        check_from -q "$OUT" grep -q "^Content-Length: $unoptimized_content_length$"
      else
        check_from -q "$OUT" grep -q "^Transfer-Encoding: chunked$"
      fi
    else
      # Optimized response.
      if ! $expect_optimization; then
        echo "Got unexpected optimization"
        echo "FAILed input: $OUT"
        fail
      fi

      check_from "$OUT" grep "$expected_optimized_cache_control"
      check_from "$OUT" grep \
        "^X-Original-Content-Length: $unoptimized_content_length$"

      # Now we've verified that the optimized version has the right headers.
      # We may not have seen the unoptimized version, if we were running with a
      # warm cache, but if we did see it, it had the right headers as well.
      break
    fi

    if [ $(date +%s) -gt $stop_s ]; then
      if $expect_optimization; then
        echo "Timed out: never got to being optimized"
        echo "FAILed input: $OUT"
        fail
      else
        echo "No optimization after $timeout_s seconds: good!"
        break
      fi
    fi

    echo -n "."
  done
}

start_test ipro resources tagged with s-maxage, CC: max-age=200
check_ipro_s_maxage "$TEST_ROOT/ipro/cc200/example.css" true 41 \
  'Cache-Control: max-age=200, s-maxage=10' \
  '^Cache-Control: max-age=[0-9]*$'

start_test ipro resources tagged with s-maxage, no CC header
EXPECTED_CACHE_CONTROL='Cache-Control: s-maxage=10'
check_ipro_s_maxage "$TEST_ROOT/ipro/nocc/example.css" true 41 \
  "$EXPECTED_CACHE_CONTROL" \
  '^Cache-Control: max-age=[0-9]*$'


start_test ipro resources tagged with s-maxage, CC: private, max-age=200
check_ipro_s_maxage "$TEST_ROOT/ipro/cc200p/example.css" false 41 \
  'Cache-Control: private, max-age=200' \
  'never-optimized'

start_test ipro resources tagged with s-maxage, CC: no-cache, max-age=200
check_ipro_s_maxage "$TEST_ROOT/ipro/cc200nc/example.css" false 41 \
  'Cache-Control: no-cache, max-age=200' \
  'never-optimized'

start_test ipro resources tagged with s-maxage, CC: no-store, max-age=200
check_ipro_s_maxage "$TEST_ROOT/ipro/cc200ns/example.css" false 41 \
  'Cache-Control: no-store, max-age=200' \
  'never-optimized'

start_test ipro resources tagged with s-maxage, CC: no-transform, max-age=200
check_ipro_s_maxage "$TEST_ROOT/ipro/cc200nt/example.css" false 41 \
  'Cache-Control: no-transform, max-age=200' \
  'never-optimized'

start_test ipro resources tagged with s-maxage, CC: s-maxage=5, max-age=200
check_ipro_s_maxage "$TEST_ROOT/ipro/cc200sma5/example.css" true 41 \
  'Cache-Control: s-maxage=5, max-age=200' \
  '^Cache-Control: max-age=[0-9]*$'

start_test ipro resources tagged with s-maxage, CC: s-maxage=50, max-age=200
check_ipro_s_maxage "$TEST_ROOT/ipro/cc200sma50/example.css" true 41 \
  'Cache-Control: s-maxage=10, max-age=200' \
  '^Cache-Control: max-age=[0-9]*$'

start_test ipro resources tagged with s-maxage, CC: s-maxage=50,max-age=200
check_ipro_s_maxage "$TEST_ROOT/ipro/cc200sma50nsp/example.css" true 41 \
  'Cache-Control: s-maxage=10, max-age=200' \
  '^Cache-Control: max-age=[0-9]*$'

start_test ipro resources tagged with s-maxage, CC: max-age=9
check_ipro_s_maxage "$TEST_ROOT/ipro/cc9/example.css" true 41 \
  'Cache-Control: max-age=9' \
  '^Cache-Control: max-age=[0-9]$'

start_test ipro resources tagged with s-maxage, CC: max-age=200, max-age=9
check_ipro_s_maxage "$TEST_ROOT/ipro/cc9/example.css" true 41 \
  'Cache-Control: max-age=9' \
  '^Cache-Control: max-age=[0-9]$'

start_test ipro resources tagged with s-maxage, multiple existing s-maxage
check_ipro_s_maxage "$TEST_ROOT/ipro/cc200sma50sma5/example.css" true 41 \
  'Cache-Control: max-age=200, s-maxage=10, s-maxage=5' \
  '^Cache-Control: max-age=[0-9]*$'

start_test ipro resources tagged with s-maxage, multiple existing max-age
check_ipro_s_maxage "$TEST_ROOT/ipro/cc200sma50cc9/example.css" true 41 \
  'Cache-Control: max-age=200, s-maxage=10, max-age=9' \
  '^Cache-Control: max-age=[0-9]'

start_test ipro resources tagged with s-maxage, no spaces
check_ipro_s_maxage "$TEST_ROOT/ipro/cc200sma50cc9nsp/example.css" true 41 \
  'Cache-Control: max-age=200, s-maxage=10, max-age=9' \
  '^Cache-Control: max-age=[0-9]'

start_test ipro resources tagged with s-maxage, multiple high s-maxage
check_ipro_s_maxage "$TEST_ROOT/ipro/cc200sma50sma51/example.css" true 41 \
  'Cache-Control: max-age=200, s-maxage=10, s-maxage=10' \
  '^Cache-Control: max-age=[0-9]'
