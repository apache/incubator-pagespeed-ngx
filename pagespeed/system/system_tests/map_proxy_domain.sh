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
start_test MapProxyDomain
# depends on MapProxyDomain in the config file
LEAF="proxy_external_resource.html?PageSpeedFilters=-inline_images"
URL="$EXAMPLE_ROOT/$LEAF"
echo Rewrite HTML with reference to a proxyable image.
fetch_until -save -recursive $URL 'grep -c 1.gif.pagespeed' 1 --save-headers
PAGESPEED_GIF=$(grep -o '/*1.gif.pagespeed[^"]*' $WGET_DIR/$LEAF)
check_from "$PAGESPEED_GIF" grep "gif$"

echo "If the next line fails, look in $WGET_DIR/wget_output.txt and you should"
echo "see a 404.  This represents a failed attempt to download the proxied gif."
# TODO(jefftk): debug why this test sometimes fails with the native fetcher in
# ngx_pagespeed.  https://github.com/pagespeed/ngx_pagespeed/issues/774
check test -f "$WGET_DIR$PAGESPEED_GIF"

# To make sure that we can reconstruct the proxied content by going back
# to the origin, we must avoid hitting the output cache.
# Note that cache-flushing does not affect the cache of rewritten resources;
# only input-resources and metadata.  To avoid hitting that cache and force
# us to rewrite the resource from origin, we grab this resource from a
# virtual host attached to a different cache.
if [ "$SECONDARY_HOSTNAME" != "" ]; then
  SECONDARY_HOST="$SECONDARY_ROOT/gstatic_images"
  PROXIED_IMAGE="$SECONDARY_HOST$PAGESPEED_GIF"
  start_test $PROXIED_IMAGE expecting one year cache.

  # With the proper hash, we'll get a long cache lifetime.
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save $PROXIED_IMAGE \
      "grep -c max-age=31536000" 1 --save-headers

  # With the wrong hash, we'll get a short cache lifetime (and also no output
  # cache hit.
  WRONG_HASH="0"
  PROXIED_IMAGE="$SECONDARY_HOST/1.gif.pagespeed.ce.$WRONG_HASH.jpg"
  start_test Fetching $PROXIED_IMAGE expecting short private cache.
  http_proxy=$SECONDARY_HOSTNAME fetch_until $PROXIED_IMAGE \
      "grep -c max-age=300,private" 1 --save-headers

  # Test fetching a pagespeed URL via a reverse proxy, with pagespeed loaded,
  # but disabled for the proxied domain. As reported in Issue 582 this used to
  # fail with a 403 (Forbidden).
  start_test Reverse proxy a pagespeed URL.

  PROXY_PATH="http://$PAGESPEED_TEST_HOST/mod_pagespeed_example/styles"
  ORIGINAL="${PROXY_PATH}/yellow.css"
  FILTERED="${PROXY_PATH}/A.yellow.css.pagespeed.cf.KM5K8SbHQL.css"

  # We should be able to fetch the original ...
  echo  http_proxy=$SECONDARY_HOSTNAME $WGET --save-headers -O - $ORIGINAL
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET --save-headers -O - $ORIGINAL 2>&1)
  check_200_http_response "$OUT"
  # ... AND the rewritten version.
  echo  http_proxy=$SECONDARY_HOSTNAME $WGET --save-headers -O - $FILTERED
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET --save-headers -O - $FILTERED 2>&1)
  check_200_http_response "$OUT"
fi

start_test proxying from external domain should optimize images in-place.
# Keep fetching this until it's headers include the string "PSA-aj" which
# means rewriting has finished.
URL="$PRIMARY_SERVER/modpagespeed_http/Puzzle.jpg"
fetch_until -save $URL "grep -c PSA-aj" 1 "--save-headers"

# We should see the origin etag in the wget output due to -save.  Note that
# the cache-control will start at 5 minutes -- the default on modpagespeed.com,
# and descend as time expires from when we strobed the image.  However, we
# provide a non-trivial etag with the content hash, but we'll just match the
# common prefix.
check_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" \
    fgrep -qi 'Etag: W/"PSA-aj-'

# Ideally this response should not have a 'chunked' encoding, because
# once we were able to optimize it, we know its length.
check_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" fgrep -q 'Content-Length:'
check_not_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" \
    fgrep -q 'Transfer-Encoding: chunked'

# Now add set jpeg compression to 75 and we expect 73238, but will test for 90k.
# Note that wc -c will include the headers.
start_test Proxying image from another domain, customizing image compression.
URL+="?PageSpeedJpegRecompressionQuality=75"
fetch_until -save $URL "wc -c" 90000 "--save-headers" "-lt"
check_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" \
    fgrep -qi 'Etag: W/"PSA-aj-'

echo Ensure that rewritten images strip cookies present at origin
check_not_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" fgrep -qi 'Set-Cookie'
$WGET -O $FETCH_UNTIL_OUTFILE --save-headers \
  http://$PAGESPEED_TEST_HOST/do_not_modify/Puzzle.jpg
ORIGINAL_HEADERS=$(extract_headers $FETCH_UNTIL_OUTFILE)
check_from "$ORIGINAL_HEADERS" fgrep -q -i 'Set-Cookie'

start_test proxying HTML from external domain should not work
URL="$PRIMARY_SERVER/modpagespeed_http/evil.html"
OUT=$(check_error_code 8 $WGET_DUMP $URL)
check_not_from "$OUT" fgrep -q 'Set-Cookie:'

start_test Fetching the HTML directly from the origin is fine including cookie.
URL="http://$PAGESPEED_TEST_HOST/do_not_modify/evil.html"
OUT=$($WGET_DUMP $URL)
check_from "$OUT" fgrep -q -i 'Set-Cookie: test-cookie'

start_test Ipro transcode to webp from MapProxyDomain
URL="$PRIMARY_SERVER/modpagespeed_http/Puzzle.jpg"
URL+="?PageSpeedFilters=+in_place_optimize_for_browser"
WGET_ARGS="--user-agent webp --header Accept:image/webp"
fetch_until "$URL" "grep -c image/webp" 1 --save-headers
URL=""
