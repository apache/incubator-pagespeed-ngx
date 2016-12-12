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
# Test ForbidAllDisabledFilters, which is set in config for
# /mod_pagespeed_test/forbid_all_disabled/disabled/ where we've disabled
# remove_quotes, remove_comments, and collapse_whitespace (which are enabled
# for its parent directory). We fetch 3 x 3 times, the first 3
# being for forbid_all_disabled, fordid_all_disabled/disabled, and
# forbid_all_disabled/disabled/cheat, to ensure that a subdirectory cannot
# circumvent the forbidden flag; and the second 3 being a normal fetch, a
# fetch using a query parameter to try to enable the forbidden filters, and a
# fetch using a request header to try to enable the forbidden filters.
function test_forbid_all_disabled() {
  QUERYP="$1"
  HEADER="$2"
  if [ -n "$QUERYP" ]; then
    INLINE_CSS=",-inline_css"
  else
    INLINE_CSS="?PageSpeedFilters=-inline_css"
  fi
  WGET_ARGS="--header=X-PSA-Blocking-Rewrite:psatest"
  URL1=$TEST_ROOT/forbid_all_disabled/forbidden.html
  URL2=$TEST_ROOT/forbid_all_disabled/disabled/forbidden.html
  URL3=$TEST_ROOT/forbid_all_disabled/disabled/cheat/forbidden.html
  OUTFILE="$TESTTMP/test_forbid_all_disabled"
  # Fetch testing that forbidden filters stay disabled.
  echo $WGET $HEADER $URL1$QUERYP$INLINE_CSS
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL1$QUERYP$INLINE_CSS
  check     egrep -q '<link rel=stylesheet'  $OUTFILE
  check_not egrep -q '<!--'                  $OUTFILE
  check     egrep -q '^<li>'                 $OUTFILE
  echo $WGET $HEADER $URL2$QUERYP$INLINE_CSS
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL2$QUERYP$INLINE_CSS
  check     egrep -q '<link rel="stylesheet' $OUTFILE
  check     egrep -q '<!--'                  $OUTFILE
  check     egrep -q '    <li>'              $OUTFILE
  echo $WGET $HEADER $URL3$QUERYP$INLINE_CSS
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL3$QUERYP$INLINE_CSS
  check     egrep -q '<link rel="stylesheet' $OUTFILE
  check     egrep -q '<!--'                  $OUTFILE
  check     egrep -q '    <li>'              $OUTFILE
  # Fetch testing that enabling inline_css for disabled/ directory works.
  echo $WGET $HEADER $URL1
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL1
  check_not egrep -q '<style>.yellow'        $OUTFILE
  echo $WGET $HEADER $URL2
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL2
  check     egrep -q '<style>.yellow'        $OUTFILE
  echo $WGET $HEADER $URL3
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL3
  check     egrep -q '<style>.yellow'        $OUTFILE
  rm -f $OUTFILE
  WGET_ARGS=""
}
start_test ForbidAllDisabledFilters baseline check.
test_forbid_all_disabled "" ""
start_test ForbidAllDisabledFilters query parameters check.
QUERYP="?PageSpeedFilters="
QUERYP="${QUERYP}+remove_quotes,+remove_comments,+collapse_whitespace"
test_forbid_all_disabled $QUERYP ""
start_test ForbidAllDisabledFilters request headers check.
HEADER="--header=PageSpeedFilters:"
HEADER="${HEADER}+remove_quotes,+remove_comments,+collapse_whitespace"
test_forbid_all_disabled "" $HEADER
