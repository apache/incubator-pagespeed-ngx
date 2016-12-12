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
start_test ?PageSpeed=noscript inserts canonical href link
OUT=$($WGET_DUMP $EXAMPLE_ROOT/defer_javascript.html?PageSpeed=noscript)
check_from "$OUT" fgrep -q \
  "link rel=\"canonical\" href=\"$EXAMPLE_ROOT/defer_javascript.html\""

# Checks that defer_javascript injects 'pagespeed.deferJs' from defer_js.js,
# but strips the comments.
test_filter defer_javascript optimize mode
echo run_wget_with_args $URL
check run_wget_with_args $URL
check grep -q text/psajs $FETCHED
check grep -q /js_defer $FETCHED
check grep -q "PageSpeed=noscript" $FETCHED

# Checks that defer_javascript,debug injects 'pagespeed.deferJs' from
# defer_js.js, but retains the comments.
test_filter defer_javascript,debug optimize mode
FILE=defer_javascript.html?PageSpeedFilters=$FILTER_NAME
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
check run_wget_with_args "$URL"
check grep -q text/psajs $FETCHED
check grep -q /js_defer_debug $FETCHED
# The deferjs src url is in the format js_defer.<hash>.js. This strips out
# everthing except the js filename and saves it to test fetching later.
DEFERJSURL=`grep js_defer $FETCHED | sed 's/^.*js_defer/js_defer/;s/\.js.*$/\.js/g;'`
check grep -q "PageSpeed=noscript" $FETCHED

# Extract out the DeferJs url from the HTML above and fetch it.
start_test Fetch the deferJs url with hash.
echo run_wget_with_args $DEFERJSURL
run_wget_with_args -q \
  http://$PROXY_DOMAIN/$PSA_JS_LIBRARY_URL_PREFIX/$DEFERJSURL
check_200_http_response_file "$WGET_OUTPUT"
check fgrep "Cache-Control: max-age=31536000" $WGET_OUTPUT

# Checks that we return 404 for static file request without hash.
start_test Access to js_defer.js without hash returns 404.
URL="http://$PROXY_DOMAIN/$PSA_JS_LIBRARY_URL_PREFIX/js_defer.js"
echo run_wget_with_args "$URL"
check_not run_wget_with_args "$URL"
check fgrep "404 Not Found" $WGET_OUTPUT

# Checks that outlined js_defer.js is served correctly.
start_test serve js_defer.0.js
URL="http://$PROXY_DOMAIN/$PSA_JS_LIBRARY_URL_PREFIX/js_defer.0.js"
echo run_wget_with_args "$URL"
run_wget_with_args -q "$URL"
check_200_http_response_file "$WGET_OUTPUT"
check fgrep "Cache-Control: max-age=300,private" $WGET_OUTPUT

# Checks that outlined js_defer_debug.js is  served correctly.
start_test serve js_defer_debug.0.js
URL="http://$PROXY_DOMAIN/$PSA_JS_LIBRARY_URL_PREFIX/js_defer_debug.0.js"
echo run_wget_with_args "$URL"
run_wget_with_args -q "$URL"
check_200_http_response_file "$WGET_OUTPUT"
check fgrep "Cache-Control: max-age=300,private" $WGET_OUTPUT
