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
test_filter rewrite_javascript minifies JavaScript and saves bytes.
# External scripts rewritten.
fetch_until -save -recursive \
  $URL 'grep -c src=.*rewrite_javascript\.js\.pagespeed\.jm\.' 2
check_not grep removed $WGET_DIR/*.pagespeed.jm.*  # No comments should remain.
check_file_size $FETCH_FILE -lt 1560               # Net savings
check grep -q preserved $FETCH_FILE                # Preserves certain comments.
# Rewritten JS is cache-extended.
check grep -qi "Cache-control: max-age=31536000" $WGET_OUTPUT
check grep -qi "Expires:" $WGET_OUTPUT
WGET_ARGS=""

start_test rewrite_javascript_external
URL="$EXAMPLE_ROOT/rewrite_javascript.html"
fetch_until -save "$URL?PageSpeedFilters=rewrite_javascript_external" \
  'grep -c src=.*rewrite_javascript\.js\.pagespeed\.jm\.' 2
check_from "$(< $FETCH_UNTIL_OUTFILE)" \
  fgrep -q "// This comment will be removed"

start_test rewrite_javascript_inline
URL="$EXAMPLE_ROOT/rewrite_javascript.html"
# We test with blocking rewrites here because we are trying to prove
# we will never rewrite the external JS, which is impractical to do
# with fetch_until.
OUT=$($WGET_DUMP --header=X-PSA-Blocking-Rewrite:psatest \
  $URL?PageSpeedFilters=rewrite_javascript_inline)
# Checks that the inline JS block was minified.
check_not_from "$OUT" fgrep -q "// This comment will be removed"
check_from "$OUT" fgrep -q 'id="int1">var state=0;document.write'
# Checks that the external JS links were left alone.
check [ $(echo "$OUT" | fgrep -c 'src="rewrite_javascript.js"') -eq 2 ]
check_not_from "$OUT" fgrep -q 'src=.*rewrite_javascript\.js\.pagespeed\.jm\.'

# Error path for fetch of outlined resources that are not in cache leaked
# at one point of development.
start_test regression test for RewriteDriver leak
check_not $WGET -O /dev/null -o /dev/null \
  $TEST_ROOT/_.pagespeed.jo.3tPymVdi9b.js

# Combination rewrite in which the same URL occurred twice used to
# lead to a large delay due to overly late lock release.
start_test regression test with same filtered input twice in combination
PAGE=_,Mco.0.css+_,Mco.0.css.pagespeed.cc.0.css
URL=$TEST_ROOT/$PAGE?PageSpeedFilters=combine_css,outline_css
check_error_code 8 \
  $WGET -q -O /dev/null -o /dev/null --tries=1 --read-timeout=3 $URL
# We want status code 8 (server-issued error) and not 4
# (network failure/timeout)
