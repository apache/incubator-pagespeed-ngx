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
# Optimize in-place images for browser. Ideal test matrix (not covered yet):
# User-Agent:  Accept:  Image type   Result
# -----------  -------  ----------   ----------------------------------
#    IE         N/A     photo        image/jpeg, Cache-Control: private *
#     :         N/A     synthetic    image/png,  no vary
#  Old Opera     no     photo        image/jpeg, Vary: Accept
#     :          no     synthetic    image/png,  no vary
#     :         webp    photo        image/webp, Vary: Accept, Lossy
#     :         webp    synthetic    image/png,  no vary
#  Chrome or     no     photo        image/jpeg, Vary: Accept
# Firefox or     no     synthetic    image/png,  no vary
#  New Opera    webp    photo        image/webp, Vary: Accept, Lossy
#     :         webp    synthetic    image/png,  no vary
# TODO(jmaessen): * cases currently send Vary: Accept.  Fix (in progress).
# TODO(jmaessen): Send image/webp lossless for synthetic and alpha-channel
# images.  Will require reverting to Vary: Accept for these.  Stuff like
# animated webp will have to remain unconverted still in IPRO mode, or switch
# to cc: private, but right now animated webp support is still pending anyway.
function test_ipro_for_browser_webp() {
  IN_UA_PRETTY="$1"; shift
  IN_UA="$1"; shift
  IN_ACCEPT="$1"; shift
  IMAGE_TYPE="$1"; shift
  OUT_CONTENT_TYPE="$1"; shift
  OUT_VARY="${1-}"; shift || true
  OUT_CC="${1-}"; shift || true

  # Remaining args are the expected headers (Name:Value), photo, or synthetic.
  if [ "$IMAGE_TYPE" = "photo" ]; then
    URL="http://ipro-for-browser.example.com/images/Puzzle.jpg"
  else
    URL="http://ipro-for-browser.example.com/images/Cuppa.png"
  fi
  TEST_ID="In-place optimize for "
  TEST_ID+="User-Agent:${IN_UA_PRETTY:-${IN_UA:-None}},"
  if [ -z "$IN_ACCEPT" ]; then
    TEST_ID+=" no accept, "
  else
    TEST_ID+=" Accept:$IN_ACCEPT, "
  fi
  TEST_ID+=" $IMAGE_TYPE.  Expect image/${OUT_CONTENT_TYPE}, "
  if [ -z "$OUT_VARY" ]; then
    TEST_ID+=" no vary, "
  else
    TEST_ID+=" Vary:${OUT_VARY}, "
  fi
  if [ -z "$OUT_CC" ]; then
    TEST_ID+=" cacheable."
  else
    TEST_ID+=" Cache-Control:${OUT_CC}."
  fi
  start_test $TEST_ID
  http_proxy=$SECONDARY_HOSTNAME \
    fetch_until -save $URL 'grep -c W/\"PSA-aj-' 1 \
        "--save-headers \
        ${IN_UA:+--user-agent $IN_UA} \
        ${IN_ACCEPT:+--header=Accept:image/$IN_ACCEPT}"
  check_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" \
    fgrep -q "Content-Type: image/$OUT_CONTENT_TYPE"
  if [ -z "$OUT_VARY" ]; then
    check_not_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" \
      fgrep -q "Vary:"
  else
    check_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" \
      fgrep -q "Vary: $OUT_VARY"
  fi
  check_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" \
    grep -q "Cache-Control: ${OUT_CC:-max-age=[0-9]*}$"
  # TODO(jmaessen): check file type of webp.  Irrelevant for now.
}

##############################################################################
# Test with testing-only user agent strings.
#                          UA           Accept Type  Out  Vary     CC
test_ipro_for_browser_webp "None" ""    ""     photo jpeg "Accept"
test_ipro_for_browser_webp "" "webp"    ""     photo jpeg "Accept"
test_ipro_for_browser_webp "" "webp-la" ""     photo jpeg "Accept"
test_ipro_for_browser_webp "None" ""    "webp" photo webp "Accept"
test_ipro_for_browser_webp "" "webp"    "webp" photo webp "Accept"
test_ipro_for_browser_webp "" "webp-la" "webp" photo webp "Accept"
test_ipro_for_browser_webp "None" ""    ""     synth png
test_ipro_for_browser_webp "" "webp"    ""     synth png
test_ipro_for_browser_webp "" "webp-la" ""     synth png
test_ipro_for_browser_webp "None" ""    "webp" synth png
test_ipro_for_browser_webp "" "webp"    "webp" synth png
test_ipro_for_browser_webp "" "webp-la" "webp" synth png
##############################################################################

# Wordy UAs need to be stored in the WGETRC file to avoid death by quoting.
OLD_WGETRC=$WGETRC
WGETRC=$TESTTMP/wgetrc-ua
export WGETRC

# IE 9 and later must re-validate Vary: Accept.  We should send CC: private.
IE9_UA="Mozilla/5.0 (Windows; U; MSIE 9.0; WIndows NT 9.0; en-US))"
IE11_UA="Mozilla/5.0 (Windows NT 6.1; WOW64; ***********; rv:11.0) like Gecko"
echo "user_agent = $IE9_UA" > $WGETRC
#                           (no accept)  Type  Out  Vary CC
test_ipro_for_browser_webp "IE 9"  "" "" photo jpeg ""   "max-age=[0-9]*,private"
test_ipro_for_browser_webp "IE 9"  "" "" synth png
echo "user_agent = $IE11_UA" > $WGETRC
test_ipro_for_browser_webp "IE 11" "" "" photo jpeg ""   "max-age=[0-9]*,private"
test_ipro_for_browser_webp "IE 11" "" "" synth png

# Older Opera did not support webp.
OPERA_UA="Opera/9.80 (Windows NT 5.2; U; en) Presto/2.7.62 Version/11.01"
echo "user_agent = $OPERA_UA" > $WGETRC
#                                (no accept) Type  Out  Vary
test_ipro_for_browser_webp "Old Opera" "" "" photo jpeg "Accept"
test_ipro_for_browser_webp "Old Opera" "" "" synth png
# Slightly newer opera supports only lossy webp, sends header.
OPERA_UA="Opera/9.80 (Windows NT 6.0; U; en) Presto/2.8.99 Version/11.10"
echo "user_agent = $OPERA_UA" > $WGETRC
#                                           Accept Type  Out  Vary
test_ipro_for_browser_webp "Newer Opera" "" "webp" photo webp "Accept"
test_ipro_for_browser_webp "Newer Opera" "" "webp" synth png

function test_decent_browsers() {
  echo "user_agent = $2" > $WGETRC
  #                          UA      Accept Type      Out  Vary
  test_ipro_for_browser_webp "$1" "" ""     photo     jpeg "Accept"
  test_ipro_for_browser_webp "$1" "" ""     synthetic  png
  test_ipro_for_browser_webp "$1" "" "webp" photo     webp "Accept"
  test_ipro_for_browser_webp "$1" "" "webp" synthetic  png
}
CHROME_UA="Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9_1) AppleWebKit/537.36 "
CHROME_UA+="(KHTML, like Gecko) Chrome/32.0.1700.102 Safari/537.36"
test_decent_browsers "Chrome" "$CHROME_UA"
FIREFOX_UA="Mozilla/5.0 (X11; U; Linux x86_64; zh-CN; rv:1.9.2.10) "
FIREFOX_UA+="Gecko/20100922 Ubuntu/10.10 (maverick) Firefox/3.6.10"
test_decent_browsers "Firefox" "$FIREFOX_UA"
test_decent_browsers "New Opera" \
  "Opera/9.80 (Windows NT 6.0) Presto/2.12.388 Version/12.14"

WGETRC=$OLD_WGETRC
