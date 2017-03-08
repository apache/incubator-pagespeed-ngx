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
test_filter inline_css converts 3 out of 5 link tags to style tags.
fetch_until $URL 'grep -c <style' 3

# In some test environments these tests can't be run because of
# restrictions on external connections
if [ -z ${DISABLE_FONT_API_TESTS:-} ]; then
  test_filter inline_google_font_css Can inline Google Font API loader CSS
  # Use a more recent version of Chrome UA than our default, which will get
  # a very large (which hit our previous default size limits) CSS using woff2
  WGETRC_OLD=$WGETRC
  export WGETRC=$TESTTMP/wgetrc-chrome
  cat > $WGETRC <<EOF
user_agent =Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.36 Safari/537.36
EOF

  fetch_until -save $URL 'grep -c @font-face' 7 "" -ge
  OUT=$(cat $FETCH_UNTIL_OUTFILE)
  check_from "$OUT" fgrep -qi "format('woff2')"
  check_not_from "$OUT" fgrep -qi "format('truetype')"
  check_not_from "$OUT" fgrep -qi "format('embedded-opentype')"
  check_not_from "$OUT" fgrep -qi ".ttf"
  check_not_from "$OUT" fgrep -qi ".eot"

  # Now try with IE6 user-agent. We do this with setting WGETRC due to
  # quoting madness
  export WGETRC=$TESTTMP/wgetrc-ie
  cat > $WGETRC <<EOF
user_agent = Mozilla/4.0 (compatible; MSIE 6.01; Windows NT 6.0)
EOF

  fetch_until -save $URL 'grep -c @font-face' 1
  # This should get an eot font. (It might also ship a woff, so we don't
  # check_not_from for that)
  OUT=$(cat $FETCH_UNTIL_OUTFILE)
  check_from "$OUT" fgrep -qi ".eot"
  check_not_from "$OUT" fgrep -qi ".ttf"

  # And now IE11.
  export WGETRC=$TESTTMP/wgetrc-ie11
  cat > $WGETRC <<EOF
user_agent = Mozilla/5.0 (Windows NT 6.1; WOW64; Trident/7.0; rv:11.0) like Gecko
EOF
  # This should get a woff font. (We used to confuse things so that it would
  # produce ttf).
  fetch_until $URL 'grep -c @font-face' 1
  OUT=$($WGET_DUMP $URL)
  check_from "$OUT" fgrep -qi ".woff"
  check_not_from "$OUT" fgrep -qi ".ttf"

  export WGETRC=$WGETRC_OLD
fi

test_filter inline_javascript inlines a small JS file.
fetch_until $URL 'grep -c document.write' 1

start_test inlining gzip-encoded resources
# If a resource is double-gzipped, or gzipped once but missing the headers,
# we need to not inline the compressed (binary) version.
#
# compressed.css and compressed.js are gzipped on disk, but small enough that
# PageSpeed would inline them if it were allowed to.  So fetch the page until we
# see two .pagespeed. resources, then verify that we see the debug comments we
# expect to see.
URL="$TEST_ROOT/gzip_precompressed/?PageSpeedFilters=+debug"
fetch_until -save $URL 'fgrep -c .pagespeed.' 2

OUT=$(cat $FETCH_UNTIL_OUTFILE)
# First verify that the inliners are actually enabled.
check_from "$OUT" fgrep "Inline Javascript"
check_from "$OUT" fgrep "Inline Css"
# Then check for the debug comments.
check_from "$OUT" fgrep "JS not inlined because it appears to be gzip-encoded"
check_from "$OUT" fgrep "CSS not inlined because it appears to be gzip-encoded"
