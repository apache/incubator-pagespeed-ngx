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
# Runs pagespeed's generic system test and nginx-specific system tests.  Not
# intended to be run on it's own; use run_tests.sh instead.
#
# Exits with status 0 if all tests pass.
# Exits with status 1 immediately if any test fails.
# Exits with status 2 if command line args are wrong.
# Exits with status 3 if all failures were expected.
# Exits with status 4 if instructed not to run any tests.

# Inherits the following from environment variables:
: ${USE_VALGRIND:?"Set USE_VALGRIND to true or false"}
: ${NATIVE_FETCHER:?"Set NATIVE_FETCHER to off or on"}
: ${PRIMARY_PORT:?"Set PRIMARY_PORT"}
: ${SECONDARY_PORT:?"Set SECONDARY_PORT"}
: ${RCPORT1:?"Set RCPORT1"}
: ${RCPORT2:?"Set RCPORT2"}
: ${RCPORT2:?"Set RCPORT3"}
: ${RCPORT4:?"Set RCPORT4"}
: ${RCPORT5:?"Set RCPORT5"}
: ${RCPORT6:?"Set RCPORT6"}
: ${RCPORT7:?"Set RCPORT7"}
: ${MOD_PAGESPEED_DIR:?"Set MOD_PAGESPEED_DIR"}
: ${NGINX_EXECUTABLE:?"Set NGINX_EXECUTABLE"}
: ${PAGESPEED_TEST_HOST:?"Set PAGESPEED_TEST_HOST"}
POSITION_AUX="${POSITION_AUX:-unset}"

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

# Argument list:
# host_name, path, post-data
# Runs 5 keepalive requests both with and without gzip for a few times.
# Curl will use keepalive when running multiple request with one command.
# When post-data is empty, a get request will be executed.
function keepalive_test() {
  HOST_NAME=$1
  URL="$SECONDARY_HOSTNAME$2"
  CURL_LOG_FILE="$1.curl.log"
  NGX_LOG_FILE="$1.error.log"
  POST_DATA=$3

  for ((i=0; i < 10; i++)); do
    for accept_encoding in "" "gzip"; do
      if [ -z "$POST_DATA" ]; then
        curl -m 2 -S -s -v -H "Accept-Encoding: $accept_encoding" \
          -H "Host: $HOST_NAME" $URL $URL $URL $URL $URL > /dev/null \
          2>>"$TEST_TMP/$CURL_LOG_FILE" || true
      else
        curl -X POST --data "$POST_DATA" -m 2 -S -s -v \
          -H "Accept-Encoding: $accept_encoding" -H "Host: $HOST_NAME"\
          $URL $URL $URL $URL $URL > /dev/null \
          2>>"$TEST_TMP/$CURL_LOG_FILE" || true
      fi
    done
  done

  # Filter the curl output from unimportant messages
  OUT=$(cat "$TEST_TMP/$CURL_LOG_FILE"\
    | grep -v "^[<>]"\
    | grep -v "^{ \\[data not shown"\
    | grep -v "^\\* About to connect"\
    | grep -v "^\\* Closing"\
    | grep -v "^\\* Connected to"\
    | grep -v "^\\* Re-using"\
    | grep -v "^\\* Connection.*left intact"\
    | grep -v "^} \\[data not shown"\
    | grep -v "^\\* upload completely sent off"\
    | grep -v "^\\* Found bundle for host"\
    | grep -v "^\\* connected"\
    | grep -v "^\\* Found bundle for host"\
    | grep -v "^\\* Adding handle"\
    | grep -v "^\\* Curl_addHandleToPipeline"\
    | grep -v "^\\* - Conn "\
    | grep -v "^\\* Server "\
    | grep -v "^\\*   Trying.*\\.\\.\\."\
    | grep -v "^\\* Hostname was NOT found in DNS cache" \
    || true)

  # Nothing should remain after that.
  check [ -z "$OUT" ]

  # Filter the nginx log from our vhost from unimportant messages.
  OUT=$(cat "$TEST_TMP/$NGX_LOG_FILE"\
    | grep -v "closed keepalive connection$" \
    | grep -v ".*Cache Flush.*" \
    || true)

  # Nothing should remain after that.
  check [ -z "$OUT" ]
}

function fire_ab_load() {
  AB_PID="0"
  if hash ab 2>/dev/null; then
    ab -n 10000 -k -c 100 http://$PRIMARY_HOSTNAME/ &>/dev/null & AB_PID=$!
    # Sleep to allow some queueing up of requests
  else
    echo "ab is not available, not able to test stressed shutdown and reload."
  fi
  sleep 2
 }

this_dir="$( cd $(dirname "$0") && pwd)"

# stop nginx/valgrind
killall -s KILL nginx
# TODO(oschaaf): Fix waiting for valgrind on 32 bits systems.
killall -s KILL memcheck-amd64-
while pgrep nginx > /dev/null; do sleep 1; done
while pgrep memcheck > /dev/null; do sleep 1; done

TEST_TMP="$this_dir/tmp"
rm -r "$TEST_TMP"
check_simple mkdir -p "$TEST_TMP"
PROXY_CACHE="$TEST_TMP/proxycache"
TMP_PROXY_CACHE="$TEST_TMP/tmpproxycache"
ERROR_LOG="$TEST_TMP/error.log"
ACCESS_LOG="$TEST_TMP/access.log"

# Check that we do ok with directories that already exist.
FILE_CACHE="$TEST_TMP/file-cache"
check_simple mkdir "$FILE_CACHE"

# And directories that don't.
SECONDARY_CACHE="$TEST_TMP/file-cache/secondary/"
IPRO_CACHE="$TEST_TMP/file-cache/ipro/"
SHM_CACHE="$TEST_TMP/file-cache/intermediate/directories/with_shm/"

VALGRIND_OPTIONS=""

if $USE_VALGRIND; then
  DAEMON=off
else
  DAEMON=on
fi

if [ "$NATIVE_FETCHER" = "on" ]; then
  RESOLVER="resolver 8.8.8.8;"
else
  RESOLVER=""
fi

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
  | sed 's#@@DAEMON@@#'"$DAEMON"'#' \
  | sed 's#@@TEST_TMP@@#'"$TEST_TMP/"'#' \
  | sed 's#@@PROXY_CACHE@@#'"$PROXY_CACHE/"'#' \
  | sed 's#@@TMP_PROXY_CACHE@@#'"$TMP_PROXY_CACHE/"'#' \
  | sed 's#@@ERROR_LOG@@#'"$ERROR_LOG"'#' \
  | sed 's#@@ACCESS_LOG@@#'"$ACCESS_LOG"'#' \
  | sed 's#@@FILE_CACHE@@#'"$FILE_CACHE/"'#' \
  | sed 's#@@SECONDARY_CACHE@@#'"$SECONDARY_CACHE/"'#' \
  | sed 's#@@IPRO_CACHE@@#'"$IPRO_CACHE/"'#' \
  | sed 's#@@SHM_CACHE@@#'"$SHM_CACHE/"'#' \
  | sed 's#@@SERVER_ROOT@@#'"$SERVER_ROOT"'#' \
  | sed 's#@@PRIMARY_PORT@@#'"$PRIMARY_PORT"'#' \
  | sed 's#@@SECONDARY_PORT@@#'"$SECONDARY_PORT"'#' \
  | sed 's#@@NATIVE_FETCHER@@#'"$NATIVE_FETCHER"'#' \
  | sed 's#@@RESOLVER@@#'"$RESOLVER"'#' \
  | sed 's#@@RCPORT1@@#'"$RCPORT1"'#' \
  | sed 's#@@RCPORT2@@#'"$RCPORT2"'#' \
  | sed 's#@@RCPORT3@@#'"$RCPORT3"'#' \
  | sed 's#@@RCPORT4@@#'"$RCPORT4"'#' \
  | sed 's#@@RCPORT5@@#'"$RCPORT5"'#' \
  | sed 's#@@RCPORT6@@#'"$RCPORT6"'#' \
  | sed 's#@@RCPORT7@@#'"$RCPORT7"'#' \
  | sed 's#@@PAGESPEED_TEST_HOST@@#'"$PAGESPEED_TEST_HOST"'#' \
  >> $PAGESPEED_CONF
# make sure we substituted all the variables
check_not_simple grep @@ $PAGESPEED_CONF

# start nginx with new config
if $USE_VALGRIND; then
  (valgrind -q --leak-check=full --gen-suppressions=all \
            --show-possibly-lost=no --log-file=$TEST_TMP/valgrind.log \
            --suppressions="$this_dir/valgrind.sup" \
      $NGINX_EXECUTABLE -c $PAGESPEED_CONF) & VALGRIND_PID=$!
  trap "echo 'terminating valgrind!' && kill -s sigterm $VALGRIND_PID" EXIT
  echo "Wait until nginx is ready to accept connections"
  while ! curl -I "http://$PRIMARY_HOSTNAME/mod_pagespeed_example/" 2>/dev/null; do
      sleep 0.1;
  done
  echo "Valgrind (pid:$VALGRIND_PID) is logging to $TEST_TMP/valgrind.log"
else
  TRACE_FILE="$TEST_TMP/conf_loading_trace"
  $NGINX_EXECUTABLE -c $PAGESPEED_CONF >& "$TRACE_FILE"
  if [[ $? -ne 0 ]]; then
    echo "FAIL"
    cat $TRACE_FILE
    if [[ $(grep -c "unknown directive \"proxy_cache_purge\"" $TRACE_FILE) == 1 ]]; then
      echo "This test requires proxy_cache_purge. One way to do this:"
      echo "Run git clone https://github.com/FRiCKLE/ngx_cache_purge.git"
      echo "And compile nginx with the additional ngx_cache_purge module."
    fi
    rm $TRACE_FILE
    exit 1
  fi
fi

# Helper methods used by downstream caching tests.

# Helper method that does a wget and verifies that the rewriting status matches
# the $1 argument that is passed to this method.
check_rewriting_status() {
  $WGET $WGET_ARGS $CACHABLE_HTML_LOC > $OUT_CONTENTS_FILE
  if $1; then
    check zgrep -q "pagespeed.ic" $OUT_CONTENTS_FILE
  else
    check_not zgrep -q "pagespeed.ic" $OUT_CONTENTS_FILE
  fi
}

# Helper method that obtains a gzipped response and verifies that rewriting
# has happened. Also takes an extra parameter that identifies extra headers
# to be added during wget.
check_for_rewriting() {
  WGET_ARGS="$GZIP_WGET_ARGS $1" check_rewriting_status true
}

# Helper method that obtains a gzipped response and verifies that no rewriting
# has happened. Also takes an extra parameter that identifies extra headers
# to be added during wget.
check_for_no_rewriting() {
  WGET_ARGS="$GZIP_WGET_ARGS $1" check_rewriting_status false
}

if $RUN_TESTS; then
  echo "Starting tests"
else
  if $USE_VALGRIND; then
    # Clear valgrind trap
    trap - EXIT
    echo "To end valgrind, run 'kill -s quit $VALGRIND_PID'"
  fi
  echo "Not running tests; commence manual testing"
  exit 4
fi

# check_stat in system_test_helpers.sh needs to know whether statstistics are
# enabled, which is always the case for ngx_pagespeed.
statistics_enabled=1
CACHE_FLUSH_TEST="on"
CACHE_PURGE_METHODS="PURGE GET"

SERVER_NAME=nginx

# run generic system tests
SYSTEM_TEST_FILE="$MOD_PAGESPEED_DIR/src/pagespeed/system/system_test.sh"

if [ ! -e "$SYSTEM_TEST_FILE" ] ; then
  echo "Not finding $SYSTEM_TEST_FILE -- is mod_pagespeed not in a parallel"
  echo "directory to ngx_pagespeed?"
  exit 2
fi

PSA_JS_LIBRARY_URL_PREFIX="pagespeed_custom_static"
BEACON_HANDLER="ngx_pagespeed_beacon"
STATISTICS_HANDLER="ngx_pagespeed_statistics"
GLOBAL_STATISTICS_HANDLER="ngx_pagespeed_global_statistics"
MESSAGES_HANDLER="ngx_pagespeed_message"
STATISTICS_URL=http://$PRIMARY_HOSTNAME/ngx_pagespeed_statistics

# An expected failure can be indicated like: "~In-place resource optimization~"
PAGESPEED_EXPECTED_FAILURES="
~Cache purging with PageSpeed off in vhost, but on in directory.~
~2-pass ipro with long ModPagespeedInPlaceRewriteDeadline~
~3-pass ipro with short ModPagespeedInPlaceRewriteDeadline~
"

if [ "$POSITION_AUX" = "true" ] ; then
  PAGESPEED_EXPECTED_FAILURES+="
~server-side includes~
"
fi


# Some tests are flakey under valgrind. For now, add them to the expected
# failures when running under valgrind.
#
# TODO(sligicki): When the prioritize critical css race condition is fixed, the
# two prioritize_critical_css tests no longer need to be listed here.
# TODO(oschaaf): Now that we wait after we send a SIGHUP for the new worker
# process to handle requests, check if we can remove more from the expected
# failures here under valgrind.
if $USE_VALGRIND; then
    PAGESPEED_EXPECTED_FAILURES+="
~combine_css Maximum size of combined CSS.~
~prioritize_critical_css~
~prioritize_critical_css Able to read POST data from temp file.~
~IPRO flow uses cache as expected.~
~IPRO flow doesn't copy uncacheable resources multiple times.~
~inline_unauthorized_resources allows unauthorized css selectors~
"
fi

# The existing system test takes its arguments as positional parameters, and
# wants different ones than we want, so we need to reset our positional args.
set -- "$PRIMARY_HOSTNAME"
source $SYSTEM_TEST_FILE

# nginx-specific system tests

start_test Test pagespeed directive inside if block inside location block.

URL="http://if-in-location.example.com/"
URL+="mod_pagespeed_example/inline_javascript.html"

# When we specify the X-Custom-Header-Inline-Js that triggers an if block in the
# config which turns on inline_javascript.
WGET_ARGS="--header=X-Custom-Header-Inline-Js:Yes"
http_proxy=$SECONDARY_HOSTNAME \
  fetch_until $URL 'grep -c document.write' 1
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $WGET_ARGS $URL)
check_from "$OUT" fgrep "X-Inline-Javascript: Yes"
check_not_from "$OUT" fgrep "inline_javascript.js"

# Without that custom header we don't trigger the if block, and shouldn't get
# any inline javascript.
WGET_ARGS=""
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $WGET_ARGS $URL)
check_from "$OUT" fgrep "X-Inline-Javascript: No"
check_from "$OUT" fgrep "inline_javascript.js"
check_not_from "$OUT" fgrep "document.write"

# Tests related to rewritten response (downstream) caching.

if [ "$NATIVE_FETCHER" = "on" ]; then
  echo "Native fetcher doesn't support PURGE requests and so we can't use or"
  echo "test downstream caching."
else
  CACHABLE_HTML_LOC="http://${SECONDARY_HOSTNAME}/mod_pagespeed_test/cachable_rewritten_html"
  CACHABLE_HTML_LOC+="/downstream_caching.html"
  TMP_LOG_LINE="proxy_cache.example.com GET /purge/mod_pagespeed_test/cachable_rewritten_"
  PURGE_REQUEST_IN_ACCESS_LOG=$TMP_LOG_LINE"html/downstream_caching.html.*(200)"

  OUT_CONTENTS_FILE="$OUTDIR/gzipped.html"
  OUT_HEADERS_FILE="$OUTDIR/headers.html"
  GZIP_WGET_ARGS="-q -S --header=Accept-Encoding:gzip -o $OUT_HEADERS_FILE -O - "

  # Number of downstream cache purges should be 0 here.
  CURRENT_STATS=$($WGET_DUMP $STATISTICS_URL)
  check_from "$CURRENT_STATS" egrep -q \
    "downstream_cache_purge_attempts:[[:space:]]*0"

  # The 1st request results in a cache miss, non-rewritten response
  # produced by pagespeed code and a subsequent purge request.
  start_test Check for case where rewritten cache should get purged.
  check_for_no_rewriting "--header=Host:proxy_cache.example.com"
  check egrep -q "X-Cache: MISS" $OUT_HEADERS_FILE
  fetch_until $STATISTICS_URL \
    'grep -c successful_downstream_cache_purges:[[:space:]]*1' 1

  check [ $(grep -ce "$PURGE_REQUEST_IN_ACCESS_LOG" $ACCESS_LOG) = 1 ];

  # The 2nd request results in a cache miss (because of the previous purge),
  # rewritten response produced by pagespeed code and no new purge requests.
  start_test Check for case where rewritten cache should not get purged.
  check_for_rewriting "--header=Host:proxy_cache.example.com \
                      --header=X-PSA-Blocking-Rewrite:psatest"
  check egrep -q "X-Cache: MISS" $OUT_HEADERS_FILE
  CURRENT_STATS=$($WGET_DUMP $STATISTICS_URL)
  check_from "$CURRENT_STATS" egrep -q \
    "downstream_cache_purge_attempts:[[:space:]]*1"
  check [ $(grep -ce "$PURGE_REQUEST_IN_ACCESS_LOG" $ACCESS_LOG) = 1 ];

  # The 3rd request results in a cache hit (because the previous response is
  # now present in cache), rewritten response served out from cache and not
  # by pagespeed code and no new purge requests.
  start_test Check for case where there is a rewritten cache hit.
  check_for_rewriting "--header=Host:proxy_cache.example.com"
  check egrep -q "X-Cache: HIT" $OUT_HEADERS_FILE
  fetch_until $STATISTICS_URL \
    'grep -c downstream_cache_purge_attempts:[[:space:]]*1' 1
  check [ $(grep -ce "$PURGE_REQUEST_IN_ACCESS_LOG" $ACCESS_LOG) = 1 ];

  # Enable one of the beaconing dependent filters and verify interaction
  # between beaconing and downstream caching logic, by verifying that
  # whenever beaconing code is present in the rewritten page, the
  # output is also marked as a cache-miss, indicating that the instrumentation
  # was done by the backend.
  start_test Check whether beaconing is accompanied by a BYPASS always.
  WGET_ARGS="-S --header=Host:proxy_cache.example.com \
                --header=X-Allow-Beacon:yes"
  CACHABLE_HTML_LOC+="?PageSpeedFilters=lazyload_images"
  fetch_until -gzip $CACHABLE_HTML_LOC \
      "zgrep -c \"pagespeed\.CriticalImages\.Run\"" 1
  check egrep -q 'X-Cache: BYPASS' $WGET_OUTPUT
  check fgrep -q 'Cache-Control: no-cache, max-age=0' $WGET_OUTPUT

fi

start_test "Custom statistics paths in server block"

# Served on normal paths by default.
URL="inherit-paths.example.com/ngx_pagespeed_statistics"
OUT=$(http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL)
check_from "$OUT" fgrep -q cache_time_us

URL="inherit-paths.example.com/ngx_pagespeed_message"
OUT=$(http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL)
check_from "$OUT" fgrep -q Info

URL="inherit-paths.example.com/pagespeed_console"
OUT=$(http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL)
check_from "$OUT" fgrep -q console_div

URL="inherit-paths.example.com/pagespeed_admin/"
OUT=$(http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL)
check_from "$OUT" fgrep -q Admin

# Not served on normal paths when overriden.
URL="custom-paths.example.com/ngx_pagespeed_statistics"
OUT=$(http_proxy=$SECONDARY_HOSTNAME check_not $WGET_DUMP $URL)
check_not_from "$OUT" fgrep -q cache_time_us

URL="custom-paths.example.com/ngx_pagespeed_message"
OUT=$(http_proxy=$SECONDARY_HOSTNAME check_not $WGET_DUMP $URL)
check_not_from "$OUT" fgrep -q Info

URL="custom-paths.example.com/pagespeed_console"
OUT=$(http_proxy=$SECONDARY_HOSTNAME check_not $WGET_DUMP $URL)
check_not_from "$OUT" fgrep -q console_div

URL="custom-paths.example.com/pagespeed_admin/"
OUT=$(http_proxy=$SECONDARY_HOSTNAME check_not $WGET_DUMP $URL)
check_not_from "$OUT" fgrep -q Admin

# Served on custom paths when overriden
URL="custom-paths.example.com/custom_pagespeed_statistics"
OUT=$(http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL)
check_from "$OUT" fgrep -q cache_time_us

URL="custom-paths.example.com/custom_pagespeed_message"
OUT=$(http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL)
check_from "$OUT" fgrep -q Info

URL="custom-paths.example.com/custom_pagespeed_console"
OUT=$(http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL)
check_from "$OUT" fgrep -q console_div

URL="custom-paths.example.com/custom_pagespeed_admin/"
OUT=$(http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL)
check_from "$OUT" fgrep -q Admin

function gunzip_grep_0ff() {
  gunzip - | fgrep -q "color:#00f"
  echo $?
}

start_test ipro with mod_deflate
CSS_FILE="http://compressed-css.example.com/"
CSS_FILE+="mod_pagespeed_test/ipro/mod_deflate/big.css"
http_proxy=$SECONDARY_HOSTNAME fetch_until -gzip $CSS_FILE gunzip_grep_0ff 0

start_test ipro with reverse proxy of compressed content
http_proxy=$SECONDARY_HOSTNAME \
  fetch_until -gzip http://ipro-proxy.example.com/big.css \
    gunzip_grep_0ff 0

# Also test the .pagespeed. version, to make sure we didn't accidentally gunzip
# stuff above when we shouldn't have.
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET -q -O - \
      http://ipro-proxy.example.com/A.big.css.pagespeed.cf.0.css)
check_from "$OUT" fgrep -q "big{color:#00f}"

start_test Respect X-Forwarded-Proto when told to
FETCHED=$OUTDIR/x_forwarded_proto
URL=$SECONDARY_HOSTNAME/mod_pagespeed_example/?PageSpeedFilters=add_base_tag
HEADERS="--header=X-Forwarded-Proto:https --header=Host:xfp.example.com"
check $WGET_DUMP -O $FETCHED $HEADERS $URL
# When enabled, we respect X-Forwarded-Proto and thus list base as https.
check fgrep -q '<base href="https://' $FETCHED

start_test Relative redirects starting with a forward slash survive.
URL=http://xfp.example.com/redirect
# wget seems a bit hairy here, when it comes to handling (relative) redirects.
# I could not get this test going with wget, and that is why curl is used here.
# TODO(oschaaf): debug wget some more and swap out curl here.
OUT=$(curl -v --proxy $SECONDARY_HOSTNAME $URL 2>&1)
check_from "$OUT" egrep -q '301 Moved Permanently'
# The important part is that we don't end up with an absolute location here.
check_from "$OUT" grep -q 'Location: /mod_pagespeed_example'
check_not_from "$OUT" grep -q 'Location: http'

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
URL+="?PageSpeed=on&PageSpeedFilters=combine_javascript"
fetch_until "$URL" "fgrep -c .pagespeed." 1 --header=Host:www.google.com

# If this accepts the Host header and fetches from google.com it will fail with
# a 404.  Instead it should use a loopback fetch and succeed.
URL="$HOSTNAME/mod_pagespeed_example/.pagespeed.ce.8CfGBvwDhH.css"
check wget -O /dev/null --header=Host:www.google.com "$URL"

start_test statistics load

OUT=$($WGET_DUMP $STATISTICS_URL)
check_from "$OUT" grep 'PageSpeed Statistics'

start_test statistics handler full-featured
OUT=$($WGET_DUMP $STATISTICS_URL?config)
check_from "$OUT" grep "InPlaceResourceOptimization (ipro)"

start_test statistics handler properly sets JSON content-type
OUT=$($WGET_DUMP $STATISTICS_URL?json)
check_from "$OUT" grep "Content-Type: application/javascript"

start_test scrape stats works

# This needs to be before reload, when we clear the stats.
check test $(scrape_stat image_rewrite_total_original_bytes) -ge 10000

# Test that ngx_pagespeed keeps working after nginx gets a signal to reload the
# configuration.  This is in the middle of tests so that significant work
# happens both before and after.
start_test "Reload config"

# Fire up some heavy load if ab is available to test a stressed reload.
# TODO(oschaaf): make sure we wait for the new worker to get ready to accept
# requests.
fire_ab_load

check wget $EXAMPLE_ROOT/styles/W.rewrite_css_images.css.pagespeed.cf.Hash.css \
  -O /dev/null
check_simple "$NGINX_EXECUTABLE" -s reload -c "$PAGESPEED_CONF"
# Wait for the new worker process with the new configuration to get ready, or
# else the sudden reset of the shared mem statistics/cache might catch upcoming
# tests unaware.
while [ $(scrape_stat image_rewrite_total_original_bytes) -gt 0 ]
do
    echo "Waiting for new worker to get ready..."
    sleep .1
done

check wget $EXAMPLE_ROOT/styles/W.rewrite_css_images.css.pagespeed.cf.Hash.css \
  -O /dev/null
if [ "$AB_PID" != "0" ]; then
    echo "Kill ab (pid: $AB_PID)"
    kill -s KILL $AB_PID &>/dev/null || true
fi

# This is dependent upon having a beacon handler.
test_filter add_instrumentation beacons load.

# Nginx won't sent a Content-Length header on a 204, and while this is correct
# per rfc 2616 wget hangs. Adding --no-http-keep-alive fixes that, as wget will.
# send 'Connection: close' in its request headers, which will make nginx
# respond with that as well. Check that we got a 204.
BEACON_URL="http%3A%2F%2Fimagebeacon.example.com%2Fmod_pagespeed_test%2F"
OUT=$(wget -q  --save-headers -O - --no-http-keep-alive \
      "$PRIMARY_SERVER/$BEACON_HANDLER?ets=load:13&url=$BEACON_URL")
check_from "$OUT" grep '^HTTP/1.1 204'
# The $'...' tells bash to interpret c-style escapes, \r in this case.
check_from "$OUT" grep $'^Cache-Control: max-age=0, no-cache\r$'

# Several cache flushing tests.

start_test Cache flushing works by touching cache.flush in cache directory.

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
check touch "$IPRO_CACHE/cache.flush"
sleep 1

CACHE_TESTING_DIR="$SERVER_ROOT/mod_pagespeed_test/cache_flush/"
CACHE_TESTING_TMPDIR="$CACHE_TESTING_DIR/$$"
mkdir "$CACHE_TESTING_TMPDIR"
cp "$CACHE_TESTING_DIR/cache_flush_test.html" "$CACHE_TESTING_TMPDIR/"
CSS_FILE="$CACHE_TESTING_TMPDIR/update.css"
echo ".class myclass { color: $COLOR0; }" > "$CSS_FILE"

URL_PATH="mod_pagespeed_test/cache_flush/$$/cache_flush_test.html"

URL="$SECONDARY_HOSTNAME/$URL_PATH"
CACHE_A="--header=Host:cache_a.example.com"
fetch_until $URL "grep -c $COLOR0" 1 $CACHE_A

CACHE_B="--header=Host:cache_b.example.com"
fetch_until $URL "grep -c $COLOR0" 1 $CACHE_B

CACHE_C="--header=Host:cache_c.example.com"
fetch_until $URL "grep -c $COLOR0" 1 $CACHE_C

# All three caches are now populated.

# Track how many flushes were noticed by pagespeed processes up till this point
# in time.  Note that each process/vhost separately detects the 'flush'.

# A helper function just used here to look up the cache flush count for each
# cache.
function cache_flush_count_scraper {
  CACHE_LETTER=$1  # a, b, or c
  URL="$SECONDARY_HOSTNAME/ngx_pagespeed_statistics"
  HOST="--header=Host:cache_${CACHE_LETTER}.example.com"
  $WGET_DUMP $HOST $URL | egrep "^cache_flush_count:? " | awk '{print $2}'
}

NUM_INITIAL_FLUSHES_A=$(cache_flush_count_scraper a)
NUM_INITIAL_FLUSHES_B=$(cache_flush_count_scraper b)
NUM_INITIAL_FLUSHES_C=$(cache_flush_count_scraper c)

# Now change the file to $COLOR1.
echo ".class myclass { color: $COLOR1; }" > "$CSS_FILE"

# We expect to have a stale cache for 5 seconds, so the result should stay
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

# Cache was just flushed, so it should see see exactly one flush and the other
# two should see none.
NUM_MEDIAL_FLUSHES_A=$(cache_flush_count_scraper a)
NUM_MEDIAL_FLUSHES_B=$(cache_flush_count_scraper b)
NUM_MEDIAL_FLUSHES_C=$(cache_flush_count_scraper c)
check [ $(($NUM_MEDIAL_FLUSHES_A - $NUM_INITIAL_FLUSHES_A)) -eq 1 ]
check [ $NUM_MEDIAL_FLUSHES_B -eq $NUM_INITIAL_FLUSHES_B ]
check [ $NUM_MEDIAL_FLUSHES_C -eq $NUM_INITIAL_FLUSHES_C ]

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

# Now cache A should see no flush while caches B and C should each see a flush.
NUM_FINAL_FLUSHES_A=$(cache_flush_count_scraper a)
NUM_FINAL_FLUSHES_B=$(cache_flush_count_scraper b)
NUM_FINAL_FLUSHES_C=$(cache_flush_count_scraper c)
check [ $NUM_FINAL_FLUSHES_A -eq $NUM_MEDIAL_FLUSHES_A ]
check [ $(($NUM_FINAL_FLUSHES_B - $NUM_MEDIAL_FLUSHES_B)) -eq 1 ]
check [ $(($NUM_FINAL_FLUSHES_C - $NUM_MEDIAL_FLUSHES_C)) -eq 1 ]

# Clean up so we don't leave behind a stray file not under source control.
rm -rf "$CACHE_TESTING_TMPDIR"

# connection_refused.html references modpagespeed.com:1023/someimage.png.
# Pagespeed will attempt to connect to that host and port to fetch the input
# resource using serf.  We expect the connection to be refused.  Relies on
# "pagespeed Domain modpagespeed.com:1023" in the config.  Also relies on
# running after a cache-flush to avoid bypassing the serf fetch, since pagespeed
# remembers fetch-failures in its cache for 5 minutes.
start_test Connection refused handling

# Monitor the log starting now.  tail -F will catch log rotations.
FETCHER_REFUSED_PATH=$TESTTMP/instaweb_fetcher_refused
rm -f $FETCHER_REFUSED_PATH
LOG="$TEST_TMP/error.log"
echo LOG = $LOG
tail --sleep-interval=0.1 -F $LOG > $FETCHER_REFUSED_PATH &
TAIL_PID=$!
# Wait for tail to start.
echo -n "Waiting for tail to start..."
while [ ! -s $FETCHER_REFUSED_PATH ]; do
  sleep 0.1
  echo -n "."
done
echo "done!"

# Actually kick off the request.
echo $WGET_DUMP $TEST_ROOT/connection_refused.html
echo checking...
check $WGET_DUMP $TEST_ROOT/connection_refused.html > /dev/null
echo check done
# If we are spewing errors, this gives time to spew lots of them.
sleep 1
# Wait up to 10 seconds for the background fetch of someimage.png to fail.
if [ "$NATIVE_FETCHER" = "on" ]; then
  EXPECTED="111: Connection refused"
else
  EXPECTED="Serf status 111"
fi
for i in {1..100}; do
  ERRS=$(grep -c "$EXPECTED" $FETCHER_REFUSED_PATH || true)
  if [ $ERRS -ge 1 ]; then
    break;
  fi;
  echo -n "."
  sleep 0.1
done;
echo "."
# Kill the log monitor silently.
kill $TAIL_PID
wait $TAIL_PID 2> /dev/null || true
check [ $ERRS -ge 1 ]

# TODO(jefftk): when we support ListOutstandingUrlsOnError uncomment the below
#
## Make sure we have the URL detail we expect because ListOutstandingUrlsOnError
## is on in the config file.
#echo Check that ListOutstandingUrlsOnError works
#check grep "URL http://modpagespeed.com:1023/someimage.png active for " \
#  $FETCHER_REFUSED_PATH

start_test Blocking rewrite enabled.
# We assume that blocking_rewrite_test_dont_reuse_1.jpg will not be
# rewritten on the first request since it takes significantly more time to
# rewrite than the rewrite deadline and it is not already accessed by
# another request earlier.
BLOCKING_REWRITE_URL="$TEST_ROOT/blocking_rewrite.html"
BLOCKING_REWRITE_URL+="?PageSpeedFilters=rewrite_images"
OUTFILE=$WGET_DIR/blocking_rewrite.out.html
OLDSTATS=$WGET_DIR/blocking_rewrite_stats.old
NEWSTATS=$WGET_DIR/blocking_rewrite_stats.new
$WGET_DUMP $STATISTICS_URL > $OLDSTATS
check $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest'\
      $BLOCKING_REWRITE_URL -O $OUTFILE
$WGET_DUMP $STATISTICS_URL > $NEWSTATS
check_stat $OLDSTATS $NEWSTATS image_rewrites 1
check_stat $OLDSTATS $NEWSTATS cache_hits 0
check_stat $OLDSTATS $NEWSTATS cache_misses 2
check_stat $OLDSTATS $NEWSTATS cache_inserts 3
# TODO(sligocki): There is no stat num_rewrites_executed. Fix.
#check_stat $OLDSTATS $NEWSTATS num_rewrites_executed 1

start_test Blocking rewrite enabled using wrong key.
URL="blocking.example.com/mod_pagespeed_test/blocking_rewrite_another.html"
OUTFILE=$WGET_DIR/blocking_rewrite.out.html
http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP \
  --header 'X-PSA-Blocking-Rewrite: junk' \
  $URL > $OUTFILE
check [ $(grep -c "[.]pagespeed[.]" $OUTFILE) -lt 1 ]

http_proxy=$SECONDARY_HOSTNAME fetch_until $URL \
  'grep -c [.]pagespeed[.]' 1

run_post_cache_flush

# Test ForbidAllDisabledFilters, which is set in the config for
# /mod_pagespeed_test/forbid_all_disabled/disabled/ where we've disabled
# remove_quotes, remove_comments, and collapse_whitespace.  We fetch 3 times
# trying to circumvent the forbidden flag: a normal fetch, a fetch using a query
# parameter to try to enable the forbidden filters, and a fetch using a request
# header to try to enable the forbidden filters.
function test_forbid_all_disabled() {
  QUERYP="$1"
  HEADER="$2"
  if [ -n "$QUERYP" ]; then
    INLINE_CSS=",-inline_css"
  else
    INLINE_CSS="?PageSpeedFilters=-inline_css"
  fi
  WGET_ARGS="--header=X-PSA-Blocking-Rewrite:psatest"
  URL=$TEST_ROOT/forbid_all_disabled/disabled/forbidden.html
  OUTFILE="$TESTTMP/test_forbid_all_disabled"
  # Fetch testing that forbidden filters stay disabled.
  echo $WGET $HEADER $URL$QUERYP$INLINE_CSS
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL$QUERYP$INLINE_CSS
  check     egrep -q '<link rel="stylesheet' $OUTFILE
  check     egrep -q '<!--'                  $OUTFILE
  check     egrep -q '    <li>'              $OUTFILE
  # Fetch testing that enabling inline_css works.
  echo $WGET $HEADER $URL
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL
  check     egrep -q '<style>.yellow'        $OUTFILE
  rm -f $OUTFILE
}
start_test ForbidAllDisabledFilters baseline check.
test_forbid_all_disabled "" ""
start_test ForbidAllDisabledFilters query parameters check.
QUERYP="?PageSpeedFilters="
QUERYP="${QUERYP}+remove_quotes,+remove_comments,+collapse_whitespace"
test_forbid_all_disabled $QUERYP ""
start_test ForbidAllDisabledFilters request headers check.
HEADER="--header=PageSpeedFilters:"
HEADER="${HEADER}+remove_quotes,+remove_comments,+collapse_whitespace"
test_forbid_all_disabled "" $HEADER

# Test the experiment framework (Furious).

start_test PageSpeedExperiment cookie is set.
EXP_EXAMPLE="http://experiment.example.com/mod_pagespeed_example"
EXP_EXTEND_CACHE="$EXP_EXAMPLE/extend_cache.html"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $EXP_EXTEND_CACHE)
check_from "$OUT" fgrep "PageSpeedExperiment="
MATCHES=$(echo "$OUT" | grep -c "PageSpeedExperiment=")
check [ $MATCHES -eq 1 ]

start_test PageSpeedFilters query param should disable experiments.
URL="$EXP_EXTEND_CACHE?PageSpeed=on&PageSpeedFilters=rewrite_css"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)
check_not_from "$OUT" fgrep 'PageSpeedExperiment='

start_test experiment assignment can be forced
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
      "$EXP_EXTEND_CACHE?PageSpeedEnrollExperiment=2")
check_from "$OUT" fgrep 'PageSpeedExperiment=2'

start_test experiment assignment can be forced to a 0% experiment
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
      "$EXP_EXTEND_CACHE?PageSpeedEnrollExperiment=3")
check_from "$OUT" fgrep 'PageSpeedExperiment=3'

start_test experiment assignment can be forced even if already assigned
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
      --header Cookie:PageSpeedExperiment=7 \
      "$EXP_EXTEND_CACHE?PageSpeedEnrollExperiment=2")
check_from "$OUT" fgrep 'PageSpeedExperiment=2'

start_test If the user is already assigned, no need to assign them again.
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
      --header='Cookie: PageSpeedExperiment=2' $EXP_EXTEND_CACHE)
check_not_from "$OUT" fgrep 'PageSpeedExperiment='

start_test The beacon should include the experiment id.
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
      --header='Cookie: PageSpeedExperiment=2' $EXP_EXTEND_CACHE)
BEACON_CODE="pagespeed.addInstrumentationInit('/$BEACON_HANDLER', 'load',"
BEACON_CODE+=" '&exptid=2', 'http://experiment.example.com/"
BEACON_CODE+="mod_pagespeed_example/extend_cache.html');"
check_from "$OUT" grep "$BEACON_CODE"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --header='Cookie: PageSpeedExperiment=7' \
      $EXP_EXTEND_CACHE)
BEACON_CODE="pagespeed.addInstrumentationInit('/$BEACON_HANDLER', 'load',"
BEACON_CODE+=" '&exptid=7', 'http://experiment.example.com/"
BEACON_CODE+="mod_pagespeed_example/extend_cache.html');"
check_from "$OUT" grep "$BEACON_CODE"

start_test The no-experiment group beacon should not include an experiment id.
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
      --header='Cookie: PageSpeedExperiment=0' $EXP_EXTEND_CACHE)
check_not_from "$OUT" grep 'pagespeed_beacon.*exptid'

# We expect id=7 to be index=a and id=2 to be index=b because that's the
# order they're defined in the config file.
start_test Resource urls are rewritten to include experiment indexes.
http_proxy=$SECONDARY_HOSTNAME \
  fetch_until $EXP_EXTEND_CACHE \
    "fgrep -c .pagespeed.a.ic." 1 --header=Cookie:PageSpeedExperiment=7
http_proxy=$SECONDARY_HOSTNAME \
  fetch_until $EXP_EXTEND_CACHE \
    "fgrep -c .pagespeed.b.ic." 1 --header=Cookie:PageSpeedExperiment=2
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --header='Cookie: PageSpeedExperiment=7' \
      $EXP_EXTEND_CACHE)
check_from "$OUT" fgrep ".pagespeed.a.ic."
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --header='Cookie: PageSpeedExperiment=2' \
      $EXP_EXTEND_CACHE)
check_from "$OUT" fgrep ".pagespeed.b.ic."

start_test Images are different when the url specifies different experiments.
# While the images are the same, image B should be smaller because in the config
# file we enable convert_jpeg_to_progressive only for id=2 (side B).  Ideally we
# would check that it was actually progressive, by checking whether "identify
# -verbose filename" produced "Interlace: JPEG" or "Interlace: None", but that
# would introduce a dependency on imagemagick.  This is just as accurate, but
# more brittle (because changes to our compression code would change the
# computed file sizes).

IMG_A="$EXP_EXAMPLE/images/xPuzzle.jpg.pagespeed.a.ic.fakehash.jpg"
IMG_B="$EXP_EXAMPLE/images/xPuzzle.jpg.pagespeed.b.ic.fakehash.jpg"
http_proxy=$SECONDARY_HOSTNAME fetch_until $IMG_A 'wc -c' 102902 "" -le
http_proxy=$SECONDARY_HOSTNAME fetch_until $IMG_B 'wc -c'  98276 "" -le

start_test Analytics javascript is added for the experimental group.
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --header='Cookie: PageSpeedExperiment=2' \
      $EXP_EXTEND_CACHE)
check_from "$OUT" fgrep -q 'Experiment: 2'
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --header='Cookie: PageSpeedExperiment=7' \
      $EXP_EXTEND_CACHE)
check_from "$OUT" fgrep -q 'Experiment: 7'

start_test Analytics javascript is not added for the no-experiment group.
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --header='Cookie: PageSpeedExperiment=0' \
      $EXP_EXTEND_CACHE)
check_not_from "$OUT" fgrep -q 'Experiment:'

start_test Analytics javascript is not added for any group with Analytics off.
EXP_NO_GA_EXTEND_CACHE="http://experiment.noga.example.com"
EXP_NO_GA_EXTEND_CACHE+="/mod_pagespeed_example/extend_cache.html"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --header='Cookie: PageSpeedExperiment=2' \
      $EXP_NO_GA_EXTEND_CACHE)
check_not_from "$OUT" fgrep -q 'Experiment:'
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --header='Cookie: PageSpeedExperiment=7' \
      $EXP_NO_GA_EXTEND_CACHE)
check_not_from "$OUT" fgrep -q 'Experiment:'
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --header='Cookie: PageSpeedExperiment=0' \
      $EXP_NO_GA_EXTEND_CACHE)
check_not_from "$OUT" fgrep -q 'Experiment:'

# Repeat the critical_css_filter test on a host that processes post data via
# temp files to test that ngx_pagespeed specific code path.
filter_spec_method="headers"
test_filter prioritize_critical_css Able to read POST data from temp file.
URL="http://beacon-post-temp-file.example.com/mod_pagespeed_example/prioritize_critical_css.html"
http_proxy=$SECONDARY_HOSTNAME\
  fetch_until -save $URL 'fgrep -c pagespeed.criticalCssBeaconInit' 1
check [ $(fgrep -o ".very_large_class_name_" $FETCH_FILE | wc -l) -eq 36 ]
CALL_PAT=".*criticalCssBeaconInit("
SKIP_ARG="[^,]*,"
CAPTURE_ARG="'\([^']*\)'.*"
BEACON_PATH=$(sed -n "s/${CALL_PAT}${CAPTURE_ARG}/\1/p" $FETCH_FILE)
ESCAPED_URL=$( \
  sed -n "s/${CALL_PAT}${SKIP_ARG}${CAPTURE_ARG}/\1/p" $FETCH_FILE)
OPTIONS_HASH=$( \
  sed -n "s/${CALL_PAT}${SKIP_ARG}${SKIP_ARG}${CAPTURE_ARG}/\1/p" $FETCH_FILE)
NONCE=$( \
  sed -n "s/${CALL_PAT}${SKIP_ARG}${SKIP_ARG}${SKIP_ARG}${CAPTURE_ARG}/\1/p" \
  $FETCH_FILE)
BEACON_URL="http://${SECONDARY_HOSTNAME}${BEACON_PATH}?url=${ESCAPED_URL}"
BEACON_DATA="oh=${OPTIONS_HASH}&n=${NONCE}&cs=.big,.blue,.bold,.foo"

OUT=$(wget -q  --save-headers -O - --no-http-keep-alive \
      --post-data "$BEACON_DATA" "$BEACON_URL" \
      --header "Host:beacon-post-temp-file.example.com")
check_from "$OUT" grep '^HTTP/1.1 204'

# Now make sure we see the correct critical css rules.
http_proxy=$SECONDARY_HOSTNAME\
  fetch_until $URL \
    'grep -c <style>[.]blue{[^}]*}</style>' 1
http_proxy=$SECONDARY_HOSTNAME\
  fetch_until $URL \
    'grep -c <style>[.]big{[^}]*}</style>' 1
http_proxy=$SECONDARY_HOSTNAME\
  fetch_until $URL \
    'grep -c <style>[.]blue{[^}]*}[.]bold{[^}]*}</style>' 1
http_proxy=$SECONDARY_HOSTNAME\
  fetch_until -save $URL \
    'grep -c <style>[.]foo{[^}]*}</style>' 1
# The last one should also have the other 3, too.
check [ `grep -c '<style>[.]blue{[^}]*}</style>' $FETCH_UNTIL_OUTFILE` = 1 ]
check [ `grep -c '<style>[.]big{[^}]*}</style>' $FETCH_UNTIL_OUTFILE` = 1 ]
check [ `grep -c '<style>[.]blue{[^}]*}[.]bold{[^}]*}</style>' \
  $FETCH_UNTIL_OUTFILE` = 1 ]

start_test keepalive with html rewriting
keepalive_test "keepalive-html.example.com"\
  "/mod_pagespeed_example/rewrite_images.html" ""

start_test keepalive with serving resources
keepalive_test "keepalive-resource.example.com"\
  "/mod_pagespeed_example/combine_javascript2.js+combine_javascript1.js+combine_javascript2.js.pagespeed.jc.0.js"\
  ""

BEACON_URL="http%3A%2F%2Fimagebeacon.example.com%2Fmod_pagespeed_test%2F"
start_test keepalive with beacon get requests
keepalive_test "keepalive-beacon-get.example.com"\
  "/$BEACON_HANDLER?ets=load:13&url=$BEACON_URL" ""

BEACON_DATA="url=http%3A%2F%2Fimagebeacon.example.com%2Fmod_pagespeed_test%2F"
BEACON_DATA+="image_rewriting%2Frewrite_images.html"
BEACON_DATA+="&oh=$OPTIONS_HASH&ci=2932493096"

start_test keepalive with beacon post requests
keepalive_test "keepalive-beacon-post.example.com" "/$BEACON_HANDLER"\
  "$BEACON_DATA"

start_test keepalive with static resources
keepalive_test "keepalive-static.example.com"\
  "/pagespeed_custom_static/js_defer.0.js" ""

start_test pagespeed_custom_static defer js served with correct headers.
# First, determine which hash js_defer is served with. We need a correct hash
# to get it served up with an Etag, which is one of the things we want to test.
URL="$HOSTNAME/mod_pagespeed_example/defer_javascript.html?PageSpeed=on&PageSpeedFilters=defer_javascript"
OUT=$($WGET_DUMP $URL)
HASH=$(echo $OUT \
  | grep --only-matching "/js_defer\\.*\([^.]\)*.js" | cut -d '.' -f 2)

start_test JS gzip headers

JS_URL="$HOSTNAME/pagespeed_custom_static/js_defer.$HASH.js"
JS_HEADERS=$($WGET -O /dev/null -q -S --header='Accept-Encoding: gzip' \
  $JS_URL 2>&1)
check_200_http_response "$JS_HEADERS"
check_from "$JS_HEADERS" fgrep -qi 'Content-Encoding: gzip'
check_from "$JS_HEADERS" fgrep -qi 'Vary: Accept-Encoding'
# Nginx's gzip module clears etags, which we don't want. Make sure we have it.
check_from "$JS_HEADERS" egrep -qi 'Etag: W/"0"'
check_from "$JS_HEADERS" fgrep -qi 'Last-Modified:'


start_test PageSpeedFilters response headers is interpreted
URL=$SECONDARY_HOSTNAME/mod_pagespeed_example/
OUT=$($WGET_DUMP --header=Host:response-header-filters.example.com $URL)
check_from "$OUT" egrep -qi 'addInstrumentationInit'
OUT=$($WGET_DUMP --header=Host:response-header-disable.example.com $URL)
check_not_from "$OUT" egrep -qi 'addInstrumentationInit'

# TODO(jmaessen, jefftk): Port proxying tests, which rely on pointing a
# MapProxyDomain construct at a static server.  Perhaps localhost:8050 will
# serve, but the tests need to use different urls then.  For mod_pagespeed these
# tests immediately precede the "scrape_secondary_stat" definition in
# system_test.sh.

start_test messages load
OUT=$($WGET_DUMP "$HOSTNAME/ngx_pagespeed_message")
check_not_from "$OUT" grep "Writing to ngx_pagespeed_message failed."
check_from "$OUT" grep -q "/mod_pagespeed_example"

start_test Check keepalive after a 304 responses.
# '-m 2' specifies that the whole operation is allowed to take 2 seconds max.
check curl -vv -m 2 http://$PRIMARY_HOSTNAME/foo.css.pagespeed.ce.0.css \
    -H 'If-Modified-Since: Z' http://$PRIMARY_HOSTNAME/foo

start_test Date response header set
OUT=$($WGET_DUMP $EXAMPLE_ROOT/combine_css.html)
check_not_from "$OUT" egrep -q '^Date: Thu, 01 Jan 1970 00:00:00 GMT'

OUT=$($WGET_DUMP --header=Host:date.example.com \
    http://$SECONDARY_HOSTNAME/mod_pagespeed_example/combine_css.html)
check_from "$OUT" egrep -q '^Date: Fri, 16 Oct 2009 23:05:07 GMT'
WGET_ARGS=

#very basic tests to test gzip nesting configuration
start_test Nested gzip gzip off
URL="http://$SECONDARY_HOSTNAME/mod_pagespeed_example/"
HEADERS="--header=Accept-Encoding:gzip --header=Host:gzip-test1.example.com"
OUT=$($WGET_DUMP -O /dev/null -S $HEADERS $URL 2>&1)
check_not_from "$OUT" fgrep -qi 'Content-Encoding: gzip'
check_not_from "$OUT" fgrep -qi 'Vary: Accept-Encoding'

start_test Nested gzip gzip on
URL="http://$SECONDARY_HOSTNAME/mod_pagespeed_example/styles/big.css"
HEADERS="--header=Accept-Encoding:gzip --header=Host:gzip-test1.example.com"
OUT=$($WGET_DUMP -O /dev/null -S $HEADERS $URL 2>&1)
check_from "$OUT" fgrep -qi 'Content-Encoding: gzip'
check_from "$OUT" fgrep -qi 'Vary: Accept-Encoding'

start_test Nested gzip pagespeed off
URL="http://$SECONDARY_HOSTNAME/mod_pagespeed_example/"
HEADERS="--header=Accept-Encoding:gzip --header=Host:gzip-test2.example.com"
OUT=$($WGET_DUMP -O /dev/null -S $HEADERS $URL 2>&1)
check_not_from "$OUT" fgrep -qi 'Content-Encoding: gzip'
check_not_from "$OUT" fgrep -qi 'Vary: Accept-Encoding'

start_test Nested gzip pagespeed on
URL="http://$SECONDARY_HOSTNAME/mod_pagespeed_example/styles/big.css"
HEADERS="--header=Accept-Encoding:gzip --header=Host:gzip-test2.example.com"
OUT=$($WGET_DUMP -O /dev/null -S $HEADERS $URL 2>&1)
check_from "$OUT" fgrep -qi 'Content-Encoding: gzip'
check_from "$OUT" fgrep -qi 'Vary: Accept-Encoding'

start_test Test that POST requests are rewritten.
URL="http://$SECONDARY_HOSTNAME/mod_pagespeed_example/rewrite_images.html"
HEADERS="--header=Host:proxy-post.example.com --post-data=abcdefgh"
OUT=$($WGET_DUMP -S $HEADERS $URL 2>&1)
check_from "$OUT" fgrep -qi 'addInstrumentationInit'

start_test Test that we can modify filters via script variables.
URL="http://$SECONDARY_HOSTNAME/mod_pagespeed_example/rewrite_images.html"
HEADERS="--header=Host:script-filters.example.com --header=X-Script:1"
OUT=$($WGET_DUMP -S $HEADERS $URL 2>&1)
check_from "$OUT" fgrep -qi 'addInstrumentationInit'

HEADERS="--header=Host:script-filters.example.com"
OUT=$($WGET_DUMP -S $HEADERS $URL 2>&1)
check_not_from "$OUT" fgrep -qi 'addInstrumentationInit'

start_test Test that we can modify domain sharding via script variables.
URL="http://$SECONDARY_HOSTNAME/mod_pagespeed_example/rewrite_images.html"
HEADERS="--header=Host:script-filters.example.com"
OUT=$($WGET_DUMP -S $HEADERS $URL 2>&1)
check_from "$OUT" fgrep "http://cdn1.example.com"
check_from "$OUT" fgrep "http://cdn2.example.com"

URL="http://$SECONDARY_HOSTNAME/mod_pagespeed_example/rewrite_images.html"
HEADERS="--header=Host:script-filters.example.com --header=X-Script:1"
OUT=$($WGET_DUMP -S $HEADERS $URL 2>&1)
check_not_from "$OUT" fgrep "http://cdn1.example.com"
check_not_from "$OUT" fgrep "http://cdn2.example.com"

if [ "$NATIVE_FETCHER" != "on" ]; then
  start_test Test that we can rewrite an HTTPS resource.
  fetch_until $TEST_ROOT/https_fetch/https_fetch.html \
   'grep -c /https_gstatic_dot_com/1.gif.pagespeed.ce' 1
fi

start_test Base config has purging disabled.  Check error message syntax.
OUT=$($WGET_DUMP "$HOSTNAME/pagespeed_admin/cache?purge=*")
check_from "$OUT" fgrep -q "pagespeed EnableCachePurge on;"

start_test Default server header in html flow.
URL=http://headers.example.com/mod_pagespeed_example/
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP -O /dev/null -S $URL 2>&1)
# '|| true' in the line below supresses the exit code from grep when there is no
# match in its input (1).
MATCHES=$(echo "$OUT" | grep -c "Server: nginx/") || true
check [ $MATCHES -eq 1 ]

start_test Default server header in resource flow.
URL=http://headers.example.com/mod_pagespeed_example/
URL+=combine_javascript2.js+combine_javascript1.js.pagespeed.jc.0.js
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP -O /dev/null -S $URL 2>&1)
MATCHES=$(echo "$OUT" | grep -c "Server: nginx/") || true
check [ $MATCHES -eq 1 ]

start_test Default server header in IPRO flow.
URL=http://headers.example.com//mod_pagespeed_example/combine_javascript2.js
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP -O /dev/null -S $URL 2>&1)
MATCHES=$(echo "$OUT" | grep -c "Server: nginx/") || true
check [ $MATCHES -eq 1 ]

start_test Override server header in html flow.
URL=http://headers.example.com/mod_pagespeed_test/whitespace.html
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP -O /dev/null -S $URL 2>&1)
MATCHES=$(echo "$OUT" | grep -c "Server: override") || true
check [ $MATCHES -eq 1 ]

if [ "$POSITION_AUX" = "true" ] ; then
  start_test Override server header in resource flow.
  URL=http://headers.example.com/mod_pagespeed_test/
  URL+=A.proxy_pass.css.pagespeed.cf.0.css
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP -O /dev/null -S $URL 2>&1)
  MATCHES=$(echo "$OUT" | grep -c "Server: override") || true
  check [ $MATCHES -eq 1 ]

  start_test Override server header in IPRO flow.
  URL=http://headers.example.com/mod_pagespeed_test/proxy_pass.css
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP -O /dev/null -S $URL 2>&1)
  MATCHES=$(echo "$OUT" | grep -c "Server: override") || true
  check [ $MATCHES -eq 1 ]
fi

start_test Conditional cache-control header override in resource flow.
URL=http://headers.example.com/mod_pagespeed_test/
URL+=A.doesnotexist.css.pagespeed.cf.0.css
# The 404 response makes wget exit with an error code, which we ignore.
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP -O /dev/null -S $URL 2>&1) || true
# We ignored the exit code, check if we got a 404 response.
check_from "$OUT" fgrep -qi '404'
MATCHES=$(echo "$OUT" | grep -c "Cache-Control: override") || true
check [ $MATCHES -eq 1 ]

start_test Shutting down.

# Fire up some heavy load if ab is available to test a stressed shutdown
fire_ab_load

if $USE_VALGRIND; then
    kill -s quit $VALGRIND_PID
    while pgrep memcheck > /dev/null; do sleep 1; done
    # Clear the previously set trap, we don't need it anymore.
    trap - EXIT

    start_test No Valgrind complaints.
    check_not [ -s "$TEST_TMP/valgrind.log" ]
else
    check_simple "$NGINX_EXECUTABLE" -s quit -c "$PAGESPEED_CONF"
    while pgrep nginx > /dev/null; do sleep 1; done
fi

if [ "$AB_PID" != "0" ]; then
    echo "Kill ab (pid: $AB_PID)"
    killall -s KILL $AB_PID &>/dev/null || true
fi

start_test Logged output looks healthy.

# TODO(oschaaf): Sanity check for all the warnings/errors here.
OUT=$(cat "$ERROR_LOG" \
    | grep "\\[" \
    | grep -v "\\[debug\\]" \
    | grep -v "\\[info\\]" \
    | grep -v "\\[notice\\]" \
    | grep -v "\\[warn\\].*Cache Flush.*" \
    | grep -v "\\[warn\\].*doesnotexist.css.*" \
    | grep -v "\\[warn\\].*Invalid filter name: bogus.*" \
    | grep -v "\\[warn\\].*You seem to have downstream caching.*" \
    | grep -v "\\[warn\\].*Warning_trigger*" \
    | grep -v "\\[warn\\].*Rewrite http://www.google.com/mod_pagespeed_example/ failed*" \
    | grep -v "\\[warn\\].*A.bad:0:Resource*" \
    | grep -v "\\[warn\\].*W.bad.pagespeed.cf.hash.css*" \
    | grep -v "\\[warn\\].*BadName*" \
    | grep -v "\\[warn\\].*CSS parsing error*" \
    | grep -v "\\[warn\\].*Fetch failed for resource*" \
    | grep -v "\\[warn\\].*Rewrite.*example.pdf failed*" \
    | grep -v "\\[warn\\].*Rewrite.*hello.js failed*" \
    | grep -v "\\[warn\\].*Resource based on.*ngx_pagespeed_statistics.*" \
    | grep -v "\\[warn\\].*Canceling 1 functions on sequence Shutdown.*" \
    | grep -v "\\[warn\\].*using uninitialized.*" \
    | grep -v "\\[error\\].*BadName*" \
    | grep -v "\\[error\\].*/mod_pagespeed/bad*" \
    | grep -v "\\[error\\].*doesnotexist.css.*" \
    | grep -v "\\[error\\].*is forbidden.*" \
    | grep -v "\\[error\\].*access forbidden by rule.*" \
    | grep -v "\\[error\\].*forbidden.example.com*" \
    | grep -v "\\[error\\].*custom-paths.example.com*" \
    | grep -v "\\[error\\].*bogus_format*" \
    | grep -v "\\[error\\].*src/install/foo*" \
    | grep -v "\\[error\\].*recv() failed*" \
    | grep -v "\\[error\\].*send() failed*" \
    | grep -v "\\[error\\].*Invalid url requested: js_defer.js.*" \
    | grep -v "\\[error\\].*/mod_pagespeed_example/styles/yellow.css+blue.css.pagespeed.cc..css.*" \
    | grep -v "\\[error\\].*/mod_pagespeed_example/images/Puzzle.jpg.pagespeed.ce..jpg.*" \
    | grep -v "\\[error\\].*/pagespeed_custom_static/js_defer.js.*" \
    | grep -v "\\[error\\].*UH8L-zY4b4AAAAAAAAAA.*" \
    | grep -v "\\[error\\].*UH8L-zY4b4.*" \
    | grep -v "\\[error\\].*Serf status 111(Connection refused) polling.*" \
    | grep -v "\\[error\\].*Failed to make directory*" \
    | grep -v "\\[error\\].*Could not create directories*" \
    | grep -v "\\[error\\].*opening temp file: No such file or directory.*" \
    | grep -v "\\[error\\].*remote\.cfg.*" \
    | grep -v "\\[error\\].*Slow read operation on file.*" \
    | grep -v "\\[error\\].*Slow ReadFile operation on file.*" \
    | grep -v "\\[error\\].*Slow write operation on file.*" \
    | grep -v "\\[warn\\].*remote\.cfg.*" \
    | grep -v "\\[warn\\].*end token not received.*" \
    | grep -v "\\[warn\\].*failed to hook next event.*" \
    || true)

check [ -z "$OUT" ]

check_failures_and_exit
