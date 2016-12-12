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
start_test Signed Urls : Correct URL signature is passed
URL_PATH="/mod_pagespeed_test/unauthorized/inline_css.html"
OPTS="?PageSpeedFilters=rewrite_images,rewrite_css"
FETCH_GREP='fgrep -c all_styles.css.pagespeed.cf'
URL_REGEX="http:\/\/[^[:space:]]+css\.pagespeed[^[:space:]]+\.css"
COMBINED_CSS=".yellow{background-color:#ff0}"
# Constants used in the next few tests.
URL="$(generate_url "signed-urls.example.com" $URL_PATH$OPTS)"
http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
    'fgrep -c all_styles.css.pagespeed.cf' 1
URL="$(grep -Eo "$URL_REGEX" $FETCH_FILE)"
check test -n "$URL"
echo wget $URL
http_proxy=$SECONDARY_HOSTNAME fetch_until $URL \
  "fgrep -c $COMBINED_CSS" 1

start_test Signed Urls : Incorrect URL signature is passed
# Substring, all but last 14 chars (signature and extension).
URL=$(echo "$URL" | sed 's/.\{14\}$//')
FINAL_URL="${URL}AAAAAAAAAA.css"
echo http_proxy=$SECONDARY_HOSTNAME wget $FINAL_URL
OUT="$(http_proxy=$SECONDARY_HOSTNAME check_not $WGET $FINAL_URL -O - 2>&1)"
check_from "$OUT" egrep -q "403 Forbidden|404 Not Found"

start_test Signed Urls : No signature is passed
FINAL_URL="$URL.css"
echo http_proxy=$SECONDARY_HOSTNAME wget $FINAL_URL
OUT="$(http_proxy=$SECONDARY_HOSTNAME check_not $WGET $FINAL_URL -O - 2>&1)"
check_from "$OUT" egrep -q "403 Forbidden|404 Not Found"

start_test Signed Urls, ignored signature : Correct URL signature is passed
URL="$(generate_url "signed-urls-transition.example.com" $URL_PATH$OPTS)"
http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
    'fgrep -c all_styles.css.pagespeed.cf' 1
URL="$(grep -Eo "$URL_REGEX" $FETCH_FILE)"
check test -n "$URL"
echo http_proxy=$SECONDARY_HOSTNAME wget $URL
http_proxy=$SECONDARY_HOSTNAME fetch_until $URL \
  "fgrep -c $COMBINED_CSS" 1

start_test Signed Urls, ignored signatures : Incorrect URL signature is passed
# Substring, all but last 14 chars (signature and extension).
URL=$(echo "$URL" | sed 's/.\{14\}$//')
FINAL_URL="${URL}AAAAAAAAAA.css"
http_proxy=$SECONDARY_HOSTNAME fetch_until $FINAL_URL \
  "fgrep -c $COMBINED_CSS" 1

start_test Signed Urls, ignored signatures : No signature is passed
FINAL_URL="$URL.css"
http_proxy=$SECONDARY_HOSTNAME fetch_until $FINAL_URL \
  "fgrep -c $COMBINED_CSS" 1

start_test Unsigned Urls, ignored signature : URL with bad signature is passed
# Change the domain from signed-urls.example.com to unsigned-urls.example.com.
URL="$(echo $URL | sed -e 's/signed-urls/unsigned-urls/')"
# And change the hash without a signature to a hash with an invalid signature.
URL="$(echo $URL | sed -e 's/Cxc4pzojlP/UH8L-zY4b4AAAAAAAAAA/')"
FINAL_URL="$URL.css"
echo http_proxy=$SECONDARY_HOSTNAME wget $FINAL_URL
http_proxy=$SECONDARY_HOSTNAME fetch_until $FINAL_URL \
  "fgrep -c $COMBINED_CSS" 1

start_test Unsigned Urls, ignored signatures : no signature is passed
URL="$(echo $URL | sed -e 's/AAAAAAAAAA//')"  # Remove signature.
FINAL_URL="$URL.css"
echo http_proxy=$SECONDARY_HOSTNAME wget $FINAL_URL
http_proxy=$SECONDARY_HOSTNAME fetch_until $FINAL_URL \
  "fgrep -c $COMBINED_CSS" 1
