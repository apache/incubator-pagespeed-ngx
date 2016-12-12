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
start_test Broken fetches with ipro-recording 404 after cache flush.

# Note that failed cached results from an earlier run can disturb this
# test, so start it always with a clean slate.
CACHE_DIR="$FILE_CACHE/broken-fetch"
rm -rf "$CACHE_DIR"

# Seed the HTTP cache with the ipro-optimized broken-fetch.js.  We know
# broken-fetch.js optimized when the spaces are removed from "a = 0".
DIR="http://broken-fetch.example.com/mod_pagespeed_test"
http_proxy=$SECONDARY_HOSTNAME fetch_until "$DIR/broken-fetch.js" \
  'fgrep -c a=0' 1

# Now when we do an HTML fetch, we will see a .pagespeed. URL, which used
# the ipro-optimized resource.  No 'fetch_until' is needed; the cache is
# already there.
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP "$DIR/broken-fetch.html")
check_from "$OUT" fgrep -q .pagespeed.

# Scrape the optimized JS URL from the HTML output.
OPT_JS=$(echo "$OUT" | grep '<script src' | cut -f2 -d\")
echo optimized JS = "$OPT_JS"

# Sanity check we fetch the optimized result.
echo http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP "$DIR/$OPT_JS"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP "$DIR/$OPT_JS")
check_from "$OUT" fgrep -q a=0

# Clear out the file-cache.  Note that with a shared-memory cache, there is
# no L1 copy of the optimized or origin resource in the cache.  The optimized
# js will now 404.  This is not desired, and this testcase captures the bugs:
#     https://github.com/pagespeed/mod_pagespeed/issues/1145
#     https://github.com/pagespeed/ngx_pagespeed/issues/1319
echo removing "$CACHE_DIR"
ls -l "$CACHE_DIR"
rm -rf "$CACHE_DIR"
OUT=$(http_proxy=$SECONDARY_HOSTNAME \
    $CURL -o/dev/null -sS --write-out '%{http_code}\n' "$DIR/$OPT_JS")
check [ "$OUT" = "404" ]
