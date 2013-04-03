#!/bin/bash
#
# Copyright 2012 Google Inc.
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
#
# Author: jefftk@google.com (Jeff Kaufman)
#
#
# Runs pagespeed's generic system test.  Will eventually run nginx-specific
# tests as well.
#
# Exits with status 0 if all tests pass.
# Exits with status 1 immediately if any test fails.
# Exits with status 2 if command line args are wrong.
#
# Usage:
#   ./ngx_system_test.sh primary_port secondary_port mod_pagespeed_dir
# Example:
#   ./ngx_system_test.sh 8050 8051 /path/to/mod_pagespeed
#

if [ "$#" -ne 4 ] ; then
  echo "Usage: $0 primary_port secondary_port mod_pagespeed_dir"
  echo "  nginx_executable"
  exit 2
fi

PRIMARY_PORT="$1"
SECONDARY_PORT="$2"
MOD_PAGESPEED_DIR="$3"
NGINX_EXECUTABLE="$4"

PRIMARY_HOSTNAME="localhost:$PRIMARY_PORT"
SECONDARY_HOSTNAME="localhost:$SECONDARY_PORT"

SERVER_ROOT="$MOD_PAGESPEED_DIR/src/install/"

# We need check and check_not before we source SYSTEM_TEST_FILE that provides
# them.
function handle_failure_simple() {
  echo "FAIL"
  exit 1
}
function check_simple() {
  echo "     check" "$@"
  "$@" || handle_failure_simple
}
function check_not_simple() {
  echo "     check_not" "$@"
  "$@" && handle_failure_simple
}

this_dir="$( cd $(dirname "$0") && pwd)"

# stop nginx
killall nginx

TEST_TMP="$this_dir/tmp"
rm -r "$TEST_TMP"
check_simple mkdir "$TEST_TMP"
FILE_CACHE="$TEST_TMP/file-cache/"
check_simple mkdir "$FILE_CACHE"
SECONDARY_CACHE="$TEST_TMP/file-cache/secondary/"
check_simple mkdir "$SECONDARY_CACHE"

# set up the config file for the test
PAGESPEED_CONF="$TEST_TMP/pagespeed_test.conf"
PAGESPEED_CONF_TEMPLATE="$this_dir/pagespeed_test.conf.template"
# check for config file template
check_simple test -e "$PAGESPEED_CONF_TEMPLATE"
# create PAGESPEED_CONF by substituting on PAGESPEED_CONF_TEMPLATE
echo > $PAGESPEED_CONF <<EOF
This file is automatically generated from $PAGESPEED_CONF_TEMPLATE"
by nginx_system_test.sh; don't edit here."
EOF
cat $PAGESPEED_CONF_TEMPLATE \
  | sed 's#@@TEST_TMP@@#'"$TEST_TMP/"'#' \
  | sed 's#@@FILE_CACHE@@#'"$FILE_CACHE/"'#' \
  | sed 's#@@SECONDARY_CACHE@@#'"$SECONDARY_CACHE/"'#' \
  | sed 's#@@SERVER_ROOT@@#'"$SERVER_ROOT"'#' \
  | sed 's#@@PRIMARY_PORT@@#'"$PRIMARY_PORT"'#' \
  | sed 's#@@SECONDARY_PORT@@#'"$SECONDARY_PORT"'#' \
  >> $PAGESPEED_CONF
# make sure we substituted all the variables
check_not_simple grep @@ $PAGESPEED_CONF

# start nginx with new config
check_simple "$NGINX_EXECUTABLE" -c "$PAGESPEED_CONF"

# run generic system tests
SYSTEM_TEST_FILE="$MOD_PAGESPEED_DIR/src/install/system_test.sh"

if [ ! -e "$SYSTEM_TEST_FILE" ] ; then
  echo "Not finding $SYSTEM_TEST_FILE -- is mod_pagespeed not in a parallel"
  echo "directory to ngx_pagespeed?"
  exit 2
fi

PSA_JS_LIBRARY_URL_PREFIX="ngx_pagespeed_static"

PAGESPEED_EXPECTED_FAILURES="
  ~compression is enabled for rewritten JS.~
  ~convert_meta_tags~
  ~insert_dns_prefetch~
  ~In-place resource optimization~
"

# The existing system test takes its arguments as positional parameters, and
# wants different ones than we want, so we need to reset our positional args.
set -- "$PRIMARY_HOSTNAME"
source $SYSTEM_TEST_FILE

# nginx-specific system tests

start_test Check for correct default X-Page-Speed header format.
OUT=$($WGET_DUMP $EXAMPLE_ROOT/combine_css.html)
check_from "$OUT" egrep -q \
  '^X-Page-Speed: [0-9]+[.][0-9]+[.][0-9]+[.][0-9]+-[0-9]+'

start_test pagespeed is defaulting to more than PassThrough
fetch_until $TEST_ROOT/bot_test.html 'grep -c \.pagespeed\.' 2

# Test that loopback route fetcher works with vhosts not listening on
# 127.0.0.1
start_test IP choice for loopback fetches.
HOST_NAME="loopbackfetch.example.com"
URL="$HOST_NAME/mod_pagespeed_example/rewrite_images.html"
http_proxy=127.0.0.2:$SECONDARY_PORT \
    fetch_until $URL 'grep -c .pagespeed.ic' 2

# When we allow ourself to fetch a resource because the Host header tells us
# that it is one of our resources, we should be fetching it from ourself.
start_test "Loopback fetches go to local IPs without DNS lookup"

# If we're properly fetching from ourself we will issue loopback fetches for
# /mod_pagespeed_example/combine_javascriptN.js, which will succeed, so
# combining will work.  If we're taking 'Host:www.google.com' to mean that we
# should fetch from www.google.com then those fetches will fail because
# google.com won't have /mod_pagespeed_example/combine_javascriptN.js and so
# we'll not rewrite any resources.

URL="$HOSTNAME/mod_pagespeed_example/combine_javascript.html"
URL+="?ModPagespeed=on&ModPagespeedFilters=combine_javascript"
fetch_until "$URL" "fgrep -c .pagespeed." 1 --header=Host:www.google.com

# If this accepts the Host header and fetches from google.com it will fail with
# a 404.  Instead it should use a loopback fetch and succeed.
URL="$HOSTNAME/mod_pagespeed_example/.pagespeed.ce.8CfGBvwDhH.css"
check wget -O /dev/null --header=Host:www.google.com "$URL"

test_filter combine_css combines 4 CSS files into 1.
fetch_until $URL 'grep -c text/css' 1
check run_wget_with_args $URL
test_resource_ext_corruption $URL $combine_css_filename

test_filter extend_cache rewrites an image tag.
fetch_until $URL 'grep -c src.*91_WewrLtP' 1
check run_wget_with_args $URL
echo about to test resource ext corruption...
test_resource_ext_corruption $URL images/Puzzle.jpg.pagespeed.ce.91_WewrLtP.jpg

test_filter outline_javascript outlines large scripts, but not small ones.
check run_wget_with_args $URL
check egrep -q '<script.*large.*src=' $FETCHED       # outlined
check egrep -q '<script.*small.*var hello' $FETCHED  # not outlined
start_test compression is enabled for rewritten JS.
JS_URL=$(egrep -o http://.*[.]pagespeed.*[.]js $FETCHED)
echo "JS_URL=\$\(egrep -o http://.*[.]pagespeed.*[.]js $FETCHED\)=\"$JS_URL\""
JS_HEADERS=$($WGET -O /dev/null -q -S --header='Accept-Encoding: gzip' \
  $JS_URL 2>&1)
echo JS_HEADERS=$JS_HEADERS
check_from "$JS_HEADERS" egrep -qi 'HTTP/1[.]. 200 OK'
check_from "$JS_HEADERS" fgrep -qi 'Content-Encoding: gzip'
check_from "$JS_HEADERS" fgrep -qi 'Vary: Accept-Encoding'
check_from "$JS_HEADERS" egrep -qi '(Etag: W/"0")|(Etag: W/"0-gzip")'
check_from "$JS_HEADERS" fgrep -qi 'Last-Modified:'

WGET_ARGS="" # Done with test_filter, so clear WGET_ARGS.

start_test Respect X-Forwarded-Proto when told to
FETCHED=$OUTDIR/x_forwarded_proto
URL=$SECONDARY_HOSTNAME/mod_pagespeed_example/?ModPagespeedFilters=add_base_tag
HEADERS="--header=X-Forwarded-Proto:https --header=Host:xfp.example.com"
check $WGET_DUMP -O $FETCHED $HEADERS $URL
# When enabled, we respect X-Forwarded-Proto and thus list base as https.
check fgrep -q '<base href="https://' $FETCHED

# Several cache flushing tests.

start_test Touching cache.flush flushes the cache.

# If we write fixed values into the css file here, there is a risk that
# we will end up seeing the 'right' value because an old process hasn't
# invalidated things yet, rather than because it updated to what we expect
# in the first run followed by what we expect in the second run.
# So, we incorporate the timestamp into RGB colors, using hours
# prefixed with 1 (as 0-123 fits the 0-255 range) to get a second value.
# A one-second precision is good enough since there is a sleep 2 below.
COLOR_SUFFIX=`date +%H,%M,%S\)`
COLOR0=rgb\($COLOR_SUFFIX
COLOR1=rgb\(1$COLOR_SUFFIX

# We test on three different cache setups:
#
#   1. A virtual host using the normal FileCachePath.
#   2. Another virtual host with a different FileCachePath.
#   3. Another virtual host with a different CacheFlushFilename.
#
# This means we need to repeat many of the steps three times.

echo "Clear out our existing state before we begin the test."
check touch "$FILE_CACHE/cache.flush"
check touch "$FILE_CACHE/othercache.flush"
check touch "$SECONDARY_CACHE/cache.flush"
sleep 1

CSS_FILE="$SERVER_ROOT/mod_pagespeed_test/update.css"
echo ".class myclass { color: $COLOR0; }" > "$CSS_FILE"

URL_PATH="mod_pagespeed_test/cache_flush_test.html"

URL="$SECONDARY_HOSTNAME/$URL_PATH"
CACHE_A="--header=Host:cache_a.example.com"
fetch_until $URL "grep -c $COLOR0" 1 $CACHE_A

CACHE_B="--header=Host:cache_b.example.com"
fetch_until $URL "grep -c $COLOR0" 1 $CACHE_B

CACHE_C="--header=Host:cache_c.example.com"
fetch_until $URL "grep -c $COLOR0" 1 $CACHE_C

# All three caches are now populated.

# TODO(jefftk): Check statistics here.  In apache_system_test.sh we can track
# reported flushed by looking at statistics, but this isn't ported to nginx
# yet.  Once that is ported, come back here and make sure it's correct.

# Now change the file to $COLOR1.
echo ".class myclass { color: $COLOR1; }" > "$CSS_FILE"

# We expect to have a stale cache for 5 minutes, so the result should stay
# $COLOR0.  This only works because we have only one worker process.  If we had
# more than one then the worker process handling this request might be different
# than the one that got the previous one, and it wouldn't be in cache.
OUT="$($WGET_DUMP $CACHE_A "$URL")"
check_from "$OUT" fgrep $COLOR0

OUT="$($WGET_DUMP $CACHE_B "$URL")"
check_from "$OUT" fgrep $COLOR0

OUT="$($WGET_DUMP $CACHE_C "$URL")"
check_from "$OUT" fgrep $COLOR0

# Flush the cache by touching a special file in the cache directory.  Now
# css gets re-read and we get $COLOR1 in the output.  Sleep here to avoid
# a race due to 1-second granularity of file-system timestamp checks.  For
# the test to pass we need to see time pass from the previous 'touch'.
#
# The three vhosts here all have CacheFlushPollIntervalSec set to 1.

sleep 2
check touch "$FILE_CACHE/cache.flush"
sleep 1

# Check that CACHE_A flushed properly.
fetch_until $URL "grep -c $COLOR1" 1 $CACHE_A

start_test Flushing one cache does not flush all caches.

# Check that CACHE_B and CACHE_C are still serving a stale version.
OUT="$($WGET_DUMP $CACHE_B "$URL")"
check_from "$OUT" fgrep $COLOR0

OUT="$($WGET_DUMP $CACHE_C "$URL")"
check_from "$OUT" fgrep $COLOR0

start_test Secondary caches also flush.

# Now flush the other two files so they can see the color change.
check touch "$FILE_CACHE/othercache.flush"
check touch "$SECONDARY_CACHE/cache.flush"
sleep 1

# Check that CACHE_B and C flushed properly.
fetch_until $URL "grep -c $COLOR1" 1 $CACHE_B
fetch_until $URL "grep -c $COLOR1" 1 $CACHE_C

# Clean up update.css from mod_pagespeed_test so it doesn't leave behind
# a stray file not under source control.
rm -f $CSS_FILE

# Make sure that when in PreserveURLs mode that we don't rewrite URLs. This is
# non-exhaustive, the unit tests should cover the rest.
# Note: We block with psatest here because this is a negative test.  We wouldn't
# otherwise know how many wget attempts should be made.
WGET_ARGS="--header=X-PSA-Blocking-Rewrite:psatest"
WGET_ARGS+=" --header=Host:preserveurls.example.com"

start_test PreserveURLs on prevents URL rewriting
FILE=preserveurls/on/preserveurls.html
URL=$SECONDARY_HOSTNAME/mod_pagespeed_test/$FILE
FETCHED=$OUTDIR/preserveurls.html
check run_wget_with_args $URL
WGET_ARGS=""
check_not fgrep -q .pagespeed. $FETCHED

# When PreserveURLs is off do a quick check to make sure that normal rewriting
# occurs.  This is not exhaustive, the unit tests should cover the rest.
start_test PreserveURLs off causes URL rewriting
WGET_ARGS="--header=Host:preserveurls.example.com"
FILE=preserveurls/off/preserveurls.html
URL=$SECONDARY_HOSTNAME/mod_pagespeed_test/$FILE
FETCHED=$OUTDIR/preserveurls.html
# Check that style.css was inlined.
fetch_until $URL 'egrep -c big.css.pagespeed.' 1
# Check that introspection.js was inlined.
fetch_until $URL 'grep -c document\.write(\"External' 1
# Check that the image was optimized.
fetch_until $URL 'grep -c BikeCrashIcn\.png\.pagespeed\.' 2

check_failures_and_exit
