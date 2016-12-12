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
test_filter combine_css combines 4 CSS files into 1.
fetch_until $URL 'fgrep -c text/css' 1
check run_wget_with_args $URL
#test_resource_ext_corruption $URL $combine_css_filename

start_test combine_css without hash field should 404
URL=$REWRITTEN_ROOT/styles/yellow.css+blue.css.pagespeed.cc..css
echo run_wget_with_args $URL
check_not run_wget_with_args $URL
check fgrep "404 Not Found" $WGET_OUTPUT

# Note: this large URL can only be processed by Apache if
# ap_hook_map_to_storage is called to bypass the default
# handler that maps URLs to filenames.
start_test Fetch large css_combine URL
LARGE_URL="$REWRITTEN_ROOT/styles/yellow.css+blue.css+big.css+\
bold.css+yellow.css+blue.css+big.css+bold.css+yellow.css+blue.css+\
big.css+bold.css+yellow.css+blue.css+big.css+bold.css+yellow.css+blue.css+\
big.css+bold.css+yellow.css+blue.css+big.css+bold.css+yellow.css+blue.css+\
big.css+bold.css+yellow.css+blue.css+big.css+bold.css+yellow.css+blue.css+\
big.css+bold.css+yellow.css+blue.css+big.css+bold.css+yellow.css+blue.css+\
big.css+bold.css+yellow.css+blue.css+big.css+bold.css+yellow.css+blue.css+\
big.css+bold.css+yellow.css+blue.css+big.css+bold.css+yellow.css+blue.css+\
big.css+bold.css+yellow.css+blue.css+big.css+bold.css+yellow.css+blue.css+\
big.css+bold.css+yellow.css+blue.css+big.css+\
bold.css.pagespeed.cc.46IlzLf_NK.css"
echo "$WGET --save-headers -q -O - $LARGE_URL | check_200_http_response"
OUT=$($WGET --save-headers -q -O - $LARGE_URL)
check_200_http_response "$OUT"
LARGE_URL_LINE_COUNT=$($WGET -q -O - $LARGE_URL | wc -l)
echo Checking that response body is at least 900 lines -- it should be 954
check [ $LARGE_URL_LINE_COUNT -gt 900 ]

test_filter combine_javascript combines 2 JS files into 1.
fetch_until $URL 'fgrep -c src=' 1
check run_wget_with_args $URL

start_test combine_javascript with long URL still works
URL=$TEST_ROOT/combine_js_very_many.html?PageSpeedFilters=combine_javascript
fetch_until $URL 'fgrep -c src=' 4

test_filter combine_heads combines 2 heads into 1
check run_wget_with_args $URL
check [ $(fgrep -c '<head>' $FETCHED) = 1 ]

start_test "combine_css debug filter"
URL=$EXAMPLE_ROOT/combine_css_debug.html?PageSpeedFilters=combine_css,debug
fetch_until -save "$URL" \
  "fgrep -c styles/yellow.css+blue.css+big.css+bold.css.pagespeed.cc" 1
check fgrep "potentially non-combinable attribute: &#39;id&#39;" $FETCH_FILE
check fgrep \
  "potentially non-combinable attributes: &#39;data-foo&#39; and &#39;data-bar&#39;" \
  $FETCH_FILE
check fgrep \
  "attributes: &#39;data-foo&#39;, &#39;data-bar&#39; and &#39;data-baz&#39;" \
  $FETCH_FILE
check fgrep "looking for media &#39;&#39; but found media=&#39;print&#39;." \
  $FETCH_FILE
check fgrep "looking for media &#39;print&#39; but found media=&#39;&#39;." \
  $FETCH_FILE
check fgrep "Could not combine over barrier: noscript" $FETCH_FILE
check fgrep "Could not combine over barrier: inline style" $FETCH_FILE
check fgrep "Could not combine over barrier: IE directive" $FETCH_FILE
