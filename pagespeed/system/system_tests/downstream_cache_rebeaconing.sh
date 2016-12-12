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
# Verify that downstream caches and rebeaconing interact correctly for images.
start_test lazyload_images,rewrite_images with downstream cache rebeaconing
HOST_NAME="http://downstreamcacherebeacon.example.com"
URL="$HOST_NAME/mod_pagespeed_test/downstream_caching.html"
URL+="?PageSpeedFilters=lazyload_images"
# 1. Even with blocking rewrite, we don't get an instrumented page when the
# PS-ShouldBeacon header is missing.
OUT1=$(http_proxy=$SECONDARY_HOSTNAME \
          $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' $URL)
check_not_from "$OUT1" egrep -q 'pagespeed\.CriticalImages\.Run'
check_from "$OUT1" grep -q "Cache-Control: private, max-age=3000"
# 2. We get an instrumented page if the correct key is present.
OUT2=$(http_proxy=$SECONDARY_HOSTNAME \
          $WGET_DUMP $WGET_ARGS \
          --header="X-PSA-Blocking-Rewrite: psatest" \
          --header="PS-ShouldBeacon: random_rebeaconing_key" $URL)
check_from "$OUT2" egrep -q "pagespeed\.CriticalImages\.Run"
check_from "$OUT2" grep -q "Cache-Control: max-age=0, no-cache"
# 3. We do not get an instrumented page if the wrong key is present.
OUT3=$(http_proxy=$SECONDARY_HOSTNAME \
          $WGET_DUMP $WGET_ARGS \
          --header="X-PSA-Blocking-Rewrite: psatest" \
          --header="PS-ShouldBeacon: wrong_rebeaconing_key" $URL)
check_not_from "$OUT3" egrep -q "pagespeed\.CriticalImages\.Run"
check_from "$OUT3" grep -q "Cache-Control: private, max-age=3000"

# Verify that downstream caches and rebeaconing interact correctly for css.
test_filter prioritize_critical_css
HOST_NAME="http://downstreamcacherebeacon.example.com"
URL="$HOST_NAME/mod_pagespeed_test/downstream_caching.html"
URL+="?PageSpeedFilters=prioritize_critical_css"
# 1. Even with blocking rewrite, we don't get an instrumented page when the
# PS-ShouldBeacon header is missing.
OUT1=$(http_proxy=$SECONDARY_HOSTNAME \
          $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' $URL)
check_not_from "$OUT1" egrep -q 'pagespeed\.criticalCssBeaconInit'
check_from "$OUT1" grep -q "Cache-Control: private, max-age=3000"

# 2. We get an instrumented page if the correct key is present.
http_proxy=$SECONDARY_HOSTNAME \
  fetch_until -save $URL 'grep -c criticalCssBeaconInit' 2 \
  "--header=PS-ShouldBeacon:random_rebeaconing_key --save-headers"
check grep -q "Cache-Control: max-age=0, no-cache" $FETCH_UNTIL_OUTFILE

# 3. We do not get an instrumented page if the wrong key is present.
OUT3=$(http_proxy=$SECONDARY_HOSTNAME \
          $WGET_DUMP \
          --header 'PS-ShouldBeacon: wrong_rebeaconing_key' \
          --header 'X-PSA-Blocking-Rewrite: psatest' \
          $URL)
check_not_from "$OUT3" egrep -q "pagespeed\.criticalCssBeaconInit"
check_from "$OUT3" grep -q "Cache-Control: private, max-age=3000"
