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
start_test Ipro transcode to webp, iterating with Noop
# There's a trick for making demo pages that show the fully optimized ipro
# images without relying on the user flushing the browser cache, which relies
# on a 'Noop' option setting via query-param.  The Noop option does not enter
# into signature computation, but does get stripped from HTTP cache keys.  To
# test this, we must first fetch an image with ?PageSpeedNoop=RANDOM1, until
# it is optimized.  Then we fetch the same image with ?PageSpeedNoop=RANDOM2,
# and we expect there will be no extra image optimizations.
#
# To use this trick you fetch the same URL from JavaScript until you get
# the optimized result, bumping the PageSpeedNoop version each time in the
# query param, which busts the browser cache but not the pagespeed Metadata
# or HTTP cache.  The Metadata cache does not bust because "Noop" is excluded
# from signature computation.  The HTTP cache is not busted because the
# pagespeed query params are stripped from the generate internal .pagespeed.
# URL (but not other query params).
#
# As we are checking some statistics, try get the system to quiesce to reduce
# flakiness from outstanding background rewrites triggered by tests above.
echo -n Waiting for quiescence by checking serf_fetch_active_count ...
while [ $(scrape_stat serf_fetch_active_count) -gt 0 ]; do
  echo -n .
  sleep .1
done
sleep 2
echo " done"
URL="$EXAMPLE_ROOT/images/Puzzle.jpg"
URL+="?PageSpeedFilters=+in_place_optimize_for_browser"
WGET_ARGS="--user-agent webp --header Accept:image/webp"
RANDOM1=$RANDOM
RANDOM2=$((RANDOM1 + 1))
URL1="${URL}&PageSpeedNoop=$RANDOM1"
URL2="${URL}&PageSpeedNoop=$RANDOM2"
fetch_until "$URL1" "grep -c image/webp" 1 --save-headers
#NUM_REWRITES_URL1=$(scrape_stat image_rewrites)
NUM_FETCHES_URL1=$(scrape_stat http_fetches)
check $WGET -q $WGET_ARGS --save-headers "$URL2" -O $WGET_OUTPUT
#NUM_REWRITES_URL2=$(scrape_stat image_rewrites)
NUM_FETCHES_URL2=$(scrape_stat http_fetches)
check_from "$(extract_headers $WGET_OUTPUT)" grep -q "image/webp"
#check [ $NUM_REWRITES_URL2 = $NUM_REWRITES_URL1 ]
check [ $NUM_FETCHES_URL2 = $NUM_FETCHES_URL1 ]
URL=""
