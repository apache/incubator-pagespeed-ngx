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
# This filter loads below the fold images lazily.
test_filter lazyload_images
check run_wget_with_args $URL
# Check src gets swapped with data-pagespeed-lazy-src
check fgrep -q "data-pagespeed-lazy-src=\"images/Puzzle2.jpg\"" $FETCHED
check fgrep -q "data-pagespeed-lazy-srcset=\"images/Puzzle.jpg 2x\"" $FETCHED
check fgrep -q "pagespeed.lazyLoadInit" $FETCHED  # inline script injected

# Checks that lazyload_images injects compiled javascript from
# lazyload_images.js.
test_filter lazyload_images optimize mode
echo run_wget_with_args $URL
check run_wget_with_args $URL
check grep -q pagespeed.lazyLoad $FETCHED
check_not grep '/\*' $FETCHED
check grep -q "PageSpeed=noscript" $FETCHED
# The lazyload placeholder image is in the format 1.<hash>.gif. This matches the
# first src attribute set to the placeholder, and then strips out everything
# except for the gif name for later testing of fetching this image.
BLANKGIFSRC=`grep -m1 -o " src=.*1.*.gif" $FETCHED | sed 's/^.*1\./1./;s/\.gif.*$/\.gif/g'`

# Fetch the blank image and make sure it's served correctly.
start_test serve_blank_gif
URL="http://$PROXY_DOMAIN/$PSA_JS_LIBRARY_URL_PREFIX/$BLANKGIFSRC"
echo run_wget_with_args $URL
run_wget_with_args -q $URL
check_200_http_response_file "$WGET_OUTPUT"
check fgrep "Cache-Control: max-age=31536000" $WGET_OUTPUT

# Checks that lazyload_images,debug injects non-optimized javascript from
# lazyload_images.js. The debug JS will still have comments stripped, since we
# run it through the closure compiler to resolve any uses of goog.require.
test_filter lazyload_images,debug debug mode
FILE=lazyload_images.html?PageSpeedFilters=$FILTER_NAME
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
check run_wget_with_args "$URL"
check grep -q pagespeed.lazyLoad $FETCHED
check_not grep -q '/\*' $FETCHED
check_not grep -q 'goog.require' $FETCHED
check grep -q "PageSpeed=noscript" $FETCHED
