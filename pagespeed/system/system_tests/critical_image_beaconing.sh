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
# Verify that we can send a critical image beacon and that lazyload_images
# does not try to lazyload the critical images.
start_test lazyload_images,rewrite_images with critical images beacon
HOST_NAME="http://imagebeacon.example.com"
URL="$HOST_NAME/mod_pagespeed_test/image_rewriting/rewrite_images.html"
# There are 3 images on rewrite_images.html.  Since beaconing is on but we've
# sent no beacon data, none should be lazy loaded.
# Run until we see beaconing on the page (should happen on first visit).
http_proxy=$SECONDARY_HOSTNAME\
  fetch_until -save $URL \
  'fgrep -c "pagespeed.CriticalImages.Run"' 1
check [ $(grep -c "data-pagespeed-lazy-src=" $FETCH_FILE) = 0 ];
# We need the options hash and nonce to send a critical image beacon, so
# extract it from injected beacon JS.
OPTIONS_HASH=$(
  awk -F\' '/^pagespeed\.CriticalImages\.Run/ {print $(NF-3)}' $FETCH_FILE)
NONCE=$(
  awk -F\' '/^pagespeed\.CriticalImages\.Run/ {print $(NF-1)}' $FETCH_FILE)
# Send a beacon response using POST indicating that Puzzle.jpg is a critical
# image.
BEACON_URL="$HOST_NAME/$BEACON_HANDLER"
BEACON_URL+="?url=http%3A%2F%2Fimagebeacon.example.com%2Fmod_pagespeed_test%2F"
BEACON_URL+="image_rewriting%2Frewrite_images.html"
BEACON_DATA="oh=$OPTIONS_HASH&n=$NONCE&ci=2932493096"

OUT=$(env http_proxy=$SECONDARY_HOSTNAME \
  $CURL -sSi -d  "$BEACON_DATA" "$BEACON_URL")
check_from "$OUT" egrep -q "HTTP/1[.]. 204"
# Now 2 of the images should be lazyloaded, Puzzle.jpg should not be.
http_proxy=$SECONDARY_HOSTNAME \
  fetch_until -save -recursive $URL 'fgrep -c data-pagespeed-lazy-src=' 2

# Now test sending a beacon with a GET request, instead of POST. Indicate that
# Puzzle.jpg and Cuppa.png are the critical images. In practice we expect only
# POSTs to be used by the critical image beacon, but both code paths are
# supported.  We add query params to URL to ensure that we get an instrumented
# page without blocking.
URL="$URL?id=4"
http_proxy=$SECONDARY_HOSTNAME\
  fetch_until -save $URL \
  'fgrep -c "pagespeed.CriticalImages.Run"' 1
check [ $(grep -c "data-pagespeed-lazy-src=" $FETCH_FILE) = 0 ];
OPTIONS_HASH=$(
  awk -F\' '/^pagespeed\.CriticalImages\.Run/ {print $(NF-3)}' $FETCH_FILE)
NONCE=$(
  awk -F\' '/^pagespeed\.CriticalImages\.Run/ {print $(NF-1)}' $FETCH_FILE)
BEACON_URL="$HOST_NAME/$BEACON_HANDLER"
BEACON_URL+="?url=http%3A%2F%2Fimagebeacon.example.com%2Fmod_pagespeed_test%2F"
BEACON_URL+="image_rewriting%2Frewrite_images.html%3Fid%3D4"
BEACON_DATA="oh=$OPTIONS_HASH&n=$NONCE&ci=2932493096"
# Add the hash for Cuppa.png to BEACON_DATA, which will be used as the query
# params for the GET.
BEACON_DATA+=",2644480723"
OUT=$(env http_proxy=$SECONDARY_HOSTNAME \
  $CURL -sSi "$BEACON_URL&$BEACON_DATA")
check_from "$OUT" egrep -q "HTTP/1[.]. 204"
# Now only BikeCrashIcn.png should be lazyloaded.
http_proxy=$SECONDARY_HOSTNAME \
  fetch_until -save -recursive $URL 'fgrep -c data-pagespeed-lazy-src=' 1
