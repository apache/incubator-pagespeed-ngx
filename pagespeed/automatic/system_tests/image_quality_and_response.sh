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
set -u
set -e

# Rewrites an image with the specified Save-Data header and Via header;
# verifies the content type, length, and Vary header of the response.
function ipro_rewrite_image_and_verify_response() {
  local HAS_SAVE_DATA=$1
  local HAS_VIA=$2
  local EXPECTED_CONTENT_TYPE=$3
  local EXPECTED_VARY=$4
  local EXPECTED_CONTENT_LENGTH=$5
  local SECONDARY_HOST="optimizeforbandwidth.example.com"
  local OPT="--save-headers --user-agent=$USER_AGENT"

  local URL
  if [ -z "${STATIC_DOMAIN:-}" ]; then
    URL="http://$HOST/$IMAGE"
  fi

  local TIME_OUT_STR=""
  if [ "$EXPECTED_CONTENT_LENGTH" = "$UNOPTIMIZED" ]; then
    TIME_OUT_STR="-expect_time_out"
  fi

  local TEST_ID="IPRO rewrite image $URL UA=$USER_AGENT"
  if [ "$ACCEPT_WEBP" = true ]; then
    OPT+=" --header=Accept:image/webp"
    TEST_ID+=" Accept:webp"
  fi
  if [ "$HAS_SAVE_DATA" = true ]; then
    OPT+=" --header=Save-Data:on"
    TEST_ID+=" Save-Data:on"
  fi
  if [ "$HAS_VIA" = true ]; then
    OPT+=" --header=Via:proxy"
    TEST_ID+=" Via:proxy"
  fi

  # Fetch the image until it's optimized or timed-out.
  start_test "$TEST_ID"
  http_proxy=$SECONDARY_HOSTNAME \
    fetch_until -save $TIME_OUT_STR $URL 'grep -c W/\"PSA-aj-' 1 "$OPT"

  local TYPE="$(extract_headers $FETCH_UNTIL_OUTFILE | \
    scrape_header 'Content-Type')"
  check [ $TYPE = $EXPECTED_CONTENT_TYPE ]
  # If the image can be optimized, content length is checked against a range
  # for accommodating image encoder version difference.
  local LENGTH="$(extract_headers $FETCH_UNTIL_OUTFILE | scrape_content_length)"
  if [ "$EXPECTED_CONTENT_LENGTH" != "$UNOPTIMIZED" ]; then
    local MIN_LENGTH=`expr $EXPECTED_CONTENT_LENGTH - 80`
    local MAX_LENGTH=`expr $EXPECTED_CONTENT_LENGTH + 80`
    check [ $LENGTH -ge $MIN_LENGTH ]
    check [ $LENGTH -le $MAX_LENGTH ]
  fi
  local VARY="$(extract_headers $FETCH_UNTIL_OUTFILE | scrape_header 'Vary')"
  if [ -z $EXPECTED_VARY ]; then
    check_not_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" grep -q "^Vary: "
  else
    check [ $VARY = $EXPECTED_VARY ]
  fi
}

# Rewrites an image using the specified user-agent. Checks the combination
# of with and without Save-Data header and Via header.
# With "Via", "AllowVaryOn: auto" implies "Save-Data,Accept".
# Without "Via", "AllowVaryOn: auto" implies "Save-Data,User-Agent".
function ipro_rewrite_image() {
  local HOST=$1
  local IMAGE=$2
  local USER_AGENT=$3
  local ACCEPT_WEBP=$4
  local EXPECTED_CONTENT_TYPE_VIA_YES=$5
  local EXPECTED_CONTENT_TYPE_VIA_NO=$6
  local EXPECTED_VARY_VIA_YES=$7
  local EXPECTED_VARY_VIA_NO=$8
  local EXPECTED_LENGTH_SD_NO_VIA_YES=$9
  local EXPECTED_LENGTH_SD_NO_VIA_NO=${10}
  local EXPECTED_LENGTH_SD_YES_VIA_YES=${11}
  local EXPECTED_LENGTH_SD_YES_VIA_NO=${12}
  # Constants
  local SAVE_DATA_YES=true
  local SAVE_DATA_NO=false
  local VIA_YES=true
  local VIA_NO=false
  # Save-Data: no, Via: yes
  ipro_rewrite_image_and_verify_response "$SAVE_DATA_NO" "$VIA_YES" \
    "$EXPECTED_CONTENT_TYPE_VIA_YES" "$EXPECTED_VARY_VIA_YES" \
    "$EXPECTED_LENGTH_SD_NO_VIA_YES"
  # Save-Data: no, Via: no
  ipro_rewrite_image_and_verify_response "$SAVE_DATA_NO" "$VIA_NO" \
    "$EXPECTED_CONTENT_TYPE_VIA_NO" "$EXPECTED_VARY_VIA_NO" \
    "$EXPECTED_LENGTH_SD_NO_VIA_NO"
  # Save-Data: yes, Via: yes
  ipro_rewrite_image_and_verify_response "$SAVE_DATA_YES" "$VIA_YES" \
    "$EXPECTED_CONTENT_TYPE_VIA_YES" "$EXPECTED_VARY_VIA_YES" \
    "$EXPECTED_LENGTH_SD_YES_VIA_YES"
  # Save-Data: yes, Via: no
  ipro_rewrite_image_and_verify_response "$SAVE_DATA_YES" "$VIA_NO" \
    "$EXPECTED_CONTENT_TYPE_VIA_NO" "$EXPECTED_VARY_VIA_NO" \
    "$EXPECTED_LENGTH_SD_YES_VIA_NO"
}

# Hosts
HOST_ALLOW_ACCEPT="ipro-for-browser.example.com"
HOST_ALLOW_AUTO="ipro-for-browser-vary-on-auto.example.com"
HOST_ALLOW_NONE="ipro-for-browser-vary-on-none.example.com"
# User-agents
CHROME_MOBILE="Mozilla*Android*Mobile*Chrome/44.*"
SAFARI_MOBILE="iPhone*Safari/8536.25"
FIREFOX_DESKTOP="Firefox/1.5"
# Images
# JPEG image which will be optimized to lossy format.
IMAGE_PUZZLE="images/Puzzle.jpg"
# PNG image which will be optimized to lossless format.
IMAGE_CUPPA="images/Cuppa.png"
# PNG image which will be optimized to lossy format.
IMAGE_BIKE="images/BikeCrashIcn.png"
# Animated GIF which can only be optimized to animated WebP.
IMAGE_ANIMATION="images/PageSpeedAnimationSmall.gif"
# Qualities overriding
UNSET_QUALITY=\
"?PageSpeedJpegQualityForSaveData=-1&PageSpeedWebpQualityForSaveData=-1"
SAME_QUALITY=\
"?PageSpeedJpegQualityForSaveData=75&PageSpeedWebpQualityForSaveData=70"
# Constants
ACCEPT_WEBP_YES=true
ACCEPT_WEBP_NO=false
UNOPTIMIZED=-1

# Allowed to vary on "Auto". Test 3 kinds of user-agents, on 3 quality levels
# for 4 kinds of images.
# JPEG image, optimized for Chrome on Android.
ipro_rewrite_image "$HOST_ALLOW_AUTO" "$IMAGE_PUZZLE" \
  "$CHROME_MOBILE" "$ACCEPT_WEBP_YES" "image/webp" "image/webp" \
  "Accept,Save-Data" "User-Agent,Save-Data" 33108 25774 19124 19124
# JPEG image, optimized for Safari on iOS.
ipro_rewrite_image "$HOST_ALLOW_AUTO" "$IMAGE_PUZZLE" \
  "$SAFARI_MOBILE" "$ACCEPT_WEBP_NO" "image/jpeg" "image/jpeg" \
  "Accept,Save-Data" "User-Agent,Save-Data" 73096 51452 38944 38944
# JPEG image, optimized for Firefox on desktop.
ipro_rewrite_image "$HOST_ALLOW_AUTO" "$IMAGE_PUZZLE" \
  "$FIREFOX_DESKTOP" "$ACCEPT_WEBP_NO" "image/jpeg" "image/jpeg" \
  "Accept,Save-Data" "User-Agent,Save-Data" 73096 73096 38944 38944
# Photographic PNG image, optimized for Chrome on Android.
ipro_rewrite_image "$HOST_ALLOW_AUTO" "$IMAGE_BIKE" \
  "$CHROME_MOBILE" "$ACCEPT_WEBP_YES" "image/webp" "image/webp" \
  "Accept,Save-Data" "User-Agent,Save-Data" 2454 2014 1476 1476
# Photographic PNG image, optimized for Safari on iOS.
ipro_rewrite_image "$HOST_ALLOW_AUTO" "$IMAGE_BIKE" \
  "$SAFARI_MOBILE" "$ACCEPT_WEBP_NO" "image/jpeg" "image/jpeg" \
  "Accept,Save-Data" "User-Agent,Save-Data" 3536 2606 2069 2069
# Photographic PNG image, optimized for Firefox on desktop.
ipro_rewrite_image "$HOST_ALLOW_AUTO" "$IMAGE_BIKE" \
  "$FIREFOX_DESKTOP" "$ACCEPT_WEBP_NO" "image/jpeg" "image/jpeg" \
  "Accept,Save-Data" "User-Agent,Save-Data" 3536 3536 2069 2069
# Non-photographic PNG image, optimized for Chrome on Android.
ipro_rewrite_image "$HOST_ALLOW_AUTO" "$IMAGE_CUPPA" \
  "$CHROME_MOBILE" "$ACCEPT_WEBP_YES" "image/png" "image/webp" \
  "" "User-Agent" 770 694 770 694
# Non-photographic PNG image, optimized for Safari on iOS.
ipro_rewrite_image "$HOST_ALLOW_AUTO" "$IMAGE_CUPPA" \
  "$SAFARI_MOBILE" "$ACCEPT_WEBP_NO" "image/png" "image/png" \
  "" "User-Agent" 770 770 770 770
# Non-photographic PNG image, optimized for Firefox on desktop.
ipro_rewrite_image "$HOST_ALLOW_AUTO" "$IMAGE_CUPPA" \
  "$FIREFOX_DESKTOP" "$ACCEPT_WEBP_NO" "image/png" "image/png" \
  "" "User-Agent" 770 770 770 770
# Animated GIF image, optimized for Chrome on Android.
ipro_rewrite_image "$HOST_ALLOW_AUTO" "$IMAGE_ANIMATION" \
  "$CHROME_MOBILE" "$ACCEPT_WEBP_YES" "image/gif" "image/webp" \
  "" "User-Agent,Save-Data" "$UNOPTIMIZED" 6122 "$UNOPTIMIZED" 3036
# Animated GIF image, optimized for Safari on iOS.
ipro_rewrite_image "$HOST_ALLOW_AUTO" "$IMAGE_ANIMATION" \
  "$SAFARI_MOBILE" "$ACCEPT_WEBP_NO" "image/gif" "image/gif" \
  "" "" "$UNOPTIMIZED" "$UNOPTIMIZED" "$UNOPTIMIZED" "$UNOPTIMIZED"
# Animated GIF image, optimized for Firefox on desktop.
ipro_rewrite_image "$HOST_ALLOW_AUTO" "$IMAGE_ANIMATION" \
  "$FIREFOX_DESKTOP" "$ACCEPT_WEBP_NO" "image/gif" "image/gif" \
  "" "" "$UNOPTIMIZED" "$UNOPTIMIZED" "$UNOPTIMIZED" "$UNOPTIMIZED"

# Only allow to vary on Accept, JPEG image is optimized to desktop quality.
ipro_rewrite_image "$HOST_ALLOW_ACCEPT" "$IMAGE_PUZZLE" \
  "$CHROME_MOBILE" "$ACCEPT_WEBP_YES" "image/webp" "image/webp" \
  "Accept" "Accept" 33108 33108 33108 33108
# Only allow to vary on Accept, photographic PNG image is optimized to PNG and
# Vary header is not added.
ipro_rewrite_image "$HOST_ALLOW_ACCEPT" "$IMAGE_CUPPA" \
  "$SAFARI_MOBILE" "$ACCEPT_WEBP_YES" "image/png" "image/png" \
  "" "" 770 770 770 770
# Only allow to vary on Accept, animated image cannot be optimized.
ipro_rewrite_image "$HOST_ALLOW_ACCEPT" "$IMAGE_ANIMATION" \
  "$SAFARI_MOBILE" "$ACCEPT_WEBP_YES" "image/gif" "image/gif" \
  "" "" "$UNOPTIMIZED" "$UNOPTIMIZED" "$UNOPTIMIZED" "$UNOPTIMIZED"
# Nothing is allowed to vary on, JPEG image is optimized to desktop quality and
# Vary header is not added.
ipro_rewrite_image "$HOST_ALLOW_NONE" "$IMAGE_PUZZLE" \
  "$CHROME_MOBILE" "$ACCEPT_WEBP_YES" "image/jpeg" "image/jpeg" \
  "" "" 73096 73096 73096 73096
# When quality for Save-Data was not set, Vary header does not include
# "Save-Data".
ipro_rewrite_image "$HOST_ALLOW_AUTO" "$IMAGE_PUZZLE$UNSET_QUALITY" \
  "$CHROME_MOBILE" "$ACCEPT_WEBP_YES" "image/webp" "image/webp" \
  "Accept" "User-Agent" 33108 25774 33108 25774
# When quality for Save-Data was set to the same for desktop, Vary header does
# not include "Save-Data".
ipro_rewrite_image "$HOST_ALLOW_AUTO" "$IMAGE_PUZZLE$SAME_QUALITY" \
  "$CHROME_MOBILE" "$ACCEPT_WEBP_YES" "image/webp" "image/webp" \
  "Accept" "User-Agent" 33108 25774 33108 25774
