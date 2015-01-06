#!/bin/bash
# Copyright 2010 Google Inc. All Rights Reserved.
# Author: abliss@google.com (Adam Bliss)
#
# Runs all Apache-specific and general system tests.
#
# See automatic/system_test_helpers.sh for usage.
#
# Expects APACHE_DEBUG_PAGESPEED_CONF to point to our config file, APACHE_LOG to
# the log file, and AUTOMATIC_SYSTEM_TEST to automatic/system_test.sh.
#
# CACHE_FLUSH_TEST=on can be passed to test our cache.flush behavior
# NO_VHOST_MERGE=on can be passed to tell tests to assume
# that ModPagespeedInheritVHostConfig has been turned off.

if [ -z $APACHE_DEBUG_PAGESPEED_CONF ]; then
  APACHE_DEBUG_PAGESPEED_CONF=/usr/local/apache2/conf/pagespeed.conf
fi

if [ -z $APACHE_LOG ]; then
  APACHE_LOG=/usr/local/apache2/logs/error_log
fi

if [ -z $APACHE_DOC_ROOT ]; then
  APACHE_DOC_ROOT=/usr/local/apache2/htdocs/
fi

PSA_JS_LIBRARY_URL_PREFIX="mod_pagespeed_static"
BEACON_HANDLER="mod_pagespeed_beacon"
HEADERS_FINALIZED=false

CACHE_FLUSH_TEST=${CACHE_FLUSH_TEST:-off}
NO_VHOST_MERGE=${NO_VHOST_MERGE:-off}
SUDO=${SUDO:-}
# TODO(jkarlin): Should we just use a vhost instead?  If so, remember to update
# all scripts that use TEST_PROXY_ORIGIN.
PAGESPEED_TEST_HOST=${PAGESPEED_TEST_HOST:-modpagespeed.com}

SERVER_NAME=apache

# Extract secondary hostname when set. Currently it's only set when doing the
# cache flush test, but it can be used in other tests we run in that run.
# Note that we use $1 not $HOSTNAME as that is only set up later by _helpers.sh.
if [ "$CACHE_FLUSH_TEST" = "on" ]; then
  # Replace any trailing :<port> with :<secondary-port>.
  SECONDARY_HOSTNAME=${1/%:*/:$APACHE_SECONDARY_PORT}
  if [ "$SECONDARY_HOSTNAME" = "$1" ]; then
    SECONDARY_HOSTNAME=${1}:$APACHE_SECONDARY_PORT
  fi

  # To fetch from the secondary test root, we must set
  # http_proxy=${SECONDARY_HOSTNAME} during fetches.
  SECONDARY_TEST_ROOT=http://secondary.example.com/mod_pagespeed_test
else
  # Force the variable to be set albeit blank so tests don't fail.
  : ${SECONDARY_HOSTNAME:=}
fi

# Inform system/system_tests.sh and the rest of this script whether statistics
# are enabled by grepping the conf file.
statistics_enabled="0"
statistics_logging_enabled="0"
if egrep -q "^    # ModPagespeedStatistics off$" \
    $APACHE_DEBUG_PAGESPEED_CONF; then
  statistics_enabled="1"
  echo STATS is ON
  if egrep -q "^ ModPagespeedStatisticsLogging on$" \
     $APACHE_DEBUG_PAGESPEED_CONF; then
    statistics_logging_enabled="1"
  fi
fi

# The 'PURGE' method is implemented, but not yet working in ngx_pagespeed, so
# have to indicate here that we want to test both PURGE and GET.  In
# nginx_system_test.sh we currently specify only GET.
#
# TODO(jmarantz) Once that's implemented in nginx, we can eliminate this
# setting.
CACHE_PURGE_METHODS="PURGE GET"

# Run General system tests.
#
# We need to know the directory this file is located in.  Unfortunately,
# if we're 'source'd from a script in a different directory $(dirname $0) gives
# us the directory that *that* script is located in
this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/../system/system_test.sh" || exit 1

# TODO(jefftk): most of these tests aren't Apache-specific and should be
# slightly generalized and moved to system/ where other implementations (like
# ngx_pagespeed) can use them.

# Grab a timestamp now so that we can check that logging works.
# Also determine where the log file is.
if [ $statistics_logging_enabled = "1" ]; then
  MOD_PAGESPEED_LOG_DIR="$(
    sed -n 's/^ ModPagespeedLogDir //p' $APACHE_DEBUG_PAGESPEED_CONF |
    sed -n 's/\"//gp')"
  # Wipe the logs so we get a clean start.
  rm $MOD_PAGESPEED_LOG_DIR/*
  # The specific log file that the console will use.
  # If per-vhost stats is enabled, this is the main vhost suffix ":0".
  # If per-vhost stats is not enabled, this is the global suffix "global".
  MOD_PAGESPEED_STATS_LOG="${MOD_PAGESPEED_LOG_DIR}/stats_log_:0"
  START_TIME=$(date +%s)000 # We need this in milliseconds.
  sleep 2; # Make sure we're around long enough to log stats.
fi

# General system tests

# Determine whether statistics are enabled or not.  If not, don't test them,
# but do an additional regression test that tries harder to get a cache miss.
if [ $statistics_enabled = "1" ]; then
  if [ "$SECONDARY_HOSTNAME" != "" ]; then
    function gunzip_grep_0ff() {
      gunzip - | fgrep -q "color:#00f"
      echo $?
    }

    start_test ipro with mod_deflate
    fetch_until -gzip $TEST_ROOT/ipro/mod_deflate/big.css gunzip_grep_0ff 0

    start_test ipro with reverse proxy of compressed content
    http_proxy=$SECONDARY_HOSTNAME \
        fetch_until -gzip http://ipro-proxy.example.com/big.css \
            gunzip_grep_0ff 0

    # Also test the .pagespeed. version, to make sure we didn't
    # accidentally gunzip stuff above when we shouldn't have.
    OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET -q -O - \
          http://ipro-proxy.example.com/A.big.css.pagespeed.cf.0.css)
    check_from "$OUT" fgrep -q "big{color:#00f}"
  fi
else
  start_test 404s are served.  Statistics are disabled so not checking them.
  OUT=$(check_not $WGET -O /dev/null $BAD_RESOURCE_URL 2>&1)
  check_from "$OUT" fgrep -q "404 Not Found"

  start_test 404s properly on uncached invalid resource.
  OUT=$(check_not $WGET -O /dev/null $BAD_RND_RESOURCE_URL 2>&1)
  check_from "$OUT" fgrep -q "404 Not Found"
fi

# Test that loopback route fetcher works with vhosts not listening on
# 127.0.0.1  Only run this during CACHE_FLUSH_TEST as that is when
# APACHE_TERTIARY_PORT is set.
if [ "${APACHE_TERTIARY_PORT:-}" != "" ]; then
  start_test IP choice for loopback fetches.
  HOST_NAME="loopbackfetch.example.com"
  URL="$HOST_NAME/mod_pagespeed_example/rewrite_images.html"
  http_proxy=127.0.0.2:$APACHE_TERTIARY_PORT \
    fetch_until $URL 'grep -c .pagespeed.ic' 2
fi
# Test /mod_pagespeed_message exists.
start_test Check if /mod_pagespeed_message page exists.
OUT=$($WGET --save-headers -q -O - $MESSAGE_URL | head -1)
check_200_http_response "$OUT"
# Note: There is a similar test in system_test.sh
#
# This tests whether fetching "/" gets you "/index.html".  With async
# rewriting, it is not deterministic whether inline css gets
# rewritten.  That's not what this is trying to test, so we use
# ?PageSpeed=off.
start_test directory is mapped to index.html.
rm -rf $OUTDIR
mkdir -p $OUTDIR
check $WGET -q "$EXAMPLE_ROOT/?PageSpeed=off" \
  -O $OUTDIR/mod_pagespeed_example
check $WGET -q "$EXAMPLE_ROOT/index.html?PageSpeed=off" -O $OUTDIR/index.html
check diff $OUTDIR/index.html $OUTDIR/mod_pagespeed_example

start_test Request Headers affect MPS options

# Get the special file response_headers.html and test the result.
# This file has Apache request_t headers_out and err_headers_out modified
# according to the query parameter.  The modification happens in
# instaweb_handler when the handler == kGenerateResponseWithOptionsHandler.
# Possible query flags include: headers_out, headers_errout, headers_override,
# and headers_combine.
function response_header_test() {
    query=$1
    mps_on=$2
    comments_removed=$3
    rm -rf $OUTDIR
    mkdir -p $OUTDIR

    # Get the file
    echo $WGET -q -S -O - \
      "$TEST_ROOT/response_headers.html?$query" '>&' $OUTDIR/header_out
    check $WGET -q -S -O - \
      "$TEST_ROOT/response_headers.html?$query" >& $OUTDIR/header_out

    # Make sure that any MPS option headers were stripped
    check_not grep -q ^PageSpeed: $OUTDIR/header_out
    check_not grep -q ^ModPagespeed: $OUTDIR/header_out

    # Verify if MPS is on or off
    if [ $mps_on = "no" ]; then
      # Verify that PageSpeed was off
      check_not fgrep -q 'X-Mod-Pagespeed:' $OUTDIR/header_out
      check_not fgrep -q '<script' $OUTDIR/header_out
    else
      # Verify that PageSpeed was on
      check fgrep -q 'X-Mod-Pagespeed:' $OUTDIR/header_out
      check fgrep -q '<script' $OUTDIR/header_out
    fi

    # Verify if comments were stripped
    if [ $comments_removed = "no" ]; then
      # Verify that comments were not removed
      check fgrep -q '<!--' $OUTDIR/header_out
    else
      # Verify that comments were removed
      check_not fgrep -q '<!--' $OUTDIR/header_out
    fi
}

# headers_out =     MPS: off
# err_headers_out =
response_header_test headers_out no no

# headers_out =
# err_headers_out = MPS: on
response_header_test headers_errout no no

# Note: The next two tests will break if remove_comments gets into the
# CoreFilter set.

# headers_out     = MPS: off, Filters: -remove_comments
# err_headers_out = MPS: on,  Filters: +remove_comments
# err_headers should is processed after headers_out, and so it should override
# but disabling a filter trumps enabling one. The overriding is described in
# the code for build_context_for_request.
response_header_test headers_override yes no

# headers_out     = MPS: on
# err_headers_out = Filters: +remove_comments
response_header_test headers_combine yes yes

start_test Respect X-Forwarded-Proto when told to
FETCHED=$OUTDIR/x_forwarded_proto
URL=$TEST_ROOT/?PageSpeedFilters=add_base_tag
check $WGET_DUMP -O $FETCHED --header="X-Forwarded-Proto: https" $URL
# When enabled, we respect X-Forwarded-Proto and thus list base as https.
check fgrep -q '<base href="https://' $FETCHED


# Individual filter tests, in alphabetical order

# This is dependent upon having a beacon handler.
test_filter add_instrumentation beacons load.
check run_wget_with_args $PRIMARY_SERVER/$BEACON_HANDLER?ets=load:13
check fgrep -q "204 No Content" $WGET_OUTPUT
check fgrep -q 'Cache-Control: max-age=0, no-cache' $WGET_OUTPUT

start_test Split HTML
SPLIT_HTML_ATF="$TEST_ROOT/split_html/split.html?x_split=atf"
wget -O - $URL $SPLIT_HTML_ATF > $FETCHED
check grep -q 'loadXMLDoc("1")' $FETCHED
check grep -q "/$PSA_JS_LIBRARY_URL_PREFIX/blink" $FETCHED
check grep -q '<!--GooglePanel begin panel-id.0--><!--GooglePanel end panel-id.0-->' $FETCHED
check grep -q -v 'pagespeed_lazy_src' $FETCHED

SPLIT_HTML_BTF=$TEST_ROOT"/split_html/split.html?x_split=atf"
wget -O - $URL $SPLIT_HTML_BTF > $FETCHED
check grep -q 'panel-id.0' $FETCHED
check grep -q 'pagespeed_lazy_src' $FETCHED

start_test mod_rewrite
check $WGET_DUMP $TEST_ROOT/redirect/php/ -O $OUTDIR/redirect_php.html
check \
  [ $(grep -ce "href=\"/mod_pagespeed_test/" $OUTDIR/redirect_php.html) = 2 ];

if [ "$SECONDARY_HOSTNAME" != "" ]; then
  if [ "$NO_VHOST_MERGE" = "on" ]; then
    start_test PageSpeed Unplugged and Off
    SPROXY="http://localhost:$APACHE_SECONDARY_PORT"
    VHOST_MPS_OFF="http://mpsoff.example.com"
    VHOST_MPS_UNPLUGGED="http://mpsunplugged.example.com"
    SITE="mod_pagespeed_example"
    ORIGINAL="$SITE/styles/yellow.css"
    FILTERED="$SITE/styles/A.yellow.css.pagespeed.cf.KM5K8SbHQL.css"

    # PageSpeed unplugged does not serve .pagespeed. resources.
    http_proxy=$SPROXY check_not $WGET -O /dev/null \
        $VHOST_MPS_UNPLUGGED/$FILTERED
    # PageSpeed off does serve .pagespeed. resources.
    http_proxy=$SPROXY check $WGET -O /dev/null $VHOST_MPS_OFF/$FILTERED

    # PageSpeed unplugged doesn't rewrite HTML, even when asked via query.
    OUT=$(http_proxy=$SPROXY check $WGET -S -O - \
    $VHOST_MPS_UNPLUGGED/$SITE/?PageSpeed=on 2>&1)
    check_not_from "$OUT" grep "X-Mod-Pagespeed:"
    # PageSpeed off does rewrite HTML if asked.
    OUT=$(http_proxy=$SPROXY check $WGET -S -O - \
    $VHOST_MPS_OFF/$SITE/?PageSpeed=on 2>&1)
    check_from "$OUT" grep "X-Mod-Pagespeed:"
  fi

  start_test MapProxyDomain for CDN setup
  # Test transitive ProxyMapDomain.  In this mode we have three hosts: cdn,
  # proxy, and origin.  Proxy runs MPS and fetches resources from origin,
  # optimizes them, and rewrites them to CDN for serving. The CDN is dumb and
  # has no caching so simply proxies all requests to proxy.  Origin serves out
  # images only.
  echo "Rewrite HTML with reference to proxyable image on CDN."
  PROXY_PM="http://proxy.pm.example.com"
  URL="$PROXY_PM/transitive_proxy.html"
  PDT_STATSDIR=$TESTTMP/stats
  rm -rf $PDT_STATSDIR
  mkdir -p $PDT_STATSDIR
  PDT_OLDSTATS=$PDT_STATSDIR/blocking_rewrite_stats.old
  PDT_NEWSTATS=$PDT_STATSDIR/blocking_rewrite_stats.new
  PDT_PROXY_STATS_URL=$PROXY_PM/mod_pagespeed_statistics?PageSpeed=off
  http_proxy=$SECONDARY_HOSTNAME \
    $WGET_DUMP $PDT_PROXY_STATS_URL > $PDT_OLDSTATS

  # The image should be proxied from origin, compressed, and rewritten to cdn.
  http_proxy=$SECONDARY_HOSTNAME \
    fetch_until -save -recursive $URL \
    'fgrep -c cdn.pm.example.com/external/xPuzzle.jpg.pagespeed.ic' 1
  check_file_size "$WGET_DIR/xPuzzle*" -lt 241260

  # Make sure that the file was only rewritten once.
  http_proxy=$SECONDARY_HOSTNAME \
    $WGET_DUMP $PDT_PROXY_STATS_URL > $PDT_NEWSTATS
  check_stat $PDT_OLDSTATS $PDT_NEWSTATS image_rewrites 1

  # The js should be fetched locally and inlined.
  http_proxy=$SECONDARY_HOSTNAME \
    fetch_until -save -recursive $URL 'fgrep -c document.write' 1

  # Save the image URL so we can try to reconstruct it later.
  PDT_IMG_URL=`egrep -o \"[^\"]*xPuzzle[^\"]*\.pagespeed[^\"]*\" $FETCH_FILE | \
    sed -e 's/\"//g'`

  # This function will be called after the cache is flushed to test
  # reconstruction.
  function map_proxy_domain_cdn_reconstruct() {
    rm -rf $PDT_STATSDIR
    mkdir -p $PDT_STATSDIR
    http_proxy=$SECONDARY_HOSTNAME \
      $WGET_DUMP $PDT_PROXY_STATS_URL > $PDT_OLDSTATS
    echo "Make sure we can reconstruct the image."
    # Fetch the url until it is less than its original size (i.e. compressed).
    http_proxy=$SECONDARY_HOSTNAME \
      fetch_until $PDT_IMG_URL "wc -c" 241260 "" "-lt"
    # Double check that we actually reconstructed.
    http_proxy=$SECONDARY_HOSTNAME \
      $WGET_DUMP $PDT_PROXY_STATS_URL > $PDT_NEWSTATS
    check_stat $PDT_OLDSTATS $PDT_NEWSTATS image_rewrites 1
  }
  on_cache_flush map_proxy_domain_cdn_reconstruct

  # See if mod_spdy is available
  SPDY_SUPPORT=$(http_proxy=$SECONDARY_HOSTNAME wget -q -O- spdy.example.com)
  if [ "$SPDY_SUPPORT" = "mod_spdy supported." ]; then
    echo "Testing fetching SSL with help of mod_spdy"
    # Now, try fetching a document from the spdyfetch.example.com vhost over SSL
    # Note that we cannot use the usual proxy trick, since it won't work with
    # SSL, but luckily we can still mess around with the Host: header and
    # X-Forwarded-Proto: headers directly and get what we want.
    # Even that, however, is not unconditional: wget >= 1.14 uses SNI, which
    # results in 400s if that doesn't match the Host:. Luckily, passing in
    # --secure-protocol=SSLv3 disables SNI.
    DATA=$(check wget -q -O - --no-check-certificate \
        --header="X-Forwarded-Proto: https" \
        --header="Host: spdyfetch.example.com"\
        --secure-protocol=SSLv3 \
        $HTTPS_EXAMPLE_ROOT/styles/A.blue.css.pagespeed.cf.0.css)
    check_from "$DATA" grep -q blue

    # Sanity-check that it fails for non-spdyfetch enabled code.
    echo "Sanity-check with mod_spdy fetch off"
    DATA=$(check_error_code 8 wget -q -O - --no-check-certificate \
        --header="X-Forwarded-Proto: https" \
        --header="Host: nospdyfetch.example.com"\
        $HTTPS_EXAMPLE_ROOT/styles/A.blue.css.pagespeed.cf.0.css)
  fi
fi

# TODO(sligocki): start_test MaxSegmentLength

if [ "$CACHE_FLUSH_TEST" = "on" ]; then
  start_test add_instrumentation has added unload handler with \
      ModPagespeedReportUnloadTime enabled in APACHE_SECONDARY_PORT.
  URL="$SECONDARY_TEST_ROOT/add_instrumentation.html\
?PageSpeedFilters=add_instrumentation"
  echo http_proxy=$SECONDARY_HOSTNAME $WGET -O $WGET_OUTPUT $URL
  http_proxy=$SECONDARY_HOSTNAME $WGET -O $WGET_OUTPUT $URL
  check [ $(grep -o "<script" $WGET_OUTPUT|wc -l) = 3 ]
  check [ $(grep -c "pagespeed.addInstrumentationInit('/$BEACON_HANDLER', 'beforeunload', '', 'http://secondary.example.com/mod_pagespeed_test/add_instrumentation.html');" $WGET_OUTPUT) = 1 ]
  check [ $(grep -c "pagespeed.addInstrumentationInit('/$BEACON_HANDLER', 'load', '', 'http://secondary.example.com/mod_pagespeed_test/add_instrumentation.html');" $WGET_OUTPUT) = 1 ]

  if [ "$NO_VHOST_MERGE" = "on" ]; then
    start_test When ModPagespeedMaxHtmlParseBytes is not set, we do not insert \
        a redirect.
    OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
      $SECONDARY_TEST_ROOT/large_file.html?PageSpeedFilters=)
    check_not_from "$OUT" fgrep -q "window.location="
    check_from "$OUT" fgrep -q "Lorem ipsum dolor sit amet"
  fi

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

  echo Clear out our existing state before we begin the test.
  echo $SUDO touch $MOD_PAGESPEED_CACHE/cache.flush
  $SUDO touch $MOD_PAGESPEED_CACHE/cache.flush
  echo $SUDO touch ${MOD_PAGESPEED_CACHE}_secondary/cache.flush
  $SUDO touch ${MOD_PAGESPEED_CACHE}_secondary/cache.flush
  echo $SUDO touch ${MOD_PAGESPEED_CACHE}_ipro_for_browser/cache.flush
  $SUDO touch ${MOD_PAGESPEED_CACHE}_ipro_for_browser/cache.flush
  sleep 1

  CACHE_TESTING_DIR="$APACHE_DOC_ROOT/mod_pagespeed_test/cache_flush/"
  CACHE_TESTING_TMPDIR="$CACHE_TESTING_DIR/$$"
  echo $SUDO mkdir "$CACHE_TESTING_TMPDIR"
  $SUDO mkdir "$CACHE_TESTING_TMPDIR"
  echo $SUDO cp "$CACHE_TESTING_DIR/cache_flush_test.html"\
                "$CACHE_TESTING_TMPDIR/"
  $SUDO cp "$CACHE_TESTING_DIR/cache_flush_test.html" "$CACHE_TESTING_TMPDIR/"
  CSS_FILE="$CACHE_TESTING_TMPDIR/update.css"
  URL_PATH=cache_flush/$$/cache_flush_test.html
  URL=$TEST_ROOT/$URL_PATH
  TMP_CSS_FILE=$TESTTMP/update.css

  # First, write color 0 into the css file and make sure it gets inlined into
  # the html.
  echo "echo \".class myclass { color: $COLOR0; }\" > $CSS_FILE"
  echo ".class myclass { color: $COLOR0; }" >$TMP_CSS_FILE
  chmod ugo+r $TMP_CSS_FILE  # in case the user's umask doesn't allow o+r
  $SUDO cp $TMP_CSS_FILE $CSS_FILE
  fetch_until $URL "grep -c $COLOR0" 1

  # Also do the same experiment using a different VirtualHost.  It points
  # to the same htdocs, but uses a separate cache directory.
  SECONDARY_URL=$SECONDARY_TEST_ROOT/$URL_PATH
  http_proxy=$SECONDARY_HOSTNAME fetch_until $SECONDARY_URL "grep -c $COLOR0" 1

  # Track how many flushes were noticed by Apache processes up till
  # this point in time.  Note that each Apache process/vhost
  # separately detects the 'flush'.
  NUM_INITIAL_FLUSHES=$(scrape_stat cache_flush_count)

  # Now change the file to $COLOR1.
  echo echo ".class myclass { color: $COLOR1; }" ">" $CSS_FILE
  echo ".class myclass { color: $COLOR1; }" >$TMP_CSS_FILE
  $SUDO cp $TMP_CSS_FILE $CSS_FILE

  # We might have stale cache for 5 seconds, so the result might stay
  # $COLOR0, but we can't really test for that since the child process
  # handling this request might not have it in cache.
  # fetch_until $URL 'grep -c $COLOR0' 1

  # Flush the cache by touching a special file in the cache directory.  Now
  # css gets re-read and we get $COLOR1 in the output.  Sleep here to avoid
  # a race due to 1-second granularity of file-system timestamp checks.  For
  # the test to pass we need to see time pass from the previous 'touch'.
  sleep 2
  echo $SUDO touch $MOD_PAGESPEED_CACHE/cache.flush
  $SUDO touch $MOD_PAGESPEED_CACHE/cache.flush
  sleep 1
  fetch_until $URL "grep -c $COLOR1" 1

  # TODO(jmarantz): we can change this test to be more exacting now, since
  # to address Issue 568, we should only get one cache-flush bump every time
  # we touch the file.
  if [ $statistics_enabled -ne 0 ]; then
    NUM_FLUSHES=$(scrape_stat cache_flush_count)
    NUM_NEW_FLUSHES=$(expr $NUM_FLUSHES - $NUM_INITIAL_FLUSHES)
    echo NUM_NEW_FLUSHES = $NUM_FLUSHES - \
      $NUM_INITIAL_FLUSHES = $NUM_NEW_FLUSHES
    check [ $NUM_NEW_FLUSHES -ge 1 ]
    check [ $NUM_NEW_FLUSHES -lt 20 ]
  fi

  # However, the secondary cache might not have seen this cache-flush, but
  # due to the multiple child processes, each of which does polling separately,
  # we cannot guarantee it.  I think if we knew we were running a 'worker' mpm
  # with just 1 child process we could do this test.
  # fetch_until $SECONDARY_URL 'grep -c blue' 1

  # Now flush the secondary cache too so it can see the change to $COLOR1.
  echo $SUDO touch ${MOD_PAGESPEED_CACHE}_secondary/cache.flush
  $SUDO touch ${MOD_PAGESPEED_CACHE}_secondary/cache.flush
  sleep 1
  http_proxy=$SECONDARY_HOSTNAME fetch_until $SECONDARY_URL "grep -c $COLOR1" 1

  # Clean up so we don't leave behind a stray file not under source control.
  echo $SUDO rm -f $CACHE_TESTING_TMPDIR
  $SUDO rm -rf "$CACHE_TESTING_TMPDIR"
  rm -f $TMP_CSS_FILE

  # connection_refused.html references modpagespeed.com:1023/someimage.png.
  # mod_pagespeed will attempt to connect to that host and port to fetch the
  # input resource using serf.  We expect the connection to be refused.  Relies
  # on "ModPagespeedDomain modpagespeed.com:1023" in debug.conf.template.  Also
  # relies on running after a cache-flush to avoid bypassing the serf fetch,
  # since mod_pagespeed remembers fetch-failures in its cache for 5 minutes.
  # Because of the empty cache requirement, we conditionalize it on a single
  # value of NO_VHOST_MERGE, so it runs only once per apache_debug_smoke_test
  if [ \( $NO_VHOST_MERGE = "on" \) -a \( "${VIRTUALBOX_TEST:-}" = "" \) ]
  then
    start_test Connection refused handling

    # Monitor the Apache log starting now.  tail -F will catch log rotations.
    SERF_REFUSED_PATH=$TESTTMP/instaweb_apache_serf_refused
    rm -f $SERF_REFUSED_PATH
    echo APACHE_LOG = $APACHE_LOG
    tail --sleep-interval=0.1 -F $APACHE_LOG > $SERF_REFUSED_PATH &
    TAIL_PID=$!

    # Wait for tail to start.
    echo -n "Waiting for tail to start..."
    while [ ! -s $SERF_REFUSED_PATH ]; do
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
    for i in {1..100}; do
      ERRS=$(grep -c "Serf status 111" $SERF_REFUSED_PATH || true)
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
    # Make sure we have the URL detail we expect because
    # ModPagespeedListOutstandingUrlsOnError is on in debug.conf.template.
    echo Check that ModPagespeedSerfListOutstandingUrlsOnError works
    check grep "URL http://modpagespeed.com:1023/someimage.png active for " \
        $SERF_REFUSED_PATH
  fi

  if [ "$NO_VHOST_MERGE" = "on" ]; then
    # Likewise, blocking rewrite tests are only run once.
    start_test Blocking rewrite enabled.
    # We assume that blocking_rewrite_test_dont_reuse_1.jpg will not be
    # rewritten on the first request since it takes significantly more time to
    # rewrite than the rewrite deadline and it is not already accessed by
    # another request earlier.
    BLOCKING_REWRITE_URL="$TEST_ROOT/blocking_rewrite.html?\
PageSpeedFilters=rewrite_images"
    OUTFILE=$OUTDIR/blocking_rewrite.out.html
    OLDSTATS=$OUTDIR/blocking_rewrite_stats.old
    NEWSTATS=$OUTDIR/blocking_rewrite_stats.new
    $WGET_DUMP $STATISTICS_URL > $OLDSTATS
    check $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest'\
      $BLOCKING_REWRITE_URL -O $OUTFILE
    $WGET_DUMP $STATISTICS_URL > $NEWSTATS
    check_stat $OLDSTATS $NEWSTATS image_rewrites 1
    check_stat $OLDSTATS $NEWSTATS cache_hits 0
    check_stat $OLDSTATS $NEWSTATS cache_misses 2
    # 2 cache inserts for image + 1 for HTML in IPRO flow.
    # Note: If we tune IPRO to exclude results for HTML, this will go back to 2.
    check_stat $OLDSTATS $NEWSTATS cache_inserts 3
    # TODO(sligocki): There is no stat num_rewrites_executed. Fix.
    #check_stat $OLDSTATS $NEWSTATS num_rewrites_executed 1

    start_test Blocking rewrite enabled using wrong key.
    BLOCKING_REWRITE_URL="$SECONDARY_TEST_ROOT/\
blocking_rewrite_another.html?PageSpeedFilters=rewrite_images"
    OUTFILE=$OUTDIR/blocking_rewrite.out.html
    http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: junk' \
      $BLOCKING_REWRITE_URL > $OUTFILE
    check [ $(grep -c "[.]pagespeed[.]" $OUTFILE) -lt 1 ]

    http_proxy=$SECONDARY_HOSTNAME fetch_until $BLOCKING_REWRITE_URL 'grep -c [.]pagespeed[.]' 1
  fi

  run_post_cache_flush
fi

start_test Send custom fetch headers on resource re-fetches.
PLAIN_HEADER="header=value"
X_OTHER_HEADER="x-other=False"

URL="$PRIMARY_SERVER/mod_pagespeed_log_request_headers.js.pagespeed.jm.0.js"
WGET_OUT=$($WGET_DUMP $URL)
check_from "$WGET_OUT" grep "$PLAIN_HEADER"
check_from "$WGET_OUT" grep "$X_OTHER_HEADER"

start_test Send custom fetch headers on resource subfetches.
URL=$TEST_ROOT/custom_fetch_headers.html?PageSpeedFilters=inline_javascript
fetch_until -save $URL 'grep -c header=value' 1
check_from "$(cat $FETCH_FILE)" grep "$X_OTHER_HEADER"

# Check that statistics logging was functional during these tests
# if it was enabled.
if [ $statistics_logging_enabled = "1" ]; then
  start_test Statistics logging works.
  check ls $MOD_PAGESPEED_STATS_LOG
  check [ $(grep "timestamp: " $MOD_PAGESPEED_STATS_LOG | wc -l) -ge 1 ]
  # An array of all the timestamps that were logged.
  TIMESTAMPS=($(sed -n '/timestamp: /s/[^0-9]*//gp' $MOD_PAGESPEED_STATS_LOG))
  check [ ${#TIMESTAMPS[@]} -ge 1 ]
  for T in ${TIMESTAMPS[@]}; do
    check [ $T -ge $START_TIME ]
  done
  # Check a few arbitrary statistics to make sure logging is taking place.
  check [ $(grep "num_flushes: " $MOD_PAGESPEED_STATS_LOG | wc -l) -ge 1 ]
  # We are not outputing histograms.
  check [ $(grep "histogram#" $MOD_PAGESPEED_STATS_LOG | wc -l) -eq 0 ]
  check [ $(grep "image_ongoing_rewrites: " $MOD_PAGESPEED_STATS_LOG | wc -l) \
    -ge 1 ]

  start_test Statistics logging JSON handler works.
  JSON=$OUTDIR/console_json.json
  STATS_JSON_URL="$CONSOLE_URL?json&granularity=0&var_titles=num_\
flushes,image_ongoing_rewrites"
  echo "$WGET_DUMP $STATS_JSON_URL > $JSON"
  $WGET_DUMP $STATS_JSON_URL > $JSON
  # Each variable we ask for should show up once.
  check [ $(grep "\"num_flushes\": " $JSON | wc -l) -eq 1 ]
  check [ $(grep "\"image_ongoing_rewrites\": " $JSON | wc -l) -eq 1 ]
  check [ $(grep "\"timestamps\": " $JSON | wc -l) -eq 1 ]
  # An array of all the timestamps that the JSON handler returned.
  JSON_TIMESTAMPS=($(sed -rn 's/^\{"timestamps": \[(([0-9]+, )*[0-9]*)\].*}$/\1/;/^[0-9]+/s/,//gp' $JSON))
  # Check that we see the same timestamps that are in TIMESTAMPS.
  # We might have generated extra timestamps in the time between TIMESTAMPS
  # and JSON_TIMESTAMPS, so only loop through TIMESTAMPS.
  check [ ${#JSON_TIMESTAMPS[@]} -ge ${#TIMESTAMPS[@]} ]
  t=0
  while [ $t -lt ${#TIMESTAMPS[@]} ]; do
    check [ ${TIMESTAMPS[$t]} -eq ${JSON_TIMESTAMPS[$t]} ]
    t=$(($t+1))
  done


  start_test Statistics console is available.
  CONSOLE_URL=$PRIMARY_SERVER/pagespeed_console
  CONSOLE_HTML=$OUTDIR/console.html
  $WGET_DUMP $CONSOLE_URL > $CONSOLE_HTML
  check grep -q "console" $CONSOLE_HTML
fi

start_test If parsing
# $STATISTICS_URL ends in ?ModPagespeed=off, so we need & for now.
# If we remove the query from $STATISTICS_URL, s/&/?/.
readonly CONFIG_URL="$STATISTICS_URL&config"
readonly SPDY_CONFIG_URL="$STATISTICS_URL&spdy_config"

echo $WGET_DUMP $CONFIG_URL
CONFIG=$($WGET_DUMP $CONFIG_URL)
spdy_config_title="<title>PageSpeed SPDY Configuration</title>"
config_title="<title>PageSpeed Configuration</title>"
check_from "$CONFIG" fgrep -q "$config_title"
check_not_from "$CONFIG" fgrep -q "$spdy_config_title"
# Regular config should have a shard line:
check_from "$CONFIG" egrep -q "http://nonspdy.example.com/ Auth Shards:{http:"
check_from "$CONFIG" egrep -q "//s1.example.com/, http://s2.example.com/}"
# And "combine CSS" on.
check_from "$CONFIG" egrep -q "Combine Css"

echo $WGET_DUMP $SPDY_CONFIG_URL
SPDY_CONFIG=$($WGET_DUMP $SPDY_CONFIG_URL)
check_not_from "$SPDY_CONFIG" fgrep -q "$config_title"
check_from "$SPDY_CONFIG" fgrep -q "$spdy_config_title"

# SPDY config should have neither shards, nor combine CSS.
check_not_from "$SPDY_CONFIG" egrep -q "http://nonspdy.example.com"
check_not_from "$SPDY_CONFIG" egrep -q "s1.example.com"
check_not_from "$SPDY_CONFIG" egrep -q "s2.example.com"
check_not_from "$SPDY_CONFIG" egrep -q "Combine Css"

# Test ForbidAllDisabledFilters, which is set in config for
# /mod_pagespeed_test/forbid_all_disabled/disabled/ where we've disabled
# remove_quotes, remove_comments, and collapse_whitespace (which are enabled
# for its parent directory). We fetch 3 x 3 times, the first 3
# being for forbid_all_disabled, fordid_all_disabled/disabled, and
# forbid_all_disabled/disabled/cheat, to ensure that a subdirectory cannot
# circumvent the forbidden flag; and the second 3 being a normal fetch, a
# fetch using a query parameter to try to enable the forbidden filters, and a
# fetch using a request header to try to enable the forbidden filters.
function test_forbid_all_disabled() {
  QUERYP="$1"
  HEADER="$2"
  if [ -n "$QUERYP" ]; then
    INLINE_CSS=",-inline_css"
  else
    INLINE_CSS="?PageSpeedFilters=-inline_css"
  fi
  WGET_ARGS="--header=X-PSA-Blocking-Rewrite:psatest"
  URL1=$TEST_ROOT/forbid_all_disabled/forbidden.html
  URL2=$TEST_ROOT/forbid_all_disabled/disabled/forbidden.html
  URL3=$TEST_ROOT/forbid_all_disabled/disabled/cheat/forbidden.html
  OUTFILE="$TESTTMP/test_forbid_all_disabled"
  # Fetch testing that forbidden filters stay disabled.
  echo $WGET $HEADER $URL1$QUERYP$INLINE_CSS
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL1$QUERYP$INLINE_CSS
  check     egrep -q '<link rel=stylesheet'  $OUTFILE
  check_not egrep -q '<!--'                  $OUTFILE
  check     egrep -q '^<li>'                 $OUTFILE
  echo $WGET $HEADER $URL2$QUERYP$INLINE_CSS
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL2$QUERYP$INLINE_CSS
  check     egrep -q '<link rel="stylesheet' $OUTFILE
  check     egrep -q '<!--'                  $OUTFILE
  check     egrep -q '    <li>'              $OUTFILE
  echo $WGET $HEADER $URL3$QUERYP$INLINE_CSS
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL3$QUERYP$INLINE_CSS
  check     egrep -q '<link rel="stylesheet' $OUTFILE
  check     egrep -q '<!--'                  $OUTFILE
  check     egrep -q '    <li>'              $OUTFILE
  # Fetch testing that enabling inline_css for disabled/ directory works.
  echo $WGET $HEADER $URL1
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL1
  check_not egrep -q '<style>.yellow'        $OUTFILE
  echo $WGET $HEADER $URL2
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL2
  check     egrep -q '<style>.yellow'        $OUTFILE
  echo $WGET $HEADER $URL3
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL3
  check     egrep -q '<style>.yellow'        $OUTFILE
  rm -f $OUTFILE
  WGET_ARGS=""
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

# Now check stuff on secondary host. The results will depend on whether
# ModPagespeedInheritVHostConfig is on or off. We run this only for some tests,
# since we don't always have the secondary port number available here.
if [ "$SECONDARY_HOSTNAME" != "" ]; then
  SECONDARY_STATS_URL=http://$SECONDARY_HOSTNAME/mod_pagespeed_statistics
  SECONDARY_CONFIG_URL=$SECONDARY_STATS_URL?config
  SECONDARY_SPDY_CONFIG_URL=$SECONDARY_STATS_URL?spdy_config

  if [ "$NO_VHOST_MERGE" = "on" ]; then
    start_test Config with VHost inheritance off
    echo $WGET_DUMP $SECONDARY_CONFIG_URL
    SECONDARY_CONFIG=$($WGET_DUMP $SECONDARY_CONFIG_URL)
    check_from "$SECONDARY_CONFIG" fgrep -q "$config_title"
    check_not_from "$SECONDARY_CONFIG" fgrep -q "$spdy_config_title"
    # No inherit, no sharding.
    check_not_from "$SECONDARY_CONFIG" egrep -q "http://nonspdy.example.com/"

    # Should not inherit the blocking rewrite key.
    check_not_from "$SECONDARY_CONFIG" egrep -q "blrw"

    echo $WGET_DUMP $SECONDARY_SPDY_CONFIG_URL
    SECONDARY_SPDY_CONFIG=$($WGET_DUMP $SECONDARY_SPDY_CONFIG_URL)
    check_not_from "$SECONDARY_SPDY_CONFIG" fgrep -q "$config_title"
    check_from "$SECONDARY_SPDY_CONFIG" \
      egrep -q "SPDY-specific configuration missing"
  else
    start_test Config with VHost inheritance on
    echo $WGET_DUMP $SECONDARY_CONFIG_URL
    SECONDARY_CONFIG=$($WGET_DUMP $SECONDARY_CONFIG_URL)
    check_from "$SECONDARY_CONFIG" fgrep -q "$config_title"
    check_not_from "$SECONDARY_CONFIG" fgrep -q "$spdy_config_title"
    # Sharding is applied in this host, thanks to global inherit flag.
    check_from "$SECONDARY_CONFIG" egrep -q "http://nonspdy.example.com/"

    # We should also inherit the blocking rewrite key.
    check_from "$SECONDARY_CONFIG" egrep -q "\(blrw\)[[:space:]]+psatest"

    echo $WGET_DUMP $SECONDARY_SPDY_CONFIG_URL
    SECONDARY_SPDY_CONFIG=$($WGET_DUMP $SECONDARY_SPDY_CONFIG_URL)
    check_not_from "$SECONDARY_SPDY_CONFIG" fgrep -q "$config_title"
    check_from "$SECONDARY_SPDY_CONFIG" fgrep -q "$spdy_config_title"
    # Disabling of combine CSS should get inherited.
    check_not_from "$SECONDARY_SPDY_CONFIG" egrep -q "Combine Css"
  fi

  if [ -n "$APACHE_LOG" ]; then
    start_test Encoded absolute urls are not respected
    HOST_NAME="http://absolute-urls.example.com"

    # Monitor the Apache log; tail -F will catch log rotations.
    ABSOLUTE_URLS_LOG_PATH=$TESTTMP/instaweb_apache_absolute_urls.log
    echo APACHE_LOG = $APACHE_LOG
    tail --sleep-interval=0.1 -F $APACHE_LOG > $ABSOLUTE_URLS_LOG_PATH &
    TAIL_PID=$!

    # should fail; the example.com isn't us.
    http_proxy=$SECONDARY_HOSTNAME check_not $WGET_DUMP \
        "$HOST_NAME/,hexample.com.pagespeed.jm.0.js"

    REJECTED="Rejected absolute url reference"

    # Wait up to 10 seconds for failure.
    for i in {1..100}; do
      REJECTIONS=$(fgrep -c "$REJECTED" $ABSOLUTE_URLS_LOG_PATH || true)
      if [ $REJECTIONS -ge 1 ]; then
        break;
      fi;
      /bin/echo -n "."
      sleep 0.1
    done;
    /bin/echo "."

    # Kill the log monitor silently.
    kill $TAIL_PID
    wait $TAIL_PID 2> /dev/null || true

    check [ $REJECTIONS -eq 1 ]
  fi

  start_test Pass through headers when Cache-Control is set early on HTML.
  http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
      http://issue809.example.com/mod_pagespeed_example/index.html \
      -O $TESTTMP/issue809.http
  check_from "$(extract_headers $TESTTMP/issue809.http)" \
      grep -q "Issue809: Issue809Value"

  start_test Pass through common headers from origin on combined resources.
  URL="http://issue809.example.com/mod_pagespeed_example/combine_css.html"
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
      "grep -c css.pagespeed.cc." 1

  # Extract out the rewritten CSS URL from the HTML saved by fetch_until
  # above (see -save and definition of fetch_until).  Fetch that CSS
  # file and look inside for the sprited image reference (ic.pagespeed.is...).
  CSS=$(grep stylesheet "$FETCH_UNTIL_OUTFILE" | cut -d\" -f 6)
  if [ ${CSS:0:7} != "http://" ]; then
    CSS="http://issue809.example.com/mod_pagespeed_example/$CSS"
  fi
  http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $CSS -O $TESTTMP/combined.http
  check_from "$(extract_headers $TESTTMP/combined.http)" \
      grep -q "Issue809: Issue809Value"

  start_test Base config has purging disabled.  Check error message syntax.
  OUT=$($WGET_DUMP "$HOSTNAME/pagespeed_admin/cache?purge=*")
  check_from "$OUT" fgrep -q "ModPagespeedEnableCachePurge on"

  start_test mobilizer with inlined XHR-helper and other JS compiled.
  MOB_SUFFIX_RE="\\.\\w+\\.js"
  URL="http://${PAGESPEED_TEST_HOST}.suffix.net/mod_pagespeed_example/index.html"
  # We use fetch_until because we only inline the XHR file once it's
  # in cache.
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
    'fgrep -c window.XMLHttpRequest=' 1
  OUT=$(grep script $FETCH_UNTIL_OUTFILE)
  check_from "$OUT" egrep -q "pagespeed_static/mobilize$MOB_SUFFIX_RE"
  check_not_from "$OUT" egrep -q "pagespeed_static/mobilize_xhr_opt$MOB_SUFFIX_RE"
  check_not_from "$OUT" fgrep -q mobilize_xhr.js
  check_not_from "$OUT" fgrep -q mobilize_layout.js
  check_not_from "$OUT" fgrep -q mobilize_util.js
  check_not_from "$OUT" fgrep -q mobilize_nav.js

  start_test mobilizer with MobStatic on
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
    "$URL?PageSpeedMobStatic=on" | grep script)
  check_not_from "$OUT" egrep -q \
    "pagespeed_static/mobilize_xhr_debug$MOB_SUFFIX_RE"
  check_not_from "$OUT" egrep -q "pagespeed_static/mobilize_debug$MOB_SUFFIX_RE"
  check_from "$OUT" fgrep -q mobilize_xhr.js
  check_from "$OUT" fgrep -q mobilize_layout.js
  check_from "$OUT" fgrep -q mobilize_util.js
  check_from "$OUT" fgrep -q mobilize_nav.js

  start_test mobilizer with debug on
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
    "$URL?PageSpeedFilters=+debug" | grep script)
  # We don't inline the XHR js in debug mode because it will be too large.
  check_from "$OUT" egrep -q "pagespeed_static/mobilize_xhr_debug$MOB_SUFFIX_RE"
  check_from "$OUT" egrep -q "pagespeed_static/mobilize_debug$MOB_SUFFIX_RE"
  check_not_from "$OUT" fgrep -q mobilize_xhr.js
  check_not_from "$OUT" fgrep -q mobilize_layout.js
  check_not_from "$OUT" fgrep -q mobilize_util.js
  check_not_from "$OUT" fgrep -q mobilize_nav.js

  start_test no mobilization files if we turn mobilization off
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
    --header 'X-PSA-Blocking-Rewrite: psatest' \
    "$URL?PageSpeedFilters=-mobilize")
  check_not_from "$OUT" fgrep -q window.XMLHttpRequest
  check_not_from "$OUT" egrep -q "pagespeed_static/mobilize$MOB_SUFFIX_RE"
  check_not_from "$OUT" egrep -q "pagespeed_static/mobilize_xhr_opt$MOB_SUFFIX_RE"
  check_not_from "$OUT" fgrep -q mobilize_xhr.js
  check_not_from "$OUT" fgrep -q mobilize_layout.js
  check_not_from "$OUT" fgrep -q mobilize_util.js
  check_not_from "$OUT" fgrep -q mobilize_nav.js

fi

start_test Issue 609 -- proxying non-.pagespeed content, and caching it locally
URL="$PRIMARY_SERVER/modpagespeed_http/not_really_a_font.woff"
echo $WGET_DUMP $URL ....
OUT1=$($WGET_DUMP $URL)
check_from "$OUT1" egrep -q "This is not really font data"
if [ $statistics_enabled = "1" ]; then
  OLDSTATS=$OUTDIR/proxy_fetch_stats.old
  NEWSTATS=$OUTDIR/proxy_fetch_stats.new
  $WGET_DUMP $STATISTICS_URL > $OLDSTATS
fi
OUT2=$($WGET_DUMP $URL)
check_from "$OUT2" egrep -q "This is not really font data"
if [ $statistics_enabled = "1" ]; then
  $WGET_DUMP $STATISTICS_URL > $NEWSTATS
  check_stat $OLDSTATS $NEWSTATS cache_hits 1
  check_stat $OLDSTATS $NEWSTATS cache_misses 0
fi

start_test Do not proxy content without a Content-Type header
# These tests depend on modpagespeed.com being configured to serve an example
# file with a content-type header on port 8091 and without one on port 8092.
# scripts/serve_proxying_tests.sh can do this.
URL="$PRIMARY_SERVER/content_type_absent/"
CONTENTS="This file should not be proxied"


OUT=$($CURL --include --silent $URL)
check_from "$OUT" fgrep -q "403 Forbidden"
check_from "$OUT" fgrep -q \
    "Missing Content-Type required for proxied resource"
check_not_from "$OUT" fgrep -q "$CONTENTS"

start_test But do proxy content if the Content-Type header is present.
URL="$PRIMARY_SERVER/content_type_present/"
CONTENTS="This file should be proxied"


OUT=$($CURL --include --silent $URL)
check_from "$OUT" fgrep -q "200 OK"
check_from "$OUT" fgrep -q "$CONTENTS"

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
check_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" fgrep -q 'Etag: W/"PSA-aj-'

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
check_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" fgrep -q 'Etag: W/"PSA-aj-'

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

function scrape_secondary_stat {
  http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
    http://secondary.example.com/mod_pagespeed_statistics/ | \
    scrape_pipe_stat "$1"
}

if [ $statistics_enabled = "1" ]; then
  start_test CompressedCache is racking up savings on the root vhost
  original_size=$(scrape_stat compressed_cache_original_size)
  compressed_size=$(scrape_stat compressed_cache_compressed_size)
  echo original_size=$original_size compressed_size=$compressed_size
  check [ "$compressed_size" -lt "$original_size" ];
  check [ "$compressed_size" -gt 0 ];
  check [ "$original_size" -gt 0 ];

  if [ "$SECONDARY_HOSTNAME" != "" ]; then
    start_test CompressedCache is turned off for the secondary vhost
    original_size=$(scrape_secondary_stat compressed_cache_original_size)
    compressed_size=$(scrape_secondary_stat compressed_cache_compressed_size)
    check [ "$compressed_size" -eq 0 ];
    check [ "$original_size" -eq 0 ];
  fi
else
  echo skipping CompressedCache test because stats is $statistics_enabled
fi

# Check all the pagespeed_admin pages, both in its default location and an
# alternate.
start_test pagespeed_admin and alternate_admin_path
function check_admin_banner() {
  path="$1"
  title="$2"
  tmpfile=$TESTTMP/admin.html
  echo $WGET_DUMP $PRIMARY_SERVER/$path '>' $tmpfile ...
  $WGET_DUMP $PRIMARY_SERVER/$path > $tmpfile
  check fgrep -q "<title>PageSpeed $title</title>" $tmpfile
  rm -f $tmpfile
}
for admin_path in pagespeed_admin pagespeed_global_admin alt/admin/path; do
  check_admin_banner $admin_path/statistics "Statistics"
  check_admin_banner $admin_path/config "Configuration"
  check_admin_banner $admin_path/spdy_config "SPDY Configuration"
  check_admin_banner $admin_path/histograms "Histograms"
  check_admin_banner $admin_path/cache "Caches"
  check_admin_banner $admin_path/console "Console"
  check_admin_banner $admin_path/message_history "Message History"
done


# TODO(matterbury): Uncomment these lines when the test is fixed.
:<< COMMENTING_BLOCK
start_test ModPagespeedIf application
# Without SPDY, we should combine things
OUT=$($WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' \
    $EXAMPLE_ROOT/combine_css.html)
check_from "$OUT" egrep -q ',Mcc'

# Despite combine_css being disabled in <ModPagespeedIf>, we still
# expect it with SPDY since it's turned on in mod_pagespeed_example/.htaccess.
# However, since rewrite_css is off, the result should be rewritten by
# cc and not also cf or ce.
OUT=$($WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' \
  --header 'X-PSA-Optimize-For-SPDY: true' \
  $EXAMPLE_ROOT/combine_css.html)
check_not_from "$OUT" egrep -q ',Mcc'
check_from "$OUT" egrep -q '.pagespeed.cc'

# Now test resource fetch. Since we've disabled extend_cache and
# rewrite_images for spdy, we should not see rewritten resources there,
# while we will in the other normal case.
OUT=$($WGET_DUMP  --header 'X-PSA-Blocking-Rewrite: psatest' \
    $EXAMPLE_ROOT/styles/A.rewrite_css_images.css.pagespeed.cf.rnLTdExmOm.css)
check_from "$OUT" grep -q 'png.pagespeed.'

OUT=$($WGET_DUMP  --header 'X-PSA-Blocking-Rewrite: psatest' \
    --header 'X-PSA-Optimize-For-SPDY: true' \
    $EXAMPLE_ROOT/styles/A.rewrite_css_images.css.pagespeed.cf.rnLTdExmOm.css)
check_not_from "$OUT" grep -q 'png.pagespeed.'
COMMENTING_BLOCK

# Cleanup
rm -rf $OUTDIR

check_failures_and_exit
