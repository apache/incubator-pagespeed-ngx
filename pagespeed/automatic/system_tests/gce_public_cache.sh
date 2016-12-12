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
YELLOW="$EXAMPLE_ROOT/styles/yellow.css"
PUBYELLOW="$TEST_ROOT/public/yellow.css"

# Checks whether a fetching a file, fetched with or without a Via header,
# results in a cache-control header with "public".
# Usage:
#  check_public_cc FILE [-via] public|no
function check_public_cc() {
  file="$1"
  shift

  if [ "$1" = -via ]; then
    echo fetching $file with Via
    cache_control=$($WGET_DUMP --header "Via: 1.1 google" "$file" | \
      scrape_header Cache-Control)
    shift
  else
    echo fetching $file without Via
    cache_control=$($WGET_DUMP "$file" | scrape_header Cache-Control)
  fi

  if [ "$1" = "public" ]; then
    check_from "$cache_control" fgrep -q public
  else
    check_not_from "$cache_control" fgrep -q public
  fi
}

# Takes an HTML CSS link and returns the URL of the CSS file.
function scrape_css_link() {
  url=$(tr -s '[:space:]' '\n' | grep href | cut -d= -f2 | cut '-d"' -f2)
  # We will want to fetch the URL with wget, so absolutify what was
  # in the HTML if necessary.
  if [[ "$url" != http* ]]; then
    url="$1/$url"
  fi
  echo "$url"
}

# For combined resources, identified with the correct hash, we do not have
# to wait for the cache to be warm to expect optimized output with proper
# headers.  This is because the HTML document requires the combined file; we
# can't fall back to individual files.
start_test Cache-Control:public added iff GCE for combined .pagespeed. file.
fetch_until -save \
  $EXAMPLE_ROOT/combine_css.html?PageSpeedFilters=combine_css \
  "fgrep -c .pagespeed.cc" 1
COMBINED=$(scrape_css_link "$EXAMPLE_ROOT" < $FETCH_FILE)
check_public_cc "$COMBINED" no
check_public_cc "$COMBINED" -via public
check_public_cc "$COMBINED" no
check_public_cc "$COMBINED" -via public

# We won't add 'public' to an ipro-request until the request is optimized, so
# wait for that to happen.  Then we can check an ipro-rewritten resource to make
# sure it gets the proper headers.
start_test Cache-Control:public added iff GCE for ipro css file.
fetch_until "$YELLOW" 'fgrep -c background-color:#ff0' 1
check_public_cc "$YELLOW" no
check_public_cc "$YELLOW" -via public
check_public_cc "$YELLOW" no
check_public_cc "$YELLOW" -via public

start_test Cache-Control:public when source has cc:public, for ipro
fetch_until "$PUBYELLOW" 'fgrep -c background-color:#ff0' 1
check_public_cc "$PUBYELLOW" -via public
check_public_cc "$PUBYELLOW" public
check_public_cc "$PUBYELLOW" -via public
check_public_cc "$PUBYELLOW" public

start_test Cache-Control:public when source has cc:public, for .pagespeed.
fetch_until -save \
  $TEST_ROOT/public/rewrite_css.html?PageSpeedFilters=rewrite_css \
  "fgrep -c .pagespeed.cf" 1
REWRITTEN_YELLOW=$(scrape_css_link "$TEST_ROOT/public" < $FETCH_FILE)
check_public_cc "$REWRITTEN_YELLOW" -via public
check_public_cc "$REWRITTEN_YELLOW" public
check_public_cc "$REWRITTEN_YELLOW" -via public
check_public_cc "$REWRITTEN_YELLOW" public
