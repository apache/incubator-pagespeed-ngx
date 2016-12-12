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
# Test ForbidFilters, which is set in the config for the VHost
# forbidden.example.com, where we've forbidden remove_quotes, remove_comments,
# collapse_whitespace, rewrite_css, and resize_images; we've also disabled
# inline_css so the link doesn't get inlined since we test that it still has
# all its quotes.
FORBIDDEN_TEST_ROOT=http://forbidden.example.com/mod_pagespeed_test
function test_forbid_filters() {
  QUERYP="$1"
  HEADER="$2"
  URL="$FORBIDDEN_TEST_ROOT/forbidden.html"
  OUTFILE="$TESTTMP/test_forbid_filters"
  echo http_proxy=$SECONDARY_HOSTNAME $WGET $HEADER $URL$QUERYP
  http_proxy=$SECONDARY_HOSTNAME $WGET -q -O $OUTFILE $HEADER $URL$QUERYP
  check egrep -q '<link rel="stylesheet' $OUTFILE
  check egrep -q '<!--'                  $OUTFILE
  check egrep -q '    <li>'              $OUTFILE
  rm -f $OUTFILE
}
start_test ForbidFilters baseline check.
test_forbid_filters "" ""
start_test ForbidFilters query parameters check.
QUERYP="?PageSpeedFilters="
QUERYP="${QUERYP}+remove_quotes,+remove_comments,+collapse_whitespace"
test_forbid_filters $QUERYP ""
start_test "ForbidFilters request headers check."
HEADER="--header=PageSpeedFilters:"
HEADER="${HEADER}+remove_quotes,+remove_comments,+collapse_whitespace"
test_forbid_filters "" $HEADER

start_test ForbidFilters disallows direct resource rewriting.
FORBIDDEN_EXAMPLE_ROOT=http://forbidden.example.com/mod_pagespeed_example
FORBIDDEN_STYLES_ROOT=$FORBIDDEN_EXAMPLE_ROOT/styles
FORBIDDEN_IMAGES_ROOT=$FORBIDDEN_EXAMPLE_ROOT/images
# .ce. is allowed
ALLOWED="$FORBIDDEN_STYLES_ROOT/all_styles.css.pagespeed.ce.n7OstQtwiS.css"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $ALLOWED 2>&1)
check_200_http_response "$OUT"
# .cf. is forbidden
FORBIDDEN=$FORBIDDEN_STYLES_ROOT/A.all_styles.css.pagespeed.cf.UH8L-zY4b4.css
OUT=$(http_proxy=$SECONDARY_HOSTNAME check_not $WGET -O /dev/null $FORBIDDEN \
  2>&1)
check_from "$OUT" fgrep -q "404 Not Found"
# The image will be optimized but NOT resized to the much smaller size,
# so it will be >200k (optimized) rather than <20k (resized).
# Use a blocking fetch to force all -allowed- rewriting to be done.
RESIZED=$FORBIDDEN_IMAGES_ROOT/256x192xPuzzle.jpg.pagespeed.ic.8AB3ykr7Of.jpg
HEADERS="$OUTDIR/headers"
http_proxy=$SECONDARY_HOSTNAME $WGET -q --server-response \
  -O $WGET_DIR/forbid.jpg \
  --header 'X-PSA-Blocking-Rewrite: psatest' $RESIZED >& $HEADERS
LENGTH=$(grep '^ *Content-Length:' $HEADERS | sed -e 's/.*://')
check test -n "$LENGTH"
check test $LENGTH -gt 200000
CCONTROL=$(grep '^ *Cache-Control:' $HEADERS | sed -e 's/.*://')
check_from "$CCONTROL" grep -w max-age=300
check_from "$CCONTROL" grep -w private
